#include "ui/font.h"

#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <string.h>

#if defined(MAPFORGE_HAVE_VK)
#include <vulkan/vulkan.h>
#include "vk_renderer.h"
#endif

typedef struct {
    int size;
    TTF_Font *font;
} FontCacheEntry;

typedef struct {
    char text[128];
    SDL_Color color;
    float scale;
    int width;
    int height;
    SDL_Texture *texture;
#if defined(MAPFORGE_HAVE_VK)
    VkRendererTexture vk_texture;
    bool has_vk_texture;
    void *vk_owner;
#endif
    bool use_vulkan;
    uint32_t stamp;
    bool in_use;
} TextCacheEntry;

static char g_font_path[256] = "assets/fonts/Montserrat-Regular.ttf";
static int g_base_point_size = 10;
static FontCacheEntry g_cache[12];
static int g_cache_count = 0;
static TextCacheEntry g_text_cache[128];
static uint32_t g_text_cache_stamp = 0;
static void clear_font_cache(void) {
    for (int i = 0; i < g_cache_count; ++i) {
        if (g_cache[i].font) {
            TTF_CloseFont(g_cache[i].font);
            g_cache[i].font = NULL;
        }
    }
    g_cache_count = 0;
}

static void text_cache_clear(Renderer *renderer) {
    for (size_t i = 0; i < (sizeof(g_text_cache) / sizeof(g_text_cache[0])); ++i) {
        if (g_text_cache[i].in_use) {
            if (g_text_cache[i].texture) {
                SDL_DestroyTexture(g_text_cache[i].texture);
            }
#if defined(MAPFORGE_HAVE_VK)
            if (g_text_cache[i].has_vk_texture) {
                if (renderer &&
                    renderer->backend == RENDERER_BACKEND_VULKAN &&
                    renderer->vk &&
                    renderer->vk == g_text_cache[i].vk_owner) {
                    VkRenderer *vk = (VkRenderer *)renderer->vk;
                    vk_renderer_queue_texture_destroy(vk, &g_text_cache[i].vk_texture);
                }
            }
#endif
            g_text_cache[i] = (TextCacheEntry){0};
        }
    }
    g_text_cache_stamp = 0;
}

static TextCacheEntry *text_cache_find(const char *text, SDL_Color color, float scale) {
    if (!text) {
        return NULL;
    }
    for (size_t i = 0; i < (sizeof(g_text_cache) / sizeof(g_text_cache[0])); ++i) {
        TextCacheEntry *entry = &g_text_cache[i];
        if (!entry->in_use) {
            continue;
        }
        if (entry->color.r != color.r || entry->color.g != color.g ||
            entry->color.b != color.b || entry->color.a != color.a) {
            continue;
        }
        if (entry->scale != scale) {
            continue;
        }
        if (strncmp(entry->text, text, sizeof(entry->text)) == 0) {
            return entry;
        }
    }
    return NULL;
}

static TextCacheEntry *text_cache_pick_slot(void) {
    TextCacheEntry *oldest = NULL;
    for (size_t i = 0; i < (sizeof(g_text_cache) / sizeof(g_text_cache[0])); ++i) {
        TextCacheEntry *entry = &g_text_cache[i];
        if (!entry->in_use) {
            return entry;
        }
        if (!oldest || entry->stamp < oldest->stamp) {
            oldest = entry;
        }
    }
    return oldest;
}

bool ui_font_set(const char *path, int base_point_size) {
    if (path && path[0]) {
        strncpy(g_font_path, path, sizeof(g_font_path) - 1);
        g_font_path[sizeof(g_font_path) - 1] = '\0';
    }
    if (base_point_size > 0) {
        g_base_point_size = base_point_size;
    }
    clear_font_cache();
    text_cache_clear(NULL);
    return true;
}

void ui_font_shutdown(Renderer *renderer) {
    clear_font_cache();
    text_cache_clear(renderer);
}

void ui_font_invalidate_cache(Renderer *renderer) {
    text_cache_clear(renderer);
}

static TTF_Font *get_font_for_scale(float scale) {
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    int size = (int)lroundf((float)g_base_point_size * scale);
    if (size < 4) {
        size = 4;
    }
    for (int i = 0; i < g_cache_count; ++i) {
        if (g_cache[i].size == size && g_cache[i].font) {
            return g_cache[i].font;
        }
    }
    if (g_cache_count >= (int)(sizeof(g_cache) / sizeof(g_cache[0]))) {
        TTF_CloseFont(g_cache[0].font);
        memmove(&g_cache[0], &g_cache[1], (size_t)(g_cache_count - 1) * sizeof(FontCacheEntry));
        g_cache_count -= 1;
    }
    TTF_Font *font = TTF_OpenFont(g_font_path, size);
    if (!font) {
        SDL_Log("Failed to load font %s @ %d: %s", g_font_path, size, TTF_GetError());
        return NULL;
    }
    g_cache[g_cache_count++] = (FontCacheEntry){.size = size, .font = font};
    return font;
}

static bool render_text(Renderer *renderer,
                        TTF_Font *font,
                        const char *text,
                        SDL_Color color,
                        SDL_Texture **out_texture,
#if defined(MAPFORGE_HAVE_VK)
                        VkRendererTexture *out_vk_texture,
#endif
                        bool *out_use_vulkan,
                        int *out_w,
                        int *out_h) {
    if (!renderer || !font || !text || !out_texture) {
        return false;
    }
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return false;
    }
    if (out_w) *out_w = surface->w;
    if (out_h) *out_h = surface->h;
    *out_texture = NULL;
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        if (out_vk_texture) {
            VkResult vk_result = vk_renderer_upload_sdl_surface(vk, surface, out_vk_texture);
            SDL_FreeSurface(surface);
            if (vk_result != VK_SUCCESS) {
                return false;
            }
            if (out_use_vulkan) {
                *out_use_vulkan = true;
            }
            return true;
        }
    }
#endif
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer->sdl, surface);
    SDL_FreeSurface(surface);
    if (!tex) {
        return false;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    *out_texture = tex;
    if (out_use_vulkan) {
        *out_use_vulkan = false;
    }
    return true;
}

void ui_draw_text(Renderer *renderer, int x, int y, const char *text, SDL_Color color, float scale) {
    if (!renderer || !text) {
        return;
    }
    TTF_Font *font = get_font_for_scale(scale);
    if (!font) {
        return;
    }
    if (strlen(text) >= sizeof(g_text_cache[0].text)) {
        int w = 0, h = 0;
        SDL_Texture *tex = NULL;
        bool use_vulkan = false;
#if defined(MAPFORGE_HAVE_VK)
        VkRendererTexture vk_texture = {0};
        if (!render_text(renderer, font, text, color, &tex, &vk_texture, &use_vulkan, &w, &h)) {
            return;
        }
        SDL_Rect dst = {x, y, w, h};
        if (use_vulkan) {
            VkRenderer *vk = (VkRenderer *)renderer->vk;
            vk_renderer_draw_texture(vk, &vk_texture, NULL, &dst);
            vk_renderer_queue_texture_destroy(vk, &vk_texture);
        } else if (tex && renderer->sdl) {
            SDL_RenderCopy(renderer->sdl, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
        }
#else
        if (!render_text(renderer, font, text, color, &tex, &use_vulkan, &w, &h)) {
            return;
        }
        SDL_Rect dst = {x, y, w, h};
        if (tex && renderer->sdl) {
            SDL_RenderCopy(renderer->sdl, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
        }
#endif
        return;
    }

    TextCacheEntry *entry = text_cache_find(text, color, scale);
    if (!entry) {
        entry = text_cache_pick_slot();
        if (!entry) {
            return;
        }
        if (entry->in_use && entry->texture) {
            SDL_DestroyTexture(entry->texture);
        }
#if defined(MAPFORGE_HAVE_VK)
        if (entry->in_use && entry->has_vk_texture &&
            renderer->backend == RENDERER_BACKEND_VULKAN &&
            renderer->vk &&
            renderer->vk == entry->vk_owner) {
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer->vk, &entry->vk_texture);
        }
#endif
        SDL_Texture *tex = NULL;
        bool use_vulkan = false;
        int w = 0, h = 0;
#if defined(MAPFORGE_HAVE_VK)
        VkRendererTexture vk_texture = {0};
        if (!render_text(renderer, font, text, color, &tex, &vk_texture, &use_vulkan, &w, &h)) {
            return;
        }
#else
        if (!render_text(renderer, font, text, color, &tex, &use_vulkan, &w, &h)) {
            return;
        }
#endif
        *entry = (TextCacheEntry){
            .color = color,
            .scale = scale,
            .width = w,
            .height = h,
            .texture = tex,
            .use_vulkan = use_vulkan,
            .stamp = ++g_text_cache_stamp,
            .in_use = true
        };
#if defined(MAPFORGE_HAVE_VK)
        entry->vk_texture = vk_texture;
        entry->has_vk_texture = use_vulkan;
        entry->vk_owner = use_vulkan ? renderer->vk : NULL;
#endif
        strncpy(entry->text, text, sizeof(entry->text) - 1);
        entry->text[sizeof(entry->text) - 1] = '\0';
    }
    entry->stamp = ++g_text_cache_stamp;
    SDL_Rect dst = {x, y, entry->width, entry->height};
#if defined(MAPFORGE_HAVE_VK)
    if (entry->use_vulkan && entry->has_vk_texture && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        vk_renderer_draw_texture((VkRenderer *)renderer->vk, &entry->vk_texture, NULL, &dst);
    } else if (entry->texture && renderer->sdl) {
        SDL_RenderCopy(renderer->sdl, entry->texture, NULL, &dst);
    }
#else
    if (entry->texture && renderer->sdl) {
        SDL_RenderCopy(renderer->sdl, entry->texture, NULL, &dst);
    }
#endif
}

int ui_measure_text_width(const char *text, float scale) {
    if (!text) {
        return 0;
    }
    TTF_Font *font = get_font_for_scale(scale);
    if (!font) {
        return 0;
    }
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) != 0) {
        return 0;
    }
    return w;
}

void ui_draw_text_clipped(Renderer *renderer,
                          int x,
                          int y,
                          const char *text,
                          SDL_Color color,
                          float scale,
                          int max_width) {
    if (!renderer || !text || max_width <= 0) {
        return;
    }
    TTF_Font *font = get_font_for_scale(scale);
    if (!font) {
        return;
    }
    if (strlen(text) >= sizeof(g_text_cache[0].text)) {
        int w = 0, h = 0;
        SDL_Texture *tex = NULL;
        bool use_vulkan = false;
#if defined(MAPFORGE_HAVE_VK)
        VkRendererTexture vk_texture = {0};
        if (!render_text(renderer, font, text, color, &tex, &vk_texture, &use_vulkan, &w, &h)) {
            return;
        }
#else
        if (!render_text(renderer, font, text, color, &tex, &use_vulkan, &w, &h)) {
            return;
        }
#endif
        int clip_w = w < max_width ? w : max_width;
        SDL_Rect src = {0, 0, clip_w, h};
        SDL_Rect dst = {x, y, clip_w, h};
#if defined(MAPFORGE_HAVE_VK)
        if (use_vulkan && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
            VkRenderer *vk = (VkRenderer *)renderer->vk;
            vk_renderer_draw_texture(vk, &vk_texture, &src, &dst);
            vk_renderer_queue_texture_destroy(vk, &vk_texture);
        } else if (tex && renderer->sdl) {
            SDL_RenderCopy(renderer->sdl, tex, &src, &dst);
            SDL_DestroyTexture(tex);
        }
#else
        if (tex && renderer->sdl) {
            SDL_RenderCopy(renderer->sdl, tex, &src, &dst);
            SDL_DestroyTexture(tex);
        }
#endif
        return;
    }

    TextCacheEntry *entry = text_cache_find(text, color, scale);
    if (!entry) {
        entry = text_cache_pick_slot();
        if (!entry) {
            return;
        }
        if (entry->in_use && entry->texture) {
            SDL_DestroyTexture(entry->texture);
        }
#if defined(MAPFORGE_HAVE_VK)
        if (entry->in_use && entry->has_vk_texture &&
            renderer->backend == RENDERER_BACKEND_VULKAN &&
            renderer->vk &&
            renderer->vk == entry->vk_owner) {
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer->vk, &entry->vk_texture);
        }
#endif
        SDL_Texture *tex = NULL;
        bool use_vulkan = false;
        int w = 0, h = 0;
#if defined(MAPFORGE_HAVE_VK)
        VkRendererTexture vk_texture = {0};
        if (!render_text(renderer, font, text, color, &tex, &vk_texture, &use_vulkan, &w, &h)) {
            return;
        }
#else
        if (!render_text(renderer, font, text, color, &tex, &use_vulkan, &w, &h)) {
            return;
        }
#endif
        *entry = (TextCacheEntry){
            .color = color,
            .scale = scale,
            .width = w,
            .height = h,
            .texture = tex,
            .use_vulkan = use_vulkan,
            .stamp = ++g_text_cache_stamp,
            .in_use = true
        };
#if defined(MAPFORGE_HAVE_VK)
        entry->vk_texture = vk_texture;
        entry->has_vk_texture = use_vulkan;
        entry->vk_owner = use_vulkan ? renderer->vk : NULL;
#endif
        strncpy(entry->text, text, sizeof(entry->text) - 1);
        entry->text[sizeof(entry->text) - 1] = '\0';
    }
    entry->stamp = ++g_text_cache_stamp;
    int clip_w = entry->width < max_width ? entry->width : max_width;
    SDL_Rect src = {0, 0, clip_w, entry->height};
    SDL_Rect dst = {x, y, clip_w, entry->height};
#if defined(MAPFORGE_HAVE_VK)
    if (entry->use_vulkan && entry->has_vk_texture && renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk) {
        vk_renderer_draw_texture((VkRenderer *)renderer->vk, &entry->vk_texture, &src, &dst);
    } else if (entry->texture && renderer->sdl) {
        SDL_RenderCopy(renderer->sdl, entry->texture, &src, &dst);
    }
#else
    if (entry->texture && renderer->sdl) {
        SDL_RenderCopy(renderer->sdl, entry->texture, &src, &dst);
    }
#endif
}

int ui_font_line_height(float scale) {
    TTF_Font *font = get_font_for_scale(scale);
    if (!font) {
        return 0;
    }
    return TTF_FontHeight(font);
}
