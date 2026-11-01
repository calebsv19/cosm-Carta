#include "ui/font.h"
#include "ui/font_bridge.h"
#include "ui/text_draw.h"
#include "ui/shared_theme_font_adapter.h"

#include <SDL2/SDL_ttf.h>

bool ui_font_set(const char *path, int base_point_size) {
    bool ok = map_forge_font_bridge_set_active(path, base_point_size);
    if (ok) {
        MapForgeFontRuntimeHooks hooks = {ui_font_set, map_forge_font_bridge_preview_begin,
                                         map_forge_font_bridge_preview_end};
        mapforge_shared_font_bind_runtime(&hooks);
    }
    return ok;
}

void ui_font_shutdown(Renderer *renderer) {
    mapforge_shared_font_bind_runtime(NULL);
    map_forge_font_bridge_shutdown(renderer);
}

void ui_font_invalidate_cache(Renderer *renderer) {
    map_forge_font_bridge_reset_renderer(renderer);
}

void ui_draw_text(Renderer *renderer, int x, int y, const char *text, SDL_Color color, float scale) {
    MapForgeResolvedFont resolved = {0};
    SDL_Rect dst = {x, y, 0, 0};

    if (!renderer || !text) {
        return;
    }
    if (!map_forge_font_bridge_acquire(scale, &resolved)) {
        return;
    }
    map_forge_text_draw_utf8(renderer, resolved.font, text, color, &dst);
}

int ui_measure_text_width(const char *text, float scale) {
    MapForgeResolvedFont resolved = {0};
    int width = 0;
    int height = 0;

    if (!text) {
        return 0;
    }
    if (!map_forge_font_bridge_acquire(scale, &resolved)) {
        return 0;
    }
    if (!map_forge_text_measure_utf8(NULL, resolved.font, text, &width, &height)) {
        return 0;
    }
    return width;
}

void ui_draw_text_clipped(Renderer *renderer,
                          int x,
                          int y,
                          const char *text,
                          SDL_Color color,
                          float scale,
                          int max_width) {
    MapForgeResolvedFont resolved = {0};
    SDL_Rect dst = {x, y, 0, 0};

    if (!renderer || !text || max_width <= 0) {
        return;
    }
    if (!map_forge_font_bridge_acquire(scale, &resolved)) {
        return;
    }
    map_forge_text_draw_utf8_clipped(renderer, resolved.font, text, color, &dst, max_width);
}

int ui_font_line_height(float scale) {
    MapForgeResolvedFont resolved = {0};

    if (!map_forge_font_bridge_acquire(scale, &resolved)) {
        return 0;
    }
    int height = 0;
    return map_forge_text_measure_utf8(NULL, resolved.font, "", NULL, &height) ? height : 0;
}
