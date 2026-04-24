#include "kit_render_external_text.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vk_renderer.h"

enum {
    KIT_RENDER_EXTERNAL_TEXT_FONT_SOURCE_CAPACITY = 32,
    KIT_RENDER_EXTERNAL_TEXT_FONT_CACHE_CAPACITY = 64,
    KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY = 192,
    KIT_RENDER_EXTERNAL_TEXT_MAX_TEXT_LEN = 512
};

typedef struct KitRenderExternalTextFontSourceEntry {
    TTF_Font *font;
    char path[384];
    int logical_point_size;
    int loaded_point_size;
    int kerning_enabled;
} KitRenderExternalTextFontSourceEntry;

typedef struct KitRenderExternalTextFontCacheEntry {
    TTF_Font *font;
    char path[384];
    int point_size;
    int kerning_enabled;
    uint64_t last_used;
} KitRenderExternalTextFontCacheEntry;

typedef struct KitRenderExternalTextTextureCacheEntry {
    int valid;
    SDL_Renderer *renderer;
    TTF_Font *base_font;
    int requested_point_size;
    int wrap_width;
    uint32_t color_rgba;
    size_t text_len;
    uint32_t text_hash;
    char *text;
    int texture_width;
    int texture_height;
    int draw_width;
    int draw_height;
    float raster_scale;
    VkRendererTexture texture;
    uint64_t last_used;
} KitRenderExternalTextTextureCacheEntry;

static KitRenderExternalTextFontSourceEntry
    g_font_sources[KIT_RENDER_EXTERNAL_TEXT_FONT_SOURCE_CAPACITY];
static KitRenderExternalTextFontCacheEntry
    g_font_cache[KIT_RENDER_EXTERNAL_TEXT_FONT_CACHE_CAPACITY];
static KitRenderExternalTextTextureCacheEntry
    g_texture_cache[KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY];
static uint64_t g_font_cache_tick = 1u;
static uint64_t g_texture_cache_tick = 1u;

static float kit_render_external_text_raster_scale(SDL_Renderer *renderer) {
    const VkRenderer *vk = NULL;
    float logical_w = 0.0f;
    float logical_h = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float raster_scale = 1.0f;

    if (!renderer) {
        return 1.0f;
    }

    vk = (const VkRenderer *)renderer;
    logical_w = vk->draw_state.logical_size[0];
    logical_h = vk->draw_state.logical_size[1];
    if (logical_w > 0.0f) {
        scale_x = (float)vk->context.swapchain.extent.width / logical_w;
    }
    if (logical_h > 0.0f) {
        scale_y = (float)vk->context.swapchain.extent.height / logical_h;
    }

    raster_scale = (scale_x > scale_y) ? scale_x : scale_y;
    if (!isfinite(raster_scale) || raster_scale < 1.0f) {
        raster_scale = 1.0f;
    }
    if (raster_scale > 4.0f) {
        raster_scale = 4.0f;
    }
    return raster_scale;
}

static int kit_render_external_text_logical_size_from_raster(int raster_pixels, float raster_scale) {
    int logical_pixels = 0;
    if (raster_pixels <= 0) {
        return 0;
    }
    if (!isfinite(raster_scale) || raster_scale < 1.0f) {
        raster_scale = 1.0f;
    }
    logical_pixels = (int)lroundf((float)raster_pixels / raster_scale);
    if (logical_pixels < 1) {
        logical_pixels = 1;
    }
    return logical_pixels;
}

static VkFilter kit_render_external_text_upload_filter_for_scale(float raster_scale) {
    if (isfinite(raster_scale) && raster_scale > 1.0f) {
        return VK_FILTER_NEAREST;
    }
    return VK_FILTER_LINEAR;
}

static const KitRenderExternalTextFontSourceEntry *
kit_render_external_text_find_font_source(TTF_Font *font) {
    int i = 0;
    if (!font) {
        return NULL;
    }
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_FONT_SOURCE_CAPACITY; ++i) {
        if (g_font_sources[i].font == font && g_font_sources[i].path[0] != '\0') {
            return &g_font_sources[i];
        }
    }
    return NULL;
}

static void kit_render_external_text_configure_font(TTF_Font *font, int kerning_enabled) {
    if (!font) {
        return;
    }
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    TTF_SetFontKerning(font, kerning_enabled ? 1 : 0);
}

static void kit_render_external_text_font_cache_destroy_entry(
    KitRenderExternalTextFontCacheEntry *entry) {
    if (!entry) {
        return;
    }
    if (entry->font) {
        TTF_CloseFont(entry->font);
    }
    memset(entry, 0, sizeof(*entry));
}

static void kit_render_external_text_texture_cache_destroy_entry(
    KitRenderExternalTextTextureCacheEntry *entry) {
    if (!entry || !entry->valid) {
        return;
    }
    if (entry->renderer) {
        vk_renderer_queue_texture_destroy((VkRenderer *)entry->renderer, &entry->texture);
    }
    free(entry->text);
    memset(entry, 0, sizeof(*entry));
}

static KitRenderExternalTextFontCacheEntry *kit_render_external_text_font_cache_find_lru(void) {
    KitRenderExternalTextFontCacheEntry *oldest = NULL;
    int i = 0;
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_FONT_CACHE_CAPACITY; ++i) {
        KitRenderExternalTextFontCacheEntry *entry = &g_font_cache[i];
        if (!entry->font) {
            continue;
        }
        if (!oldest || entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }
    return oldest;
}

static KitRenderExternalTextTextureCacheEntry *kit_render_external_text_texture_cache_find_lru(void) {
    KitRenderExternalTextTextureCacheEntry *oldest = NULL;
    int i = 0;
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY; ++i) {
        KitRenderExternalTextTextureCacheEntry *entry = &g_texture_cache[i];
        if (!entry->valid) {
            continue;
        }
        if (!oldest || entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }
    return oldest;
}

static uint32_t kit_render_external_text_pack_color(SDL_Color color) {
    return ((uint32_t)color.r << 24) |
           ((uint32_t)color.g << 16) |
           ((uint32_t)color.b << 8) |
           (uint32_t)color.a;
}

static uint32_t kit_render_external_text_hash_bytes(const char *text, size_t len) {
    uint32_t h = 2166136261u;
    size_t i = 0;
    for (i = 0; i < len; ++i) {
        h ^= (uint8_t)text[i];
        h *= 16777619u;
    }
    return h;
}

static void kit_render_external_text_texture_cache_invalidate_font(TTF_Font *font) {
    int i = 0;
    if (!font) {
        return;
    }
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY; ++i) {
        if (g_texture_cache[i].valid && g_texture_cache[i].base_font == font) {
            kit_render_external_text_texture_cache_destroy_entry(&g_texture_cache[i]);
        }
    }
}

static TTF_Font *kit_render_external_text_get_font_for_point_size(TTF_Font *base_font,
                                                                  int point_size) {
    const KitRenderExternalTextFontSourceEntry *source = NULL;
    KitRenderExternalTextFontCacheEntry *slot = NULL;
    int i = 0;

    if (!base_font || point_size <= 0) {
        return NULL;
    }

    source = kit_render_external_text_find_font_source(base_font);
    if (!source || source->path[0] == '\0') {
        return base_font;
    }

    if (source->loaded_point_size == point_size) {
        return base_font;
    }

    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_FONT_CACHE_CAPACITY; ++i) {
        KitRenderExternalTextFontCacheEntry *entry = &g_font_cache[i];
        if (!entry->font) {
            if (!slot) {
                slot = entry;
            }
            continue;
        }
        if (entry->point_size == point_size &&
            entry->kerning_enabled == source->kerning_enabled &&
            strcmp(entry->path, source->path) == 0) {
            entry->last_used = g_font_cache_tick++;
            return entry->font;
        }
    }

    if (!slot) {
        slot = kit_render_external_text_font_cache_find_lru();
    }
    if (!slot) {
        return base_font;
    }
    kit_render_external_text_font_cache_destroy_entry(slot);

    slot->font = TTF_OpenFont(source->path, point_size);
    if (!slot->font) {
        return base_font;
    }
    kit_render_external_text_configure_font(slot->font, source->kerning_enabled);
    strncpy(slot->path, source->path, sizeof(slot->path) - 1);
    slot->path[sizeof(slot->path) - 1] = '\0';
    slot->point_size = point_size;
    slot->kerning_enabled = source->kerning_enabled;
    slot->last_used = g_font_cache_tick++;
    return slot->font;
}

static TTF_Font *kit_render_external_text_get_rasterized_font(TTF_Font *font,
                                                              SDL_Renderer *renderer,
                                                              float *out_raster_scale,
                                                              int *out_requested_point_size) {
    const KitRenderExternalTextFontSourceEntry *source = NULL;
    int requested_point_size = 0;
    float render_scale = 1.0f;

    if (out_raster_scale) {
        *out_raster_scale = 1.0f;
    }
    if (out_requested_point_size) {
        *out_requested_point_size = 0;
    }
    if (!font) {
        return NULL;
    }

    source = kit_render_external_text_find_font_source(font);
    if (!source || source->path[0] == '\0' || source->logical_point_size <= 0) {
        return font;
    }

    render_scale = kit_render_external_text_raster_scale(renderer);
    requested_point_size = (int)lroundf((float)source->logical_point_size * render_scale);
    if (requested_point_size < source->logical_point_size) {
        requested_point_size = source->logical_point_size;
    }

    if (out_raster_scale) {
        *out_raster_scale =
            (float)requested_point_size / (float)source->logical_point_size;
    }
    if (out_requested_point_size) {
        *out_requested_point_size = requested_point_size;
    }
    return kit_render_external_text_get_font_for_point_size(font, requested_point_size);
}

static TTF_Font *kit_render_external_text_get_measure_font(TTF_Font *font) {
    const KitRenderExternalTextFontSourceEntry *source = NULL;
    if (!font) {
        return NULL;
    }
    source = kit_render_external_text_find_font_source(font);
    if (!source || source->logical_point_size <= 0) {
        return font;
    }
    return kit_render_external_text_get_font_for_point_size(font, source->logical_point_size);
}

static int kit_render_external_text_measure_with_font(TTF_Font *font,
                                                      const char *text,
                                                      int *out_w,
                                                      int *out_h) {
    int width = 0;
    int height = 0;

    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!font || !text) {
        return 0;
    }

    if (text[0] == '\0') {
        height = TTF_FontHeight(font);
        if (height <= 0) {
            return 0;
        }
        if (out_h) *out_h = height;
        return 1;
    }

    if (TTF_SizeUTF8(font, text, &width, &height) != 0) {
        return 0;
    }
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    return 1;
}

static KitRenderExternalTextTextureCacheEntry *
kit_render_external_text_texture_cache_lookup(SDL_Renderer *renderer,
                                              TTF_Font *font,
                                              int requested_point_size,
                                              int wrap_width,
                                              const char *text,
                                              SDL_Color color) {
    size_t text_len = 0;
    uint32_t color_rgba = 0;
    uint32_t text_hash = 0;
    int i = 0;

    if (!renderer || !font || !text || text[0] == '\0') {
        return NULL;
    }
    text_len = strlen(text);
    if (text_len == 0 || text_len > KIT_RENDER_EXTERNAL_TEXT_MAX_TEXT_LEN) {
        return NULL;
    }
    color_rgba = kit_render_external_text_pack_color(color);
    text_hash = kit_render_external_text_hash_bytes(text, text_len);
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY; ++i) {
        KitRenderExternalTextTextureCacheEntry *entry = &g_texture_cache[i];
        if (!entry->valid) {
            continue;
        }
        if (entry->renderer == renderer &&
            entry->base_font == font &&
            entry->requested_point_size == requested_point_size &&
            entry->wrap_width == wrap_width &&
            entry->color_rgba == color_rgba &&
            entry->text_len == text_len &&
            entry->text_hash == text_hash &&
            entry->text &&
            memcmp(entry->text, text, text_len) == 0) {
            entry->last_used = g_texture_cache_tick++;
            return entry;
        }
    }
    return NULL;
}

static KitRenderExternalTextTextureCacheEntry *
kit_render_external_text_texture_cache_store(SDL_Renderer *renderer,
                                             TTF_Font *font,
                                             int requested_point_size,
                                             int wrap_width,
                                             const char *text,
                                             SDL_Color color,
                                             SDL_Surface *surface,
                                             float raster_scale,
                                             int draw_width,
                                             int draw_height) {
    KitRenderExternalTextTextureCacheEntry *slot = NULL;
    size_t text_len = 0;
    uint32_t color_rgba = 0;
    uint32_t text_hash = 0;
    int i = 0;

    if (!renderer || !font || !text || !surface || surface->w <= 0 || surface->h <= 0) {
        return NULL;
    }

    text_len = strlen(text);
    if (text_len == 0 || text_len > KIT_RENDER_EXTERNAL_TEXT_MAX_TEXT_LEN) {
        return NULL;
    }
    color_rgba = kit_render_external_text_pack_color(color);
    text_hash = kit_render_external_text_hash_bytes(text, text_len);

    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY; ++i) {
        if (!g_texture_cache[i].valid) {
            slot = &g_texture_cache[i];
            break;
        }
    }
    if (!slot) {
        slot = kit_render_external_text_texture_cache_find_lru();
    }
    if (!slot) {
        return NULL;
    }
    kit_render_external_text_texture_cache_destroy_entry(slot);

    slot->text = (char *)malloc(text_len + 1);
    if (!slot->text) {
        memset(slot, 0, sizeof(*slot));
        return NULL;
    }
    memcpy(slot->text, text, text_len + 1);
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surface,
                                                   &slot->texture,
                                                   kit_render_external_text_upload_filter_for_scale(
                                                       raster_scale)) != VK_SUCCESS) {
        free(slot->text);
        memset(slot, 0, sizeof(*slot));
        return NULL;
    }

    slot->valid = 1;
    slot->renderer = renderer;
    slot->base_font = font;
    slot->requested_point_size = requested_point_size;
    slot->wrap_width = wrap_width;
    slot->color_rgba = color_rgba;
    slot->text_len = text_len;
    slot->text_hash = text_hash;
    slot->texture_width = surface->w;
    slot->texture_height = surface->h;
    slot->draw_width = draw_width;
    slot->draw_height = draw_height;
    slot->raster_scale = raster_scale;
    slot->last_used = g_texture_cache_tick++;
    return slot;
}

static void kit_render_external_text_draw_cached_entry(SDL_Renderer *renderer,
                                                       const KitRenderExternalTextTextureCacheEntry *entry,
                                                       SDL_Rect *io_dst) {
    SDL_Rect dst = {0};
    SDL_Rect src = {0};

    if (!renderer || !entry || !entry->valid || !io_dst) {
        return;
    }

    dst = *io_dst;
    if (dst.w <= 0) {
        dst.w = entry->draw_width;
    }
    if (dst.h <= 0) {
        dst.h = entry->draw_height;
    }
    src = (SDL_Rect){0, 0, entry->texture_width, entry->texture_height};
    vk_renderer_draw_texture((VkRenderer *)renderer, &entry->texture, &src, &dst);
    *io_dst = dst;
}

void kit_render_external_text_register_font_source(TTF_Font *font,
                                                   const char *path,
                                                   int logical_point_size,
                                                   int loaded_point_size,
                                                   int kerning_enabled) {
    int i = 0;
    if (!font || !path || !path[0] || logical_point_size <= 0 || loaded_point_size <= 0) {
        return;
    }

    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_FONT_SOURCE_CAPACITY; ++i) {
        if (g_font_sources[i].font == font || !g_font_sources[i].font) {
            g_font_sources[i].font = font;
            strncpy(g_font_sources[i].path, path, sizeof(g_font_sources[i].path) - 1);
            g_font_sources[i].path[sizeof(g_font_sources[i].path) - 1] = '\0';
            g_font_sources[i].logical_point_size = logical_point_size;
            g_font_sources[i].loaded_point_size = loaded_point_size;
            g_font_sources[i].kerning_enabled = kerning_enabled ? 1 : 0;
            return;
        }
    }
}

void kit_render_external_text_unregister_font_source(TTF_Font *font) {
    int i = 0;
    if (!font) {
        return;
    }
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_FONT_SOURCE_CAPACITY; ++i) {
        if (g_font_sources[i].font == font) {
            kit_render_external_text_texture_cache_invalidate_font(font);
            memset(&g_font_sources[i], 0, sizeof(g_font_sources[i]));
            return;
        }
    }
}

void kit_render_external_text_reset_renderer(SDL_Renderer *renderer) {
    int i = 0;
    if (!renderer) {
        return;
    }
    for (i = 0; i < KIT_RENDER_EXTERNAL_TEXT_TEXTURE_CACHE_CAPACITY; ++i) {
        if (g_texture_cache[i].valid && g_texture_cache[i].renderer == renderer) {
            kit_render_external_text_texture_cache_destroy_entry(&g_texture_cache[i]);
        }
    }
}

int kit_render_external_text_measure_utf8(SDL_Renderer *renderer,
                                          TTF_Font *font,
                                          const char *text,
                                          int *out_w,
                                          int *out_h) {
    TTF_Font *measure_font = NULL;
    TTF_Font *raster_font = NULL;
    int raster_w = 0;
    int raster_h = 0;
    float raster_scale = 1.0f;

    (void)renderer;

    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!font || !text) {
        return 0;
    }

    measure_font = kit_render_external_text_get_measure_font(font);
    if (measure_font && kit_render_external_text_measure_with_font(measure_font, text, out_w, out_h)) {
        return 1;
    }

    raster_font = kit_render_external_text_get_rasterized_font(font, renderer, &raster_scale, NULL);
    if (!raster_font) {
        raster_font = font;
    }
    if (text[0] == '\0') {
        raster_h = TTF_FontHeight(raster_font);
        if (raster_h <= 0) {
            return 0;
        }
        if (out_h) *out_h = kit_render_external_text_logical_size_from_raster(raster_h, raster_scale);
        return 1;
    }
    if (TTF_SizeUTF8(raster_font, text, &raster_w, &raster_h) != 0) {
        return 0;
    }
    if (out_w) *out_w = kit_render_external_text_logical_size_from_raster(raster_w, raster_scale);
    if (out_h) *out_h = kit_render_external_text_logical_size_from_raster(raster_h, raster_scale);
    return 1;
}

int kit_render_external_text_draw_utf8(SDL_Renderer *renderer,
                                       TTF_Font *font,
                                       const char *text,
                                       SDL_Color color,
                                       SDL_Rect *io_dst) {
    TTF_Font *raster_font = NULL;
    KitRenderExternalTextTextureCacheEntry *cached = NULL;
    SDL_Surface *surface = NULL;
    VkRendererTexture texture = {0};
    SDL_Rect dst = {0};
    SDL_Rect src = {0};
    float raster_scale = 1.0f;
    int requested_point_size = 0;
    int logical_width = 0;
    int logical_height = 0;

    if (!renderer || !font || !text || !text[0] || !io_dst) {
        return 0;
    }

    raster_font = kit_render_external_text_get_rasterized_font(font,
                                                               renderer,
                                                               &raster_scale,
                                                               &requested_point_size);
    if (!raster_font) {
        raster_font = font;
    }

    cached = kit_render_external_text_texture_cache_lookup(renderer,
                                                           font,
                                                           requested_point_size,
                                                           0,
                                                           text,
                                                           color);
    if (cached) {
        dst = *io_dst;
        kit_render_external_text_draw_cached_entry(renderer, cached, &dst);
        *io_dst = dst;
        return 1;
    }

    surface = TTF_RenderUTF8_Blended(raster_font, text, color);
    if (!surface) {
        return 0;
    }

    if (!kit_render_external_text_measure_utf8(renderer, font, text, &logical_width, &logical_height)) {
        logical_width = kit_render_external_text_logical_size_from_raster(surface->w, raster_scale);
        logical_height = kit_render_external_text_logical_size_from_raster(surface->h, raster_scale);
    }

    dst = *io_dst;
    if (dst.w <= 0) {
        dst.w = logical_width;
    }
    if (dst.h <= 0) {
        dst.h = logical_height;
    }
    if (dst.w < 1) {
        dst.w = 1;
    }
    if (dst.h < 1) {
        dst.h = 1;
    }

    cached = kit_render_external_text_texture_cache_store(renderer,
                                                          font,
                                                          requested_point_size,
                                                          0,
                                                          text,
                                                          color,
                                                          surface,
                                                          raster_scale,
                                                          dst.w,
                                                          dst.h);
    if (cached) {
        kit_render_external_text_draw_cached_entry(renderer, cached, &dst);
        SDL_FreeSurface(surface);
        *io_dst = dst;
        return 1;
    }

    src = (SDL_Rect){0, 0, surface->w, surface->h};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surface,
                                                   &texture,
                                                   kit_render_external_text_upload_filter_for_scale(
                                                       raster_scale)) != VK_SUCCESS) {
        SDL_FreeSurface(surface);
        return 0;
    }
    vk_renderer_draw_texture((VkRenderer *)renderer, &texture, &src, &dst);
    vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &texture);
    SDL_FreeSurface(surface);
    *io_dst = dst;
    return 1;
}

int kit_render_external_text_draw_utf8_at(SDL_Renderer *renderer,
                                          TTF_Font *font,
                                          const char *text,
                                          int x,
                                          int y,
                                          SDL_Color color) {
    SDL_Rect dst = {x, y, 0, 0};
    return kit_render_external_text_draw_utf8(renderer, font, text, color, &dst);
}

int kit_render_external_text_draw_utf8_wrapped(SDL_Renderer *renderer,
                                               TTF_Font *font,
                                               const char *text,
                                               int wrap_width,
                                               SDL_Color color,
                                               SDL_Rect *io_dst) {
    TTF_Font *raster_font = NULL;
    KitRenderExternalTextTextureCacheEntry *cached = NULL;
    SDL_Surface *surface = NULL;
    VkRendererTexture texture = {0};
    SDL_Rect dst = {0};
    SDL_Rect src = {0};
    float raster_scale = 1.0f;
    int requested_point_size = 0;
    int raster_wrap_width = 0;
    int logical_width = 0;
    int logical_height = 0;

    if (!renderer || !font || !text || !text[0] || !io_dst) {
        return 0;
    }
    if (wrap_width < 1) {
        wrap_width = 1;
    }

    raster_font = kit_render_external_text_get_rasterized_font(font,
                                                               renderer,
                                                               &raster_scale,
                                                               &requested_point_size);
    if (!raster_font) {
        raster_font = font;
    }

    cached = kit_render_external_text_texture_cache_lookup(renderer,
                                                           font,
                                                           requested_point_size,
                                                           wrap_width,
                                                           text,
                                                           color);
    if (cached) {
        dst = *io_dst;
        kit_render_external_text_draw_cached_entry(renderer, cached, &dst);
        *io_dst = dst;
        return 1;
    }

    raster_wrap_width = (int)lroundf((float)wrap_width * raster_scale);
    if (raster_wrap_width < 1) {
        raster_wrap_width = 1;
    }

    surface = TTF_RenderUTF8_Blended_Wrapped(raster_font, text, color, (Uint32)raster_wrap_width);
    if (!surface) {
        return 0;
    }

    logical_width = kit_render_external_text_logical_size_from_raster(surface->w, raster_scale);
    logical_height = kit_render_external_text_logical_size_from_raster(surface->h, raster_scale);

    dst = *io_dst;
    if (dst.w <= 0) {
        dst.w = logical_width;
    }
    if (dst.h <= 0) {
        dst.h = logical_height;
    }
    if (dst.w < 1) {
        dst.w = 1;
    }
    if (dst.h < 1) {
        dst.h = 1;
    }

    cached = kit_render_external_text_texture_cache_store(renderer,
                                                          font,
                                                          requested_point_size,
                                                          wrap_width,
                                                          text,
                                                          color,
                                                          surface,
                                                          raster_scale,
                                                          dst.w,
                                                          dst.h);
    if (cached) {
        kit_render_external_text_draw_cached_entry(renderer, cached, &dst);
        SDL_FreeSurface(surface);
        *io_dst = dst;
        return 1;
    }

    src = (SDL_Rect){0, 0, surface->w, surface->h};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surface,
                                                   &texture,
                                                   kit_render_external_text_upload_filter_for_scale(
                                                       raster_scale)) != VK_SUCCESS) {
        SDL_FreeSurface(surface);
        return 0;
    }
    vk_renderer_draw_texture((VkRenderer *)renderer, &texture, &src, &dst);
    vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &texture);
    SDL_FreeSurface(surface);
    *io_dst = dst;
    return 1;
}
