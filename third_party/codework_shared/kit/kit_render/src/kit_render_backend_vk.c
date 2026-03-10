#include "kit_render_backend.h"

#include <stdint.h>
#include <string.h>

#if KIT_RENDER_ENABLE_VK_BACKEND
#include <ctype.h>
#include <sys/stat.h>
#include "vk_renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif

typedef struct KitRenderVkBackendState {
    void *renderer;
    int owns_renderer;
    KitRenderExternalReleaseFn release_fn;
    void *release_user;
    void *frame_command_buffer;
    int frame_active;
#if KIT_RENDER_ENABLE_VK_BACKEND
    TTF_Font *ttf_font_cache[CORE_FONT_ROLE_COUNT][CORE_FONT_TEXT_SIZE_COUNT];
    uint8_t ttf_font_failed[CORE_FONT_ROLE_COUNT][CORE_FONT_TEXT_SIZE_COUNT];
    CoreFontPresetId ttf_cache_preset_id;
    int ttf_cache_initialized;
    int ttf_runtime_ready;
    int ttf_runtime_failed;
#endif
} KitRenderVkBackendState;

static CoreResult vk_backend_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static KitRenderVkBackendState *vk_backend_state(const KitRenderContext *ctx) {
    if (!ctx || !ctx->backend_state) {
        return 0;
    }
    return (KitRenderVkBackendState *)ctx->backend_state;
}

#if KIT_RENDER_ENABLE_VK_BACKEND
static void vk_backend_clear_font_cache(KitRenderVkBackendState *state);
#endif

static CoreResult vk_backend_init(KitRenderContext *ctx) {
    KitRenderVkBackendState *state;

    if (!ctx) {
        return vk_backend_invalid("null context");
    }

    state = (KitRenderVkBackendState *)core_calloc(1u, sizeof(*state));
    if (!state) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    ctx->backend_state = state;
    return core_result_ok();
}

static void vk_backend_shutdown(KitRenderContext *ctx) {
    KitRenderVkBackendState *state;

    if (!ctx || !ctx->backend_state) {
        return;
    }
    state = (KitRenderVkBackendState *)ctx->backend_state;
#if KIT_RENDER_ENABLE_VK_BACKEND
    vk_backend_clear_font_cache(state);
#endif
    if (state->renderer && state->owns_renderer && state->release_fn) {
        state->release_fn(state->renderer, state->release_user);
    }
    core_free(ctx->backend_state);
    ctx->backend_state = 0;
}

static CoreResult vk_backend_attach_external(KitRenderContext *ctx,
                                             void *backend_handle,
                                             int take_ownership,
                                             KitRenderExternalReleaseFn release_fn,
                                             void *release_user) {
    KitRenderVkBackendState *state = vk_backend_state(ctx);

    if (!state) {
        return vk_backend_invalid("vulkan backend not initialized");
    }
    if (!backend_handle) {
        return vk_backend_invalid("null external backend handle");
    }
    if (state->renderer) {
        return vk_backend_invalid("backend handle already attached");
    }
    if (take_ownership && !release_fn) {
        return vk_backend_invalid("owned backend requires release callback");
    }

    state->renderer = backend_handle;
    state->owns_renderer = take_ownership ? 1 : 0;
    state->release_fn = release_fn;
    state->release_user = release_user;
    return core_result_ok();
}

static CoreResult vk_backend_begin_frame(KitRenderContext *ctx, KitRenderFrame *frame) {
    KitRenderVkBackendState *state = vk_backend_state(ctx);

    if (!state) {
        return vk_backend_invalid("vulkan backend not initialized");
    }
    if (!frame || !frame->command_buffer) {
        return vk_backend_invalid("invalid frame");
    }
    if (!state->renderer) {
        return vk_backend_invalid("no attached vk renderer");
    }

#if KIT_RENDER_ENABLE_VK_BACKEND
    {
        VkRenderer *renderer = (VkRenderer *)state->renderer;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D extent = { 0, 0 };
        VkResult result;

        result = vk_renderer_begin_frame(renderer, &cmd, &framebuffer, &extent);
        (void)framebuffer;
        (void)extent;
        if (result != VK_SUCCESS) {
            CoreResult r = { CORE_ERR_IO, "vk_renderer_begin_frame failed" };
            return r;
        }

        state->frame_command_buffer = (void *)cmd;
        state->frame_active = 1;
    }
#endif

    return core_result_ok();
}

#if KIT_RENDER_ENABLE_VK_BACKEND
static void vk_backend_apply_color(VkRenderer *renderer, KitRenderColor color) {
    vk_renderer_set_draw_color(renderer,
                               (float)color.r / 255.0f,
                               (float)color.g / 255.0f,
                               (float)color.b / 255.0f,
                               (float)color.a / 255.0f);
}

static SDL_Rect vk_backend_rect_to_sdl(KitRenderRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    return out;
}

static void vk_backend_glyph_rows(unsigned char c, uint8_t rows[7]) {
    size_t i;

    for (i = 0; i < 7; ++i) {
        rows[i] = 0;
    }

    if (c >= 'a' && c <= 'z') {
        c = (unsigned char)toupper((int)c);
    }

    switch (c) {
        case 'A': rows[0]=14; rows[1]=17; rows[2]=17; rows[3]=31; rows[4]=17; rows[5]=17; rows[6]=17; break;
        case 'B': rows[0]=30; rows[1]=17; rows[2]=17; rows[3]=30; rows[4]=17; rows[5]=17; rows[6]=30; break;
        case 'C': rows[0]=14; rows[1]=17; rows[2]=16; rows[3]=16; rows[4]=16; rows[5]=17; rows[6]=14; break;
        case 'D': rows[0]=30; rows[1]=17; rows[2]=17; rows[3]=17; rows[4]=17; rows[5]=17; rows[6]=30; break;
        case 'E': rows[0]=31; rows[1]=16; rows[2]=16; rows[3]=30; rows[4]=16; rows[5]=16; rows[6]=31; break;
        case 'F': rows[0]=31; rows[1]=16; rows[2]=16; rows[3]=30; rows[4]=16; rows[5]=16; rows[6]=16; break;
        case 'G': rows[0]=14; rows[1]=17; rows[2]=16; rows[3]=23; rows[4]=17; rows[5]=17; rows[6]=14; break;
        case 'H': rows[0]=17; rows[1]=17; rows[2]=17; rows[3]=31; rows[4]=17; rows[5]=17; rows[6]=17; break;
        case 'I': rows[0]=31; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=4; rows[5]=4; rows[6]=31; break;
        case 'J': rows[0]=1; rows[1]=1; rows[2]=1; rows[3]=1; rows[4]=17; rows[5]=17; rows[6]=14; break;
        case 'K': rows[0]=17; rows[1]=18; rows[2]=20; rows[3]=24; rows[4]=20; rows[5]=18; rows[6]=17; break;
        case 'L': rows[0]=16; rows[1]=16; rows[2]=16; rows[3]=16; rows[4]=16; rows[5]=16; rows[6]=31; break;
        case 'M': rows[0]=17; rows[1]=27; rows[2]=21; rows[3]=21; rows[4]=17; rows[5]=17; rows[6]=17; break;
        case 'N': rows[0]=17; rows[1]=25; rows[2]=21; rows[3]=19; rows[4]=17; rows[5]=17; rows[6]=17; break;
        case 'O': rows[0]=14; rows[1]=17; rows[2]=17; rows[3]=17; rows[4]=17; rows[5]=17; rows[6]=14; break;
        case 'P': rows[0]=30; rows[1]=17; rows[2]=17; rows[3]=30; rows[4]=16; rows[5]=16; rows[6]=16; break;
        case 'Q': rows[0]=14; rows[1]=17; rows[2]=17; rows[3]=17; rows[4]=21; rows[5]=18; rows[6]=13; break;
        case 'R': rows[0]=30; rows[1]=17; rows[2]=17; rows[3]=30; rows[4]=20; rows[5]=18; rows[6]=17; break;
        case 'S': rows[0]=15; rows[1]=16; rows[2]=16; rows[3]=14; rows[4]=1; rows[5]=1; rows[6]=30; break;
        case 'T': rows[0]=31; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=4; rows[5]=4; rows[6]=4; break;
        case 'U': rows[0]=17; rows[1]=17; rows[2]=17; rows[3]=17; rows[4]=17; rows[5]=17; rows[6]=14; break;
        case 'V': rows[0]=17; rows[1]=17; rows[2]=17; rows[3]=17; rows[4]=17; rows[5]=10; rows[6]=4; break;
        case 'W': rows[0]=17; rows[1]=17; rows[2]=17; rows[3]=21; rows[4]=21; rows[5]=21; rows[6]=10; break;
        case 'X': rows[0]=17; rows[1]=17; rows[2]=10; rows[3]=4; rows[4]=10; rows[5]=17; rows[6]=17; break;
        case 'Y': rows[0]=17; rows[1]=17; rows[2]=10; rows[3]=4; rows[4]=4; rows[5]=4; rows[6]=4; break;
        case 'Z': rows[0]=31; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=8; rows[5]=16; rows[6]=31; break;
        case '0': rows[0]=14; rows[1]=17; rows[2]=19; rows[3]=21; rows[4]=25; rows[5]=17; rows[6]=14; break;
        case '1': rows[0]=4; rows[1]=12; rows[2]=4; rows[3]=4; rows[4]=4; rows[5]=4; rows[6]=14; break;
        case '2': rows[0]=14; rows[1]=17; rows[2]=1; rows[3]=2; rows[4]=4; rows[5]=8; rows[6]=31; break;
        case '3': rows[0]=30; rows[1]=1; rows[2]=1; rows[3]=14; rows[4]=1; rows[5]=1; rows[6]=30; break;
        case '4': rows[0]=2; rows[1]=6; rows[2]=10; rows[3]=18; rows[4]=31; rows[5]=2; rows[6]=2; break;
        case '5': rows[0]=31; rows[1]=16; rows[2]=16; rows[3]=30; rows[4]=1; rows[5]=1; rows[6]=30; break;
        case '6': rows[0]=14; rows[1]=16; rows[2]=16; rows[3]=30; rows[4]=17; rows[5]=17; rows[6]=14; break;
        case '7': rows[0]=31; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=8; rows[5]=8; rows[6]=8; break;
        case '8': rows[0]=14; rows[1]=17; rows[2]=17; rows[3]=14; rows[4]=17; rows[5]=17; rows[6]=14; break;
        case '9': rows[0]=14; rows[1]=17; rows[2]=17; rows[3]=15; rows[4]=1; rows[5]=1; rows[6]=14; break;
        case '-': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=14; rows[4]=0; rows[5]=0; rows[6]=0; break;
        case '_': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=0; rows[5]=0; rows[6]=31; break;
        case '.': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=0; rows[5]=12; rows[6]=12; break;
        case ':': rows[0]=0; rows[1]=12; rows[2]=12; rows[3]=0; rows[4]=12; rows[5]=12; rows[6]=0; break;
        case '/': rows[0]=1; rows[1]=2; rows[2]=4; rows[3]=8; rows[4]=16; rows[5]=0; rows[6]=0; break;
        case ' ': break;
        default: rows[0]=14; rows[1]=17; rows[2]=1; rows[3]=2; rows[4]=4; rows[5]=0; rows[6]=4; break;
    }
}

static int vk_backend_text_scale(const KitRenderContext *ctx,
                                 const KitRenderTextCommand *text_cmd) {
    CoreFontRoleSpec role_spec;
    int point_size = 0;

    if (core_font_resolve_role(&ctx->font, text_cmd->font_role, &role_spec).code != CORE_OK) {
        return 2;
    }
    if (core_font_point_size_for_tier(&role_spec, text_cmd->text_tier, &point_size).code != CORE_OK) {
        point_size = role_spec.point_size;
    }
    if (point_size < 8) return 1;
    if (point_size < 14) return 2;
    if (point_size < 20) return 3;
    if (point_size < 28) return 4;
    return 5;
}

static void vk_backend_clear_font_cache(KitRenderVkBackendState *state) {
    int role_index;
    int tier_index;

    if (!state) {
        return;
    }

    for (role_index = 0; role_index < CORE_FONT_ROLE_COUNT; ++role_index) {
        for (tier_index = 0; tier_index < CORE_FONT_TEXT_SIZE_COUNT; ++tier_index) {
            if (state->ttf_font_cache[role_index][tier_index]) {
                TTF_CloseFont(state->ttf_font_cache[role_index][tier_index]);
                state->ttf_font_cache[role_index][tier_index] = 0;
            }
            state->ttf_font_failed[role_index][tier_index] = 0u;
        }
    }

    state->ttf_cache_initialized = 0;
}

static int vk_backend_stat_path_exists(const char *path, void *user) {
    struct stat st;
    (void)user;

    if (!path || !path[0]) {
        return 0;
    }
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int vk_backend_path_is_absolute(const char *path) {
    if (!path || !path[0]) {
        return 0;
    }
#if defined(_WIN32)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }
#endif
    return path[0] == '/';
}

static TTF_Font *vk_backend_open_font_with_search(const char *font_path, int point_size) {
    TTF_Font *font;
    char *base_path;
    int depth;

    if (!font_path || !font_path[0]) {
        return 0;
    }

    font = TTF_OpenFont(font_path, point_size);
    if (font) {
        return font;
    }
    if (vk_backend_path_is_absolute(font_path)) {
        return 0;
    }

    base_path = SDL_GetBasePath();
    if (!base_path || !base_path[0]) {
        if (base_path) {
            SDL_free(base_path);
        }
        return 0;
    }

    for (depth = 0; depth <= 8; ++depth) {
        char candidate[2048];
        int written;
        int offset;
        int i;

        written = snprintf(candidate, sizeof(candidate), "%s", base_path);
        if (written <= 0 || (size_t)written >= sizeof(candidate)) {
            continue;
        }
        offset = written;
        for (i = 0; i < depth; ++i) {
            written = snprintf(candidate + offset,
                               sizeof(candidate) - (size_t)offset,
                               "../");
            if (written <= 0 || (size_t)written >= (sizeof(candidate) - (size_t)offset)) {
                offset = -1;
                break;
            }
            offset += written;
        }
        if (offset < 0) {
            continue;
        }

        written = snprintf(candidate + offset,
                           sizeof(candidate) - (size_t)offset,
                           "%s",
                           font_path);
        if (written <= 0 || (size_t)written >= (sizeof(candidate) - (size_t)offset)) {
            continue;
        }

        font = TTF_OpenFont(candidate, point_size);
        if (font) {
            SDL_free(base_path);
            return font;
        }
    }

    SDL_free(base_path);
    return 0;
}

static CoreResult vk_backend_get_ttf_font(KitRenderContext *ctx,
                                          KitRenderVkBackendState *state,
                                          CoreFontRoleId role_id,
                                          CoreFontTextSizeTier tier,
                                          TTF_Font **out_font) {
    CoreFontRoleSpec role_spec;
    CoreResult result;
    const char *font_paths[2];
    int point_size = 0;
    TTF_Font *font = 0;
    int path_index;

    if (!ctx || !state || !out_font) {
        return vk_backend_invalid("invalid ttf font request");
    }
    if ((int)role_id < 0 || role_id >= CORE_FONT_ROLE_COUNT ||
        (int)tier < 0 || tier >= CORE_FONT_TEXT_SIZE_COUNT) {
        return vk_backend_invalid("invalid ttf font key");
    }

    if (state->ttf_runtime_failed) {
        return vk_backend_invalid("sdl_ttf unavailable");
    }
    if (!state->ttf_runtime_ready) {
        if (TTF_WasInit() == 0 && TTF_Init() != 0) {
            state->ttf_runtime_failed = 1;
            return vk_backend_invalid("failed to initialize sdl_ttf");
        }
        state->ttf_runtime_ready = 1;
    }

    if (!state->ttf_cache_initialized || state->ttf_cache_preset_id != ctx->font.id) {
        vk_backend_clear_font_cache(state);
        state->ttf_cache_preset_id = ctx->font.id;
        state->ttf_cache_initialized = 1;
    }

    font = state->ttf_font_cache[(int)role_id][(int)tier];
    if (font) {
        *out_font = font;
        return core_result_ok();
    }
    if (state->ttf_font_failed[(int)role_id][(int)tier]) {
        return vk_backend_invalid("cached ttf font failure");
    }

    result = core_font_resolve_role(&ctx->font, role_id, &role_spec);
    if (result.code != CORE_OK) {
        state->ttf_font_failed[(int)role_id][(int)tier] = 1u;
        return result;
    }
    result = core_font_point_size_for_tier(&role_spec, tier, &point_size);
    if (result.code != CORE_OK) {
        point_size = role_spec.point_size;
    }
    if (point_size <= 0) {
        point_size = 10;
    }

    font_paths[0] = role_spec.primary_path;
    font_paths[1] = role_spec.fallback_path;
    for (path_index = 0; path_index < 2; ++path_index) {
        const char *path = font_paths[path_index];
        if (!path || !path[0]) {
            continue;
        }
        font = vk_backend_open_font_with_search(path, point_size);
        if (font) {
            break;
        }
    }
    if (!font) {
        const char *chosen_path = 0;
        result = core_font_choose_path(&role_spec, vk_backend_stat_path_exists, 0, &chosen_path);
        (void)chosen_path;
        state->ttf_font_failed[(int)role_id][(int)tier] = 1u;
        return vk_backend_invalid("failed to open font");
    }

    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(font, 1);

    state->ttf_font_cache[(int)role_id][(int)tier] = font;
    *out_font = font;
    return core_result_ok();
}

static CoreResult vk_backend_draw_text_bitmap(KitRenderContext *ctx,
                                              VkRenderer *renderer,
                                              const KitRenderTextCommand *text_cmd) {
    CoreThemeColor theme_color;
    SDL_Surface *surface;
    VkRendererTexture texture;
    size_t length;
    int scale;
    int glyph_w = 5;
    int glyph_h = 7;
    int spacing;
    int width;
    int height;
    size_t i;

    if (!ctx || !renderer || !text_cmd || !text_cmd->text) {
        return vk_backend_invalid("invalid text command");
    }

    if (core_theme_get_color(&ctx->theme, text_cmd->color_token, &theme_color).code != CORE_OK) {
        theme_color.r = 255;
        theme_color.g = 255;
        theme_color.b = 255;
        theme_color.a = 255;
    }

    length = strlen(text_cmd->text);
    if (length == 0) {
        return core_result_ok();
    }

    scale = vk_backend_text_scale(ctx, text_cmd);
    spacing = scale;
    width = (int)length * ((glyph_w * scale) + spacing);
    if (width <= 0) {
        width = glyph_w * scale;
    }
    height = glyph_h * scale;

    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return vk_backend_invalid("failed to create text surface");
    }

    SDL_FillRect(surface, 0, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    for (i = 0; i < length; ++i) {
        uint8_t rows[7];
        int gx;
        int gy;
        int pixel_x = (int)i * ((glyph_w * scale) + spacing);

        vk_backend_glyph_rows((unsigned char)text_cmd->text[i], rows);
        for (gy = 0; gy < glyph_h; ++gy) {
            for (gx = 0; gx < glyph_w; ++gx) {
                if ((rows[gy] >> (glyph_w - 1 - gx)) & 1u) {
                    SDL_Rect px = {
                        pixel_x + (gx * scale),
                        gy * scale,
                        scale,
                        scale
                    };
                    SDL_FillRect(surface,
                                 &px,
                                 SDL_MapRGBA(surface->format,
                                             theme_color.r,
                                             theme_color.g,
                                             theme_color.b,
                                             theme_color.a));
                }
            }
        }
    }

    if (vk_renderer_upload_sdl_surface(renderer, surface, &texture) != VK_SUCCESS) {
        SDL_FreeSurface(surface);
        return vk_backend_invalid("failed to upload text texture");
    }

    {
        SDL_Rect dst = {
            (int)text_cmd->origin.x,
            (int)(text_cmd->origin.y - ((float)height * 0.5f)),
            width,
            height
        };
        vk_renderer_draw_texture(renderer, &texture, 0, &dst);
    }

    vk_renderer_queue_texture_destroy(renderer, &texture);
    SDL_FreeSurface(surface);
    return core_result_ok();
}

static CoreResult vk_backend_draw_text_ttf(KitRenderContext *ctx,
                                           KitRenderVkBackendState *state,
                                           VkRenderer *renderer,
                                           const KitRenderTextCommand *text_cmd) {
    CoreResult font_result;
    TTF_Font *font = 0;
    SDL_Color color;
    CoreThemeColor theme_color;
    SDL_Surface *surface = 0;
    VkRendererTexture texture;
    SDL_Rect dst;

    if (!ctx || !state || !renderer || !text_cmd || !text_cmd->text || text_cmd->text[0] == '\0') {
        return vk_backend_invalid("invalid text command");
    }

    font_result = vk_backend_get_ttf_font(ctx, state, text_cmd->font_role, text_cmd->text_tier, &font);
    if (font_result.code != CORE_OK) {
        return font_result;
    }

    if (core_theme_get_color(&ctx->theme, text_cmd->color_token, &theme_color).code != CORE_OK) {
        theme_color.r = 255;
        theme_color.g = 255;
        theme_color.b = 255;
        theme_color.a = 255;
    }
    color.r = theme_color.r;
    color.g = theme_color.g;
    color.b = theme_color.b;
    color.a = theme_color.a;

    surface = TTF_RenderUTF8_Blended(font, text_cmd->text, color);
    if (!surface) {
        return vk_backend_invalid("failed to rasterize text");
    }
    if (surface->w <= 0 || surface->h <= 0) {
        SDL_FreeSurface(surface);
        return core_result_ok();
    }

    if (vk_renderer_upload_sdl_surface(renderer, surface, &texture) != VK_SUCCESS) {
        SDL_FreeSurface(surface);
        return vk_backend_invalid("failed to upload text texture");
    }

    dst.x = (int)text_cmd->origin.x;
    dst.y = (int)(text_cmd->origin.y - ((float)surface->h * 0.5f));
    dst.w = surface->w;
    dst.h = surface->h;
    vk_renderer_draw_texture(renderer, &texture, 0, &dst);
    vk_renderer_queue_texture_destroy(renderer, &texture);
    SDL_FreeSurface(surface);
    return core_result_ok();
}

static CoreResult vk_backend_draw_text(KitRenderContext *ctx,
                                       KitRenderVkBackendState *state,
                                       VkRenderer *renderer,
                                       const KitRenderTextCommand *text_cmd) {
    CoreResult ttf_result;

    ttf_result = vk_backend_draw_text_ttf(ctx, state, renderer, text_cmd);
    if (ttf_result.code == CORE_OK) {
        return core_result_ok();
    }

    return vk_backend_draw_text_bitmap(ctx, renderer, text_cmd);
}

static CoreResult vk_backend_measure_text_bitmap(const KitRenderContext *ctx,
                                                 CoreFontRoleId font_role,
                                                 CoreFontTextSizeTier text_tier,
                                                 const char *text,
                                                 KitRenderTextMetrics *out_metrics) {
    KitRenderTextCommand text_cmd;
    size_t length;
    int scale;
    int glyph_w = 5;
    int glyph_h = 7;
    int spacing;

    if (!ctx || !text || !out_metrics) {
        return vk_backend_invalid("invalid text metrics request");
    }

    text_cmd.origin = (KitRenderVec2){ 0.0f, 0.0f };
    text_cmd.text = text;
    text_cmd.font_role = font_role;
    text_cmd.text_tier = text_tier;
    text_cmd.color_token = CORE_THEME_COLOR_TEXT_PRIMARY;
    text_cmd.transform = kit_render_identity_transform();

    length = strlen(text);
    scale = vk_backend_text_scale(ctx, &text_cmd);
    spacing = scale;

    out_metrics->width_px = (float)((int)length * ((glyph_w * scale) + spacing));
    out_metrics->height_px = (float)(glyph_h * scale);
    if (out_metrics->height_px <= 0.0f) {
        out_metrics->height_px = 14.0f;
    }
    return core_result_ok();
}

static CoreResult vk_backend_measure_text_ttf(const KitRenderContext *ctx,
                                              KitRenderVkBackendState *state,
                                              CoreFontRoleId font_role,
                                              CoreFontTextSizeTier text_tier,
                                              const char *text,
                                              KitRenderTextMetrics *out_metrics) {
    CoreResult font_result;
    TTF_Font *font = 0;
    int width = 0;
    int height = 0;

    if (!ctx || !state || !text || !out_metrics) {
        return vk_backend_invalid("invalid text metrics request");
    }

    font_result = vk_backend_get_ttf_font((KitRenderContext *)ctx,
                                          state,
                                          font_role,
                                          text_tier,
                                          &font);
    if (font_result.code != CORE_OK) {
        return font_result;
    }

    if (text[0] == '\0') {
        out_metrics->width_px = 0.0f;
        out_metrics->height_px = (float)TTF_FontHeight(font);
        if (out_metrics->height_px <= 0.0f) {
            out_metrics->height_px = 14.0f;
        }
        return core_result_ok();
    }

    if (TTF_SizeUTF8(font, text, &width, &height) != 0) {
        return vk_backend_invalid("failed to measure text");
    }

    out_metrics->width_px = (float)width;
    out_metrics->height_px = (float)height;
    if (out_metrics->height_px <= 0.0f) {
        out_metrics->height_px = (float)TTF_FontHeight(font);
    }
    if (out_metrics->height_px <= 0.0f) {
        out_metrics->height_px = 14.0f;
    }

    return core_result_ok();
}

static CoreResult vk_backend_measure_text(const KitRenderContext *ctx,
                                          CoreFontRoleId font_role,
                                          CoreFontTextSizeTier text_tier,
                                          const char *text,
                                          KitRenderTextMetrics *out_metrics) {
    KitRenderVkBackendState *state = vk_backend_state(ctx);

    if (!ctx || !text || !out_metrics) {
        return vk_backend_invalid("invalid text metrics request");
    }
    if (!state) {
        return vk_backend_invalid("vulkan backend not initialized");
    }

#if KIT_RENDER_ENABLE_VK_BACKEND
    {
        CoreResult ttf_result = vk_backend_measure_text_ttf(ctx,
                                                            state,
                                                            font_role,
                                                            text_tier,
                                                            text,
                                                            out_metrics);
        if (ttf_result.code == CORE_OK) {
            return core_result_ok();
        }
    }
#endif

    return vk_backend_measure_text_bitmap(ctx, font_role, text_tier, text, out_metrics);
}

static CoreResult vk_backend_submit_enabled(KitRenderContext *ctx, KitRenderFrame *frame) {
    KitRenderVkBackendState *state = vk_backend_state(ctx);
    VkRenderer *renderer;
    size_t i;

    if (!state || !state->renderer) {
        return vk_backend_invalid("no attached vk renderer");
    }

    renderer = (VkRenderer *)state->renderer;
    vk_renderer_set_logical_size(renderer, (float)frame->width_px, (float)frame->height_px);

    for (i = 0; i < frame->command_buffer->count; ++i) {
        const KitRenderCommand *cmd = &frame->command_buffer->commands[i];

        switch (cmd->kind) {
            case KIT_RENDER_CMD_CLEAR: {
                SDL_Rect full = { 0, 0, (int)frame->width_px, (int)frame->height_px };
                vk_backend_apply_color(renderer, cmd->data.clear.color);
                vk_renderer_fill_rect(renderer, &full);
                break;
            }
            case KIT_RENDER_CMD_SET_CLIP: {
                SDL_Rect clip = vk_backend_rect_to_sdl(cmd->data.clip.rect);
                vk_renderer_set_clip_rect(renderer, &clip);
                break;
            }
            case KIT_RENDER_CMD_CLEAR_CLIP:
                vk_renderer_set_clip_rect(renderer, 0);
                break;
            case KIT_RENDER_CMD_RECT: {
                SDL_Rect rect = vk_backend_rect_to_sdl(cmd->data.rect.rect);
                vk_backend_apply_color(renderer, cmd->data.rect.color);
                /* Rounded rects currently fall back to filled rects in the bridge. */
                vk_renderer_fill_rect(renderer, &rect);
                break;
            }
            case KIT_RENDER_CMD_LINE:
                vk_backend_apply_color(renderer, cmd->data.line.color);
                vk_renderer_draw_line(renderer,
                                      cmd->data.line.p0.x,
                                      cmd->data.line.p0.y,
                                      cmd->data.line.p1.x,
                                      cmd->data.line.p1.y);
                break;
            case KIT_RENDER_CMD_POLYLINE:
                vk_backend_apply_color(renderer, cmd->data.polyline.color);
                vk_renderer_draw_line_strip(renderer,
                                            (const SDL_FPoint *)cmd->data.polyline.points,
                                            cmd->data.polyline.point_count);
                break;
            case KIT_RENDER_CMD_TEXTURED_QUAD: {
                SDL_Rect dst = vk_backend_rect_to_sdl(cmd->data.textured_quad.rect);
                const VkRendererTexture *texture =
                    (const VkRendererTexture *)(uintptr_t)cmd->data.textured_quad.texture_id;
                vk_renderer_draw_texture(renderer, texture, 0, &dst);
                break;
            }
            case KIT_RENDER_CMD_TEXT:
                {
                    CoreResult text_result = vk_backend_draw_text(ctx, state, renderer, &cmd->data.text);
                    if (text_result.code != CORE_OK) {
                        return text_result;
                    }
                    break;
                }
            default:
                return vk_backend_invalid("unknown render command");
        }
    }

    return core_result_ok();
}
#endif

#if !KIT_RENDER_ENABLE_VK_BACKEND
static CoreResult vk_backend_measure_text(const KitRenderContext *ctx,
                                          CoreFontRoleId font_role,
                                          CoreFontTextSizeTier text_tier,
                                          const char *text,
                                          KitRenderTextMetrics *out_metrics) {
    int char_width = 9;
    int line_height = 18;
    size_t len;

    (void)ctx;
    (void)font_role;

    if (!text || !out_metrics) {
        return vk_backend_invalid("invalid text metrics request");
    }

    switch (text_tier) {
        case CORE_FONT_TEXT_SIZE_HEADER:
            char_width = 13;
            line_height = 28;
            break;
        case CORE_FONT_TEXT_SIZE_TITLE:
            char_width = 11;
            line_height = 22;
            break;
        case CORE_FONT_TEXT_SIZE_CAPTION:
            char_width = 8;
            line_height = 16;
            break;
        case CORE_FONT_TEXT_SIZE_PARAGRAPH:
        case CORE_FONT_TEXT_SIZE_BASIC:
        default:
            char_width = 9;
            line_height = 18;
            break;
    }

    len = strlen(text);
    out_metrics->width_px = (float)((int)len * char_width);
    out_metrics->height_px = (float)line_height;
    return core_result_ok();
}
#endif

static CoreResult vk_backend_submit(KitRenderContext *ctx, KitRenderFrame *frame) {
    if (!frame || !frame->command_buffer) {
        return vk_backend_invalid("invalid frame");
    }

#if KIT_RENDER_ENABLE_VK_BACKEND
    return vk_backend_submit_enabled(ctx, frame);
#else
    (void)ctx;
    return vk_backend_invalid("vulkan backend support disabled at build time");
#endif
}

static CoreResult vk_backend_end_frame(KitRenderContext *ctx, KitRenderFrame *frame) {
    KitRenderVkBackendState *state = vk_backend_state(ctx);

    if (!frame || !frame->command_buffer) {
        return vk_backend_invalid("invalid frame");
    }

#if KIT_RENDER_ENABLE_VK_BACKEND
    if (state && state->renderer && state->frame_active) {
        VkResult result = vk_renderer_end_frame((VkRenderer *)state->renderer,
                                                (VkCommandBuffer)state->frame_command_buffer);
        if (result != VK_SUCCESS) {
            CoreResult r = { CORE_ERR_IO, "vk_renderer_end_frame failed" };
            return r;
        }
    }
#endif

    if (state) {
        state->frame_command_buffer = 0;
        state->frame_active = 0;
    }
    return core_result_ok();
}

static CoreResult vk_backend_wait_idle(KitRenderContext *ctx) {
#if KIT_RENDER_ENABLE_VK_BACKEND
    KitRenderVkBackendState *state = vk_backend_state(ctx);
    if (state && state->renderer) {
        vk_renderer_wait_idle((VkRenderer *)state->renderer);
    }
#else
    (void)ctx;
#endif
    return core_result_ok();
}

const KitRenderBackendOps *kit_render_backend_vk_ops(void) {
    static const KitRenderBackendOps ops = {
        vk_backend_init,
        vk_backend_shutdown,
        vk_backend_attach_external,
        vk_backend_begin_frame,
        vk_backend_submit,
        vk_backend_end_frame,
        vk_backend_wait_idle,
        vk_backend_measure_text
    };
    return &ops;
}
