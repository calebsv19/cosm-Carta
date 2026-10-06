#include "app/app_internal.h"
#include "app/app_utility_workspace.h"
#include "app/terrain.h"
#include "map/terrain_pack.h"
#include "map/mercator.h"
#include "render/image.h"
#include "ui/font.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int requested, draws, expected = 3;
static bool refuse;
// Supplies distinct overview/detail footprints so incorrect raster lookup cannot pass silently.
bool terrain_pack_open(TerrainPack *p, const char *path, char *e, size_t n) {
    (void)path;
    if (refuse) {
        snprintf(e, n, "refused test replacement");
        return false;
    }
    p->db = (sqlite3 *)1;
    p->bytes = 4096;
    p->style_count = 4;
    for (int s = 0; s < 4; s++) {
        TerrainStyle *t = &p->styles[s];
        double b = s < 2 ? 1000 : 500;
        t->extent[0] = t->extent[1] = -b;
        t->extent[2] = t->extent[3] = b;
        t->mode = s % 2;
        snprintf(t->area, 64, "%s", s < 2 ? "Overview" : "detail");
        for (int l = 0; l < 5; l++) {
            int w = (s < 2 ? 256 : 512) >> l;
            t->levels[l] = (TerrainLevel){w, w, (w + 255) / 256, (w + 255) / 256};
        }
    }
    return true;
}
void terrain_pack_close(TerrainPack *p) { memset(p, 0, sizeof(*p)); }
int terrain_pack_select(const TerrainPack *p, int mode, double x0, double y0, double x1, double y1) {
    (void)p;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    return mode + 2;
}
// Refuses overview texels when geometry comes from the local detail raster.
bool terrain_pack_tile(TerrainPack *p, int style, int lod, int col, int row, unsigned char *rgba) {
    (void)p;
    assert(style == expected && (lod == 0 || lod == 4) && col >= 0 && col < 2 && row >= 0 && row < 2);
    memset(rgba, style, TERRAIN_RGBA_BYTES);
    requested++;
    return true;
}
const char *region_data_root(void) { return "/test-only"; }
bool camera_visible_world_aabb(const Camera *c, int w, int h, float *x0, float *y0, float *x1, float *y1) {
    (void)c;
    (void)w;
    (void)h;
    *x0 = *y0 = -100;
    *x1 = *y1 = 100;
    return true;
}
float camera_pixels_per_meter(const Camera *c) {
    (void)c;
    return 1;
}
bool app_map_world_to_viewport_local(const AppState *a, float x, float y, float *sx, float *sy) {
    (void)a;
    *sx = x + 100;
    *sy = 100 - y;
    return true;
}
SDL_FRect app_map_viewport_rect(const AppState *a) {
    (void)a;
    return (SDL_FRect){0, 0, 200, 200};
}
bool renderer_image_ready(Renderer *r, RendererImage *i) {
    (void)r;
    return i->gpu != NULL;
}
bool renderer_image_upload(Renderer *r, RendererImage *i, const unsigned char *rgba, int w, int h) {
    (void)r;
    assert(rgba[0] == expected && w == 258 && h == 258);
    i->gpu = (void *)1;
    i->gpu_bytes = TERRAIN_RGBA_BYTES;
    return true;
}
bool renderer_image_draw(Renderer *r, RendererImage *i, const SDL_FPoint p[4]) {
    (void)r;
    if (!i->gpu)
        return false;
    assert((p[1].x - p[0].x == 500 || p[1].x - p[0].x == 8000) &&
           p[1].x - p[0].x == p[3].y - p[0].y);
    draws++;
    return true;
}
void renderer_image_release(Renderer *r, RendererImage *i) {
    (void)r;
    memset(i, 0, sizeof(*i));
}
void renderer_set_draw_color(Renderer *r, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    (void)r;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
}
void renderer_fill_rect(Renderer *r, const SDL_FRect *p) {
    (void)r;
    (void)p;
}
void ui_draw_text_clipped(Renderer *r, int x, int y, const char *s, SDL_Color c, float z, int w) {
    (void)r;
    (void)x;
    (void)y;
    (void)s;
    (void)c;
    (void)z;
    (void)w;
}
// Exercises production cache lookup, matched texel footprint, failed reload and Off behavior.
int main(void) {
    assert(!setenv("MAPFORGE_TERRAIN_PATH", "/test-only/terrain.sqlite", 1));
    AppState *a = calloc(1, sizeof(*a));
    assert(a);
    a->ui_state_bridge.utility_workspace = APP_UTILITY_HIKING_HISTORY;
    a->renderer.width = a->renderer.height = 200;
    terrain_set_style(1);
    for (int i = 0; i < 100; i++) {
        terrain_draw(a); SDL_Delay(1);
    }
    assert(requested == 5 && draws > 0 && !a->terrain_pending);
    refuse = true;
    assert(!terrain_reload(a));
    assert(terrain_active(a));
    terrain_draw(a);
    assert(requested == 5);
    terrain_credit_draw(a);
    terrain_set_style(2);
    int before = draws;
    terrain_draw(a);
    assert(draws == before && !a->terrain_pending);
    terrain_shutdown(a);
    free(a);
    puts("terrain native policy: detail texels match projected footprint; cache/reload/off passed");
}

// Keeps raster policy proof independent of the optional vector cache.
void terrain_vectors_shutdown(void) {}

// Keeps coverage formatting independent of map projection in this renderer policy fixture.
MercatorMeters mercator_from_latlon(LatLon p) { return (MercatorMeters){p.lon,p.lat}; }
