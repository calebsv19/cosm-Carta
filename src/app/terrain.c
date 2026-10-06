#include "app/terrain.h"
#include "app/terrain_vectors.h"
#include "app/terrain_decode.h"
#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_utility_workspace.h"
#include "map/terrain_pack.h"
#include "map/terrain_catalog.h"
#include "map/mercator.h"
#include "render/image.h"
#include "ui/font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TERRAIN_CACHE 48
// Keeps one bounded cache key and its decoded source for context recovery.
typedef struct TerrainTile {
    int area, style, lod, col, row;
    double x, y, width, height;
    unsigned long used;
    bool occupied, failed, queued;
    unsigned char *rgba;
    RendererImage image;
} TerrainTile;
// Owns app-local geographic policy without touching the personal database.
static struct {
    TerrainPack pack;
    TerrainCatalog catalog;
    int area, failed_area, raster, previous_area, previous_raster;
    TerrainDecode *decoder;
    TerrainPresentationStats presentation;
    bool was_pending;
    bool visible, override;
    TerrainTile tiles[TERRAIN_CACHE];
    int style;
    bool attempted;
    unsigned long frame;
    int loading, drawn, errors;
    char status[160], reload_error[160];
} terrain = {.style = 1, .area = -1, .failed_area = -1, .previous_area = -1};
bool terrain_active(AppState *app) {
    return app && app->ui_state_bridge.utility_workspace == APP_UTILITY_HIKING_HISTORY &&
           terrain.style != 2 && terrain.pack.db && terrain.visible;
}
int terrain_style(void) { return terrain.style; }
void terrain_set_style(int s) {
    if (s >= 0 && s <= 2)
        terrain.style = s;
}
int terrain_render_mode(void) {
    return terrain.area >= 0 && terrain.catalog.areas[terrain.area].removable && terrain.style != 2
               ? 0 : terrain.style;
}
const char *terrain_style_label(void) {
    int mode = terrain_render_mode();
    return mode == 0 ? "Terrain: Relief" : mode == 1 ? "Terrain: Topo" : "Terrain: Off";
}
void terrain_cycle(void) { terrain.style = terrain.style == 1 ? 0 : terrain.style == 0 ? 2 : 1; }
// Clears owned tiles only at reload/shutdown or through safe cache retirement.
static void clear(AppState *app) {
    for (int i = 0; i < TERRAIN_CACHE; i++) {
        TerrainTile *t = &terrain.tiles[i];
        renderer_image_release(&app->renderer, &t->image);
        free(t->rgba);
        memset(t, 0, sizeof(*t));
    }
}
// Changes the admitted source while retaining bounded geographically keyed warm textures.
static bool open_area(AppState *app, int area) {
    (void)app;
    if (area < 0 || area >= (int)terrain.catalog.count) return false;
    TerrainArea *entry = &terrain.catalog.areas[area];
    TerrainPack next = {0};
    if (!terrain_pack_open(&next, entry->raster_path, terrain.reload_error, sizeof(terrain.reload_error))) {
        terrain.failed_area = area;
        return false;
    }
    if (entry->removable) {
        for (int i = 0; i < 4; i++) if (fabs(next.styles[0].extent[i] - entry->extent[i]) > 1) {
            terrain_pack_close(&next);
            snprintf(terrain.reload_error, sizeof(terrain.reload_error), "Terrain bounds differ from catalog; Reload after repair");
            terrain.failed_area = area;
            return false;
        }
    }
    if (!terrain.decoder) terrain.decoder = terrain_decode_create();
    if (!terrain.decoder) {
        terrain_pack_close(&next); terrain.failed_area = area;
        snprintf(terrain.reload_error, sizeof(terrain.reload_error), "Terrain decoder unavailable; Reload to retry");
        return false;
    }
    terrain_vectors_shutdown();
    terrain_decode_source(terrain.decoder, entry->raster_path, &next);
    for (int i = 0; i < TERRAIN_CACHE; i++) terrain.tiles[i].queued = false;
    terrain.previous_area = terrain.area; terrain.previous_raster = terrain.raster;
    terrain_pack_close(&terrain.pack);
    terrain.pack = next;
    terrain.area = area;
    terrain.failed_area = -1;
    terrain.visible = true;
    terrain.reload_error[0] = 0;
    SDL_Log("[terrain] area=%s verified pack bytes=%zu cache_limit=%d", entry->id, next.bytes, TERRAIN_CACHE);
    return true;
}
// Resolves the mutable patch cache separately from read-only region packs and personal history.
static void storage_root(char *out, size_t cap) {
    const char *override = getenv("MAPFORGE_TERRAIN_AREAS_DIR");
    const char *runtime = getenv("MAPFORGE_RUNTIME_DIR");
    if (override) snprintf(out, cap, "%s", override);
    else snprintf(out, cap, "%s/terrain", runtime && *runtime ? runtime : "data/runtime");
}
bool terrain_reload(AppState *app) {
    terrain.attempted = true;
    const char *override = getenv("MAPFORGE_TERRAIN_PATH");
    if (override && *override) {
        terrain.override = true;
        terrain.catalog.count = 1;
        TerrainArea *entry = &terrain.catalog.areas[0];
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->id, sizeof(entry->id), "override");
        snprintf(entry->name, sizeof(entry->name), "Terrain override");
        snprintf(entry->raster_path, sizeof(entry->raster_path), "%s", override);
        const char *vector = getenv("MAPFORGE_TERRAIN_VECTORS_PATH");
        snprintf(entry->vector_path, sizeof(entry->vector_path), "%s", vector ? vector : "");
        bool opened = open_area(app, 0);
        if (opened) clear(app);
        return opened;
    }
    char root[4096]; storage_root(root, sizeof(root));
    TerrainCatalog *next = calloc(1, sizeof(*next));
    if (!next) return false;
    bool ok = terrain_catalog_load(next, region_data_root(), root, false,
                                   terrain.reload_error, sizeof(terrain.reload_error));
    if (!ok) {
        if (!terrain.catalog.count) terrain_catalog_load(&terrain.catalog, region_data_root(), root, true, NULL, 0);
        free(next);
        return false;
    }
    terrain_vectors_shutdown();
    terrain_decode_destroy(terrain.decoder); terrain.decoder = NULL;
    clear(app);
    terrain_pack_close(&terrain.pack);
    terrain.catalog = *next;
    free(next);
    terrain.area = terrain.failed_area = terrain.previous_area = -1;
    terrain.visible = false;
    terrain.override = false;
    snprintf(terrain.status, sizeof(terrain.status), "Terrain areas loaded");
    SDL_Log("[terrain catalog] areas=%zu additional_bytes=%zu limit=%u", terrain.catalog.count,
            terrain.catalog.additional_bytes, TERRAIN_ADDITIONAL_BUDGET);
    return true;
}
const TerrainCatalog *terrain_area_catalog(void) { return &terrain.catalog; }
const char *terrain_vector_path(void) {
    return terrain.visible && terrain.area >= 0 ? terrain.catalog.areas[terrain.area].vector_path : NULL;
}
// Removes a user-confirmed cache patch only after releasing all owned raster/vector resources.
bool terrain_remove_area(AppState *app, size_t index, char *error, size_t cap) {
    if (terrain.override || index >= terrain.catalog.count || !terrain.catalog.areas[index].removable) {
        if (error && cap) snprintf(error, cap, "The baseline overview is protected");
        return false;
    }
    terrain_vectors_shutdown();
    terrain_decode_destroy(terrain.decoder); terrain.decoder = NULL;
    clear(app); terrain_pack_close(&terrain.pack);
    terrain.area = terrain.failed_area = terrain.previous_area = -1; terrain.visible = false;
    bool ok = terrain_catalog_remove(&terrain.catalog, index, error, cap);
    if (!ok) terrain_reload(app);
    return ok;
}
// Reports full, partial or missing background independently of saved route geometry/status.
void terrain_coverage_text(double min_lat, double min_lon, double max_lat, double max_lon, char *out, size_t cap) {
    MercatorMeters a = mercator_from_latlon((LatLon){min_lat,min_lon});
    MercatorMeters b = mercator_from_latlon((LatLon){max_lat,max_lon});
    int i = terrain_catalog_select(&terrain.catalog, a.x-1, a.y-1, b.x+1, b.y+1);
    if (i < 0) { snprintf(out, cap, "Background unavailable · route remains visible"); return; }
    const TerrainArea *area = &terrain.catalog.areas[i];
    bool full = a.x >= area->extent[0] && a.y >= area->extent[1] && b.x <= area->extent[2] && b.y <= area->extent[3];
    snprintf(out, cap, "%s: %s · %.1f MiB", full ? "Background" : "Partial background", area->name, area->bytes/1048576.0);
}
bool terrain_covers(double min_lat,double min_lon,double max_lat,double max_lon) {
    MercatorMeters a = mercator_from_latlon((LatLon){min_lat,min_lon});
    MercatorMeters b = mercator_from_latlon((LatLon){max_lat,max_lon});
    return terrain_catalog_select(&terrain.catalog,a.x-1,a.y-1,b.x+1,b.y+1) >= 0;
}
void terrain_shutdown(AppState *app) {
    terrain_vectors_shutdown();
    terrain_decode_destroy(terrain.decoder); terrain.decoder = NULL;
    if (app) clear(app);
    terrain_pack_close(&terrain.pack);
    memset(&terrain.catalog, 0, sizeof(terrain.catalog));
    terrain.attempted = terrain.visible = terrain.override = false;
    terrain.area = terrain.failed_area = terrain.previous_area = -1;
}
// Computes a raster tile's exact Mercator footprint without rounded-resolution drift.
static void corners(AppState *app, const TerrainTile *t, SDL_FPoint out[4]) {
    for (int j = 0; j < 4; j++)
        app_map_world_to_viewport_local(app, (float)(t->x + (j == 1 || j == 2 ? t->width : 0)),
                                        (float)(t->y - (j >= 2 ? t->height : 0)), &out[j].x, &out[j].y);
}
// Rejects offscreen cache tiles before drawing fallback levels.
static bool onscreen(AppState *app, TerrainTile *t, SDL_FPoint p[4]) {
    corners(app, t, p);
    float minx = p[0].x, maxx = minx, miny = p[0].y, maxy = miny;
    for (int j = 1; j < 4; j++) {
        minx = fminf(minx, p[j].x);
        maxx = fmaxf(maxx, p[j].x);
        miny = fminf(miny, p[j].y);
        maxy = fmaxf(maxy, p[j].y);
    }
    return minx < app->renderer.width && maxx > 0 && miny < app->renderer.height && maxy > 0;
}
// Protects the current backdrop and the previous area's warm return within the same 48 slots.
static bool pinned(const TerrainTile *t) {
    return t->occupied && t->lod == 4 &&
        ((t->area == terrain.area && t->style == terrain.raster) ||
         (t->area == terrain.previous_area && t->style == terrain.previous_raster));
}
// Finds a geographic key without confusing textures from another area's raster numbering.
static TerrainTile *find(int style, int lod, int col, int row) {
    for (int i = 0; i < TERRAIN_CACHE; i++) {
        TerrainTile *t = &terrain.tiles[i];
        if (t->occupied && t->area == terrain.area && t->style == style &&
            t->lod == lod && t->col == col && t->row == row) return t;
    }
    return NULL;
}
// Retires only older unrequested detail, preserving the coarse foundation during refinement.
static TerrainTile *slot(AppState *app, int style, int lod, int col, int row) {
    TerrainTile *t = find(style, lod, col, row);
    if (t) return t;
    TerrainTile *old = NULL;
    for (int i = 0; i < TERRAIN_CACHE; i++) {
        t = &terrain.tiles[i];
        if (!t->occupied) { old = t; break; }
        if (!t->queued && !pinned(t) && t->used + 2 < terrain.frame &&
            (!old || t->used < old->used)) old = t;
    }
    if (!old) return NULL;
    renderer_image_release(&app->renderer, &old->image); free(old->rgba);
    TerrainStyle *s = &terrain.pack.styles[style]; TerrainLevel *l = &s->levels[lod];
    double dx = (s->extent[2] - s->extent[0]) / l->width * 256;
    double dy = (s->extent[3] - s->extent[1]) / l->height * 256;
    *old = (TerrainTile){.area=terrain.area, .style=style, .lod=lod, .col=col, .row=row,
        .occupied=true, .x=s->extent[0]+col*dx, .y=s->extent[3]-row*dy, .width=dx, .height=dy};
    return old;
}
// Transfers checked current-source buffers without decoding or touching SQLite on this thread.
static void collect(void) {
    TerrainDecodeResult result;
    while (terrain_decode_take(terrain.decoder, &result)) {
        TerrainDecodeKey k = result.key;
        TerrainTile *t = find(k.style, k.lod, k.col, k.row);
        if (!t || !t->queued) { free(result.rgba); continue; }
        t->queued = false; t->failed = !result.ok; t->rgba = result.rgba;
    }
}
// Enqueues missing texels and limits GPU uploads on the render owner to two per frame.
static TerrainTile *prepare(AppState *app, int style, int lod, int col, int row,
                            unsigned *uploads, Uint64 start) {
    TerrainTile *t = slot(app, style, lod, col, row);
    if (!t) return NULL;
    t->used = terrain.frame;
    if (t->failed) return t;
    if (!t->rgba) {
        if (!t->queued) t->queued = terrain_decode_request(terrain.decoder,
            (TerrainDecodeKey){style, lod, col, row});
        return t;
    }
    if (!renderer_image_ready(&app->renderer, &t->image) && *uploads < 2 &&
        (!*uploads || (SDL_GetPerformanceCounter()-start)/(double)SDL_GetPerformanceFrequency() < .003)) {
        (*uploads)++;
        renderer_image_release(&app->renderer, &t->image);
        if (!renderer_image_upload(&app->renderer, &t->image, t->rgba, 258, 258)) t->failed = true;
        size_t gpu_bytes = 0;
        for (int i = 0; i < TERRAIN_CACHE; i++) gpu_bytes += terrain.tiles[i].image.gpu_bytes;
        if (gpu_bytes > 32u * 1024u * 1024u) {
            renderer_image_release(&app->renderer, &t->image); t->failed = true;
        }
    }
    return t;
}
TerrainPresentationStats terrain_presentation_stats(void) { return terrain.presentation; }
void terrain_draw(AppState *app) {
    app->terrain_pending = false;
    memset(&terrain.presentation, 0, sizeof(terrain.presentation));
    terrain.presentation.area = -1;
    if (app->ui_state_bridge.utility_workspace != APP_UTILITY_HIKING_HISTORY)
        return;
    if (!terrain.attempted)
        terrain_reload(app);
    if (terrain.style == 2) return;
    Camera *camera = &app->view_state_bridge.camera;
    float xmin, ymin, xmax, ymax;
    if (!camera_visible_world_aabb(camera, app->renderer.width, app->renderer.height, &xmin, &ymin, &xmax,
                                   &ymax))
        return;
    int area = terrain.override ? 0 : terrain_catalog_select(&terrain.catalog, xmin, ymin, xmax, ymax);
    terrain.visible = false;
    if (area < 0) {
        snprintf(terrain.status, sizeof(terrain.status), "Background unavailable here · saved routes remain visible");
        return;
    }
    if (area != terrain.area && area != terrain.failed_area) open_area(app, area);
    if (area != terrain.area || !terrain.pack.db) return;
    terrain.visible = true;
    terrain.frame++;
    terrain.loading = terrain.drawn = terrain.errors = 0;
    int mode = terrain_render_mode();
    int raster = terrain_pack_select(&terrain.pack, mode, xmin, ymin, xmax, ymax);
    terrain.raster = raster;
    TerrainStyle *s = &terrain.pack.styles[raster];
    int lod = 0;
    double ppm = camera_pixels_per_meter(camera);
    while (lod < 4 && (s->extent[2] - s->extent[0]) / s->levels[lod].width * ppm < .8)
        lod++;
    TerrainLevel *l = &s->levels[lod];
    double dx = (s->extent[2] - s->extent[0]) / l->width * 256,
           dy = (s->extent[3] - s->extent[1]) / l->height * 256;
    int c0 = fmax(0, floor((xmin - s->extent[0]) / dx)),
        c1 = fmin(l->cols - 1, floor((xmax - s->extent[0]) / dx)),
        r0 = fmax(0, floor((s->extent[3] - ymax) / dy)),
        r1 = fmin(l->rows - 1, floor((s->extent[3] - ymin) / dy));
    // Keep even wide/rotated viewports within cache capacity before admitting new keys.
    while (lod < 4 && (c1 - c0 + 1) * (r1 - r0 + 1) > 32) {
        l = &s->levels[++lod];
        dx = (s->extent[2] - s->extent[0]) / l->width * 256;
        dy = (s->extent[3] - s->extent[1]) / l->height * 256;
        c0 = fmax(0, floor((xmin - s->extent[0]) / dx));
        c1 = fmin(l->cols - 1, floor((xmax - s->extent[0]) / dx));
        r0 = fmax(0, floor((s->extent[3] - ymax) / dy));
        r1 = fmin(l->rows - 1, floor((s->extent[3] - ymin) / dy));
    }
    collect();
    unsigned uploads = 0;
    Uint64 start = SDL_GetPerformanceCounter();
    TerrainLevel *coarse = &s->levels[4];
    int coarse_processed = 0;
    // The complete lowest level is at most four tiles and stays underneath every detail level.
    for (int row = 0; row < coarse->rows; row++) for (int col = 0; col < coarse->cols; col++) {
        TerrainTile *t = prepare(app, raster, 4, col, row, &uploads, start);
        terrain.presentation.coarse_needed++;
        if (t && (t->failed || renderer_image_ready(&app->renderer, &t->image))) coarse_processed++;
        if (t && !t->failed && renderer_image_ready(&app->renderer, &t->image))
            terrain.presentation.coarse_ready++;
    }
    for (int row = r0; row <= r1; row++) for (int col = c0; col <= c1; col++) {
        unsigned blocked = 2;
        TerrainTile *t = prepare(app, raster, lod, col, row,
            coarse_processed == terrain.presentation.coarse_needed ? &uploads : &blocked, start);
        terrain.presentation.detail_needed++;
        if (t && t->failed) terrain.errors++;
        else if (!t || !renderer_image_ready(&app->renderer, &t->image)) terrain.loading++;
        else terrain.presentation.detail_ready++;
    }
    for (int row = 0; row < coarse->rows; row++) for (int col = 0; col < coarse->cols; col++) {
        TerrainTile *t = find(raster, 4, col, row);
        if (t && t->failed) terrain.errors++;
        else if (!t || !renderer_image_ready(&app->renderer, &t->image)) terrain.loading++;
    }
    // Coarser/finer cached tiles remain underneath incoming level tiles during zoom.
    for (int fallback_lod = 4; terrain.presentation.detail_ready < terrain.presentation.detail_needed && fallback_lod >= 0; fallback_lod--)
        for (int i = 0; i < TERRAIN_CACHE; i++) {
            TerrainTile *t = &terrain.tiles[i];
            SDL_FPoint p[4];
            if (t->occupied && t->area == terrain.area && !t->failed &&
                terrain.pack.styles[t->style].mode == mode &&
                (t->style != raster || t->lod != lod) && t->lod == fallback_lod && onscreen(app, t, p))
                renderer_image_draw(&app->renderer, &t->image, p);
        }
    for (int row = r0; row <= r1; row++)
        for (int col = c0; col <= c1; col++)
            for (int i = 0; i < TERRAIN_CACHE; i++) {
                TerrainTile *t = &terrain.tiles[i];
                if (t->occupied && t->area == terrain.area && !t->failed && t->style == raster && t->lod == lod && t->col == col &&
                    t->row == row) {
                    SDL_FPoint p[4];
                    corners(app, t, p);
                    if (renderer_image_draw(&app->renderer, &t->image, p))
                        terrain.drawn++;
                    else if (!t->failed)
                        terrain.loading++;
                    break;
                }
            }

    app->terrain_pending = terrain.loading > 0;
    terrain.presentation.area = terrain.area;
    terrain.presentation.pending = app->terrain_pending;
    terrain.presentation.worker_slots = terrain_decode_stats(terrain.decoder).occupied;
    for (int i = 0; i < TERRAIN_CACHE; i++) {
        terrain.presentation.resident_tiles += terrain.tiles[i].occupied;
        terrain.presentation.rgba_bytes += terrain.tiles[i].rgba ? TERRAIN_RGBA_BYTES : 0;
        terrain.presentation.gpu_bytes += terrain.tiles[i].image.gpu_bytes;
    }
    if (terrain.was_pending != app->terrain_pending || terrain.frame == 1) {
        SDL_Log("[terrain presentation] area=%s coarse=%d/%d detail=%d/%d pending=%d resident=%zu gpu=%zu worker=%zu",
            terrain.catalog.areas[terrain.area].id, terrain.presentation.coarse_ready,
            terrain.presentation.coarse_needed, terrain.presentation.detail_ready,
            terrain.presentation.detail_needed, app->terrain_pending, terrain.presentation.resident_tiles,
            terrain.presentation.gpu_bytes, terrain.presentation.worker_slots);
    }
    terrain.was_pending = app->terrain_pending;
    if (terrain.frame % 300 == 0) {
        size_t raw = 0, gpu = 0;
        int entries = 0;
        for (int i = 0; i < TERRAIN_CACHE; i++) {
            TerrainTile *t = &terrain.tiles[i];
            entries += t->occupied;
            raw += t->rgba ? TERRAIN_RGBA_BYTES : 0;
            gpu += t->image.gpu_bytes;
        }
        SDL_Log("[terrain] residency entries=%d cpu_rgba=%zu gpu_allocation=%zu visible=%d pending=%d",
                entries, raw, gpu, terrain.drawn, terrain.loading);
    }
    if (c0 > c1 || r0 > r1)
        snprintf(terrain.status, sizeof(terrain.status), "Outside downloaded terrain coverage");
    else if (terrain.errors)
        snprintf(terrain.status, sizeof(terrain.status), "Terrain tile damaged (%d); reload verified pack",
                 terrain.errors);
    else if (terrain.loading)
        snprintf(terrain.status, sizeof(terrain.status), "Preparing terrain: %d tiles pending",
                 terrain.loading);
    else
        snprintf(terrain.status, sizeof(terrain.status), "USGS %s | %s | %d visible tiles",
                 mode == 1 ? "Topo" : "3DEP relief", s->area, terrain.drawn);
}
void terrain_credit_draw(AppState *app) {
    if (app->ui_state_bridge.utility_workspace != APP_UTILITY_HIKING_HISTORY)
        return;
    SDL_FRect r = app_map_viewport_rect(app);
    char line[300];
    snprintf(line, sizeof(line), "%s | %s", terrain_style_label(),
             terrain.style == 2      ? "background hidden"
             : *terrain.reload_error ? terrain.reload_error
                                     : terrain.status);
    renderer_set_draw_color(&app->renderer, 24, 31, 35, 220);
    float width=fminf(r.w-24,520);
    renderer_fill_rect(&app->renderer, &(SDL_FRect){r.x+12,r.y+r.h-44,width,36});
    ui_draw_text_clipped(&app->renderer,r.x+20,r.y+r.h-42,line,(SDL_Color){238,242,241,255},.65f,width-16);
    char usage[160];
    snprintf(usage,sizeof(usage),"USGS 3DEP + GNIS · offline · additional terrain %.1f / 128 MiB",
             terrain.catalog.additional_bytes/1048576.0);
    ui_draw_text_clipped(&app->renderer,r.x+20,r.y+r.h-24,usage,
        (SDL_Color){205,214,212,255},.60f,width-16);
}
