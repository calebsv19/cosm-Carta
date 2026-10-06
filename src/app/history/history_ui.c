#include "app/history_ui.h"
#include "app/history_layout.h"
#include "app/history_browser.h"
#include "app/terrain.h"
#include "app/terrain_ui.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_persist_state.h"
#include "app/app_ui_internal.h"
#include "app/app_utility_workspace.h"
#include "app/history.h"
#include "app/history_status.h"
#include "app/terrain_vectors.h"
#include "map/mercator.h"
#include "ui/font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Holds bounded display assets and an explicit native import/edit form.
typedef struct HistoryUI {
    HistoryTrack tracks[64];
    char hashes[64][65];
    size_t count, bytes;
    float scroll;
    bool scroll_dragging;
    float scroll_grab;
    char scroll_query[128];
    HistoryBrowser browser;
    char failed_hashes[64][65];
    size_t failed_count;
    bool cache_blocked;
    int point, field, type, visit_state, peak_offset;
    bool archived, form, exporting, editing, tools, inspect, peak_picker;
    char undo_id[64], undo_progress[24];
    int64_t undo_revision;
    char fields[7][2048], status[256], form_id[64];
    int64_t form_revision;
    struct json_object *request, *preview, *undo_snapshot;
} HistoryUI;
static HistoryUI ui;
// Reads a catalog field without JSON coercion.
static const char *str(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string)
               ? json_object_get_string(v)
               : "";
}
// Gets the durable history catalog without introducing empty mutations.
static struct json_object *catalog(AppState *app) {
    struct json_object *a = NULL;
    json_object_object_get_ex(app->pins_file.source_json, "hiking", &a);
    return a && json_object_is_type(a, json_type_array) ? a : NULL;
}
// Resolves the selected stable ID after import, archive, restore or region reload.
static struct json_object *selected(AppState *app) {
    struct json_object *a = catalog(app);
    for (size_t i = 0; a && i < json_object_array_length(a); i++) {
        struct json_object *v = json_object_array_get_idx(a, i);
        if (!strcmp(str(v, "id"), app->ui_state_bridge.history_selected_id))
            return v;
    }
    return NULL;
}
// Displays the strongest explicit active visit state while leaving unknown peaks unconfirmed.
static const char *peak_state(AppState *app, struct json_object *peak) {
    const char *state = str(peak, "state");
    struct json_object *a = catalog(app);
    for (size_t i = 0; a && i < json_object_array_length(a); i++) {
        struct json_object *v = json_object_array_get_idx(a, i), *archived = NULL;
        json_object_object_get_ex(v, "archived", &archived);
        if (strcmp(str(v, "kind"), "visit") || strcmp(str(v, "peak_id"), str(peak, "id")) ||
            json_object_get_boolean(archived))
            continue;
        if (!strcmp(str(v, "state"), "summited"))
            return "summited";
        if (!strcmp(str(v, "state"), "visited") && strcmp(state, "summited"))
            state = "visited";
    }
    return *state ? state : "unreviewed";
}
// Applies source/collection/archive/search filters while preserving backing identities.
static bool visible(AppState *app, struct json_object *v) {
    struct json_object *a = NULL;
    json_object_object_get_ex(v, "archived", &a);
    if (!ui.archived && json_object_get_boolean(a))
        return false;
    const char *progress=history_progress(app->pins_file.source_json,v);
    bool type=history_views_membership(app,v,false)!=0;
    const char *q = app->ui_state_bridge.place_search;
    return type && (app_utility_text_matches(str(v, "name"), q) ||
                    app_utility_text_matches(str(v, "visited_on"), q) ||
                    app_utility_text_matches(str(v, "notes"), q) ||
                    app_utility_text_matches(str(v, "source_url"), q) ||
                    app_utility_text_matches(progress, q));
}
// Projects verified source geometry independently of optional installed terrain coverage.
static bool has_source_geometry(struct json_object *v) {
    struct json_object *m = NULL, *bounds = NULL, *lat = NULL, *lon = NULL;
    json_object_object_get_ex(v, "metrics", &m);
    json_object_object_get_ex(m, "bounds", &bounds);
    if (*str(v, "asset_hash") && bounds && json_object_is_type(bounds, json_type_array) &&
        json_object_array_length(bounds) == 4)
        return true;
    return json_object_object_get_ex(v, "lat", &lat) &&
           json_object_object_get_ex(v, "lon", &lon);
}
// Uses identical personal filters for the browser and map overlays.
bool history_ui_record_visible(AppState *app, struct json_object *v) { return visible(app,v); }
bool history_ui_overlay_visible(AppState *app, struct json_object *v) {
    return visible(app,v) && history_views_membership(app,v,true) && has_source_geometry(v) && (!app->ui_state_bridge.history_selected_only || v==selected(app)) &&
        (strcmp(str(v,"kind"),"peak") || history_peak_tracked(app->pins_file.source_json,v));
}
bool history_ui_geographic_labels_enabled(AppState *app) { return !app->ui_state_bridge.history_hide_geographic_labels; }
// Suppresses duplicate trail/activity cards in personal collections while retaining Activities explicitly.
static bool representative(AppState *app,struct json_object *v, bool map_only) {
    if((strcmp(str(v,"kind"),"recorded") && strcmp(str(v,"kind"),"visit")))return true;
    struct json_object *a=catalog(app);
    for(size_t i=0;a&&i<json_object_array_length(a);i++) {
        struct json_object *other=json_object_array_get_idx(a,i);
        if((!strcmp(str(other,"recording_id"),str(v,"id")) || (*str(v,"trail_id")&&!strcmp(str(other,"id"),str(v,"trail_id")))) && (map_only?history_ui_overlay_visible(app,other):visible(app,other)))return false;
    }
    return true;
}
// Assigns status colors consistently to cards, peaks and trails; source type uses stroke style.
static SDL_Color progress_color(AppState *app,struct json_object *v) {
    const char *p=history_progress(app->pins_file.source_json,v);
    return !strcmp(p,"completed")?(SDL_Color){35,178,105,255}:!strcmp(p,"planned")?(SDL_Color){62,144,235,255}:
           !strcmp(p,"partial")?(SDL_Color){236,165,52,255}:(SDL_Color){140,146,155,255};
}
// Labels personal progress separately from source geometry.
static const char *progress_label(AppState *app,struct json_object *v) {
    const char *p=history_progress(app->pins_file.source_json,v);
    return !strcmp(p,"completed")?"Completed":!strcmp(p,"planned")?"Planned":!strcmp(p,"partial")?"Partial":!strcmp(p,"not_done")?"Not done":"Needs review";
}
// Maps the filtered ordinal to its stable record.
static struct json_object *row(AppState *app, int n) {
    struct json_object *a = catalog(app);
    for (size_t i = 0; a && i < json_object_array_length(a); i++) {
        struct json_object *v = json_object_array_get_idx(a, i);
        if (visible(app, v) && representative(app,v,false) && n-- == 0)
            return v;
    }
    return NULL;
}
// Lays out the history pane using the existing map/pane boundary.
static SDL_FRect pane(AppState *app) {
    SDL_FRect r = app->ui_state_bridge.left_pane_rect;
    if (r.w < 180)
        r = (SDL_FRect){0, 68, fminf(360, app->width * .45f), app->height - 68};
    return r;
}
// Exposes the same current control rectangles used by drawing and pointer input.
SDL_FRect history_ui_action_rect(AppState *app, int n) {
    HistoryLayout l = history_layout(pane(app), ui.tools, ui.inspect);
    return n >= 0 && n < 29 ? l.actions[n] : (SDL_FRect){0};
}
// Tests a pointer in a current draw rectangle.
static bool hit(SDL_FRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
// Returns a modal field rectangle that fits a narrow window.
static SDL_FRect field_rect(AppState *app, int n) {
    float w = fminf(620, app->width - 24);
    return (SDL_FRect){(app->width - w) / 2, 110 + n * 46, w, 32};
}
// Retrieves a verified original once and retains its normalized points under a 32 MiB budget.
// Frees display-only assets while preserving all durable originals.
static void clear_cache(void) {
    for (size_t i = 0; i < ui.count; i++)
        history_track_free(&ui.tracks[i]);
    ui.count = ui.bytes = ui.failed_count = 0;
    ui.cache_blocked=false;
}
// Stops retrying missing, damaged or budget-refused originals until an explicit reload.
static bool can_load(struct json_object *v) {
    const char *h=str(v,"asset_hash");
    if(!*h || ui.cache_blocked)return false;
    for(size_t i=0;i<ui.failed_count;i++)if(!strcmp(ui.failed_hashes[i],h))return false;
    return true;
}
// Retains a terminal display failure without altering durable route metadata.
static void failed(struct json_object *v) {
    if(ui.failed_count<64)snprintf(ui.failed_hashes[ui.failed_count++],65,"%s",str(v,"asset_hash"));
    else ui.cache_blocked=true;
}
static HistoryTrack *track(AppState *app, struct json_object *v, bool load) {
    const char *h = str(v, "asset_hash");
    if (!*h)
        return NULL;
    for (size_t i = 0; i < ui.count; i++)
        if (!strcmp(ui.hashes[i], h))
            return &ui.tracks[i];
    if (!load || !can_load(v))
        return NULL;
    if (ui.count == 64) {
        if (v == selected(app))
            clear_cache();
        else {
            snprintf(ui.status, sizeof(ui.status), "Display cache limit reached; use Map: Selected.");
            failed(v);return NULL;
        }
    }
    HistoryTrack t = {0};
    char err[256] = {0};
    if (!history_asset_load(app->pins_path, h, &t, err, sizeof(err))) {
        snprintf(ui.status, sizeof(ui.status), "%s", err);
        failed(v);return NULL;
    }
    size_t cost = t.byte_count + t.point_count * sizeof(HistoryPoint);
    if (cost > 32u * 1024u * 1024u - ui.bytes && v == selected(app))
        clear_cache();
    if (cost > 32u * 1024u * 1024u - ui.bytes) {
        history_track_free(&t);
        snprintf(ui.status, sizeof(ui.status), "Display cache full; select a smaller collection.");
        failed(v);return NULL;
    }
    snprintf(ui.hashes[ui.count], 65, "%s", h);
    ui.tracks[ui.count] = t;
    ui.bytes += cost;
    return &ui.tracks[ui.count++];
}
// Fits source geometry or a peak independently of road-graph and basemap coverage.
static void fit(AppState *app, bool all) {
    double minx = INFINITY, miny = INFINITY, maxx = -INFINITY, maxy = -INFINITY;
    struct json_object *a = catalog(app);
    for (size_t i = 0; a && i < json_object_array_length(a); i++) {
        struct json_object *v = json_object_array_get_idx(a, i);
        if ((all && (!history_ui_overlay_visible(app, v) || (!strcmp(str(v,"kind"),"peak") && !history_peak_tracked(app->pins_file.source_json,v)))) || (!all && v != selected(app)))
            continue;
        if(!has_source_geometry(v))continue;
        if(*str(v,"recording_id")){for(size_t j=0;a&&j<json_object_array_length(a);j++){struct json_object *other=json_object_array_get_idx(a,j);if(!strcmp(str(other,"id"),str(v,"recording_id")))v=other;}}
        struct json_object *m = NULL, *b = NULL, *lat = NULL, *lon = NULL;
        json_object_object_get_ex(v, "metrics", &m);
        json_object_object_get_ex(m, "bounds", &b);
        double la0, lo0, la1, lo1;
        if (b && json_object_array_length(b) == 4) {
            la0 = json_object_get_double(json_object_array_get_idx(b, 0));
            lo0 = json_object_get_double(json_object_array_get_idx(b, 1));
            la1 = json_object_get_double(json_object_array_get_idx(b, 2));
            lo1 = json_object_get_double(json_object_array_get_idx(b, 3));
        } else if (json_object_object_get_ex(v, "lat", &lat) && json_object_object_get_ex(v, "lon", &lon)) {
            la0 = la1 = json_object_get_double(lat);
            lo0 = lo1 = json_object_get_double(lon);
        } else
            continue;
        MercatorMeters p = mercator_from_latlon((LatLon){la0, lo0}),
                       q = mercator_from_latlon((LatLon){la1, lo1});
        minx = fmin(minx, p.x);
        miny = fmin(miny, p.y);
        maxx = fmax(maxx, q.x);
        maxy = fmax(maxy, q.y);
    }
    if (!isfinite(minx)) {
        snprintf(ui.status, sizeof(ui.status), "No source geometry · catalog entry retained");
        return;
    }
    ui.status[0] = 0;
    SDL_FRect view = app_map_viewport_rect(app);
    float ppm = fminf(fmaxf(1, view.w - 80) / fmax(100, maxx - minx),
                     fmaxf(1, view.h - 100) / fmax(100, maxy - miny));
    Camera *c = &app->view_state_bridge.camera;
    c->x = c->x_target = (float)((minx + maxx) / 2);
    c->y = c->y_target = (float)((miny + maxy) / 2);
    c->zoom = c->zoom_target = fminf(18, fmaxf(1, (float)log2(ppm * mercator_world_size_meters() / 256)));
    c->heading_rad = c->heading_target_rad = 0;
    app_save_persisted_view_state(app);
}
// Reopens hiking geometry after the normal startup region/home camera has been initialized.
void history_ui_restore_view(AppState *app) {
    if (app && app->ui_state_bridge.utility_workspace == APP_UTILITY_HIKING_HISTORY) {
        history_views_ensure(app);
        fit(app, selected(app) == NULL);
    }
}
// Clears a preview and owned request before cancellation or a changed form.
static void clear_preview(void) {
    if (ui.request)
        json_object_put(ui.request);
    if (ui.preview)
        json_object_put(ui.preview);
    ui.request = ui.preview = NULL;
}
// Opens a native form without modifying history until confirmed import.
static void open_form(AppState *app, bool edit, bool export) {
    clear_preview();
    memset(ui.fields, 0, sizeof(ui.fields));
    ui.form = true;
    ui.field = 0;
    ui.type = 0;
    ui.visit_state = -1;
    ui.exporting = export;
    ui.editing = edit;
    struct json_object *v = selected(app);
    struct json_object *form_revision = NULL;
    json_object_object_get_ex(v, "revision", &form_revision);
    snprintf(ui.form_id, sizeof(ui.form_id), "%s", str(v, "id"));
    ui.form_revision = json_object_get_int64(form_revision);
    const char *k = str(v, "kind"), *types[] = {"recorded", "recommended", "manual", "peak", "visit"};
    if (edit && v) {
        for (int i = 0; i < 5; i++)
            if (!strcmp(k, types[i]))
                ui.type = i;
        snprintf(ui.fields[1], 2048, "%s", str(v, "name"));
        snprintf(ui.fields[2], 2048, "%s", str(v, "visited_on"));
        snprintf(ui.fields[3], 2048, "%s", str(v, "notes"));
    }
    if (edit && v && !strcmp(k, "peak")) {
        struct json_object *la = NULL, *lo = NULL;
        json_object_object_get_ex(v, "lat", &la);
        json_object_object_get_ex(v, "lon", &lo);
        snprintf(ui.fields[4], 2048, "%.8f", json_object_get_double(la));
        snprintf(ui.fields[5], 2048, "%.8f", json_object_get_double(lo));
    }
    if (edit && v) {
        const char *state = str(v, "state");
        ui.visit_state = !strcmp(state, "wishlist")    ? 0
                         : !strcmp(state, "visited")   ? 1
                         : !strcmp(state, "summited")  ? 2
                         : !strcmp(state, "unvisited") ? 3
                                                       : -1;
    }
    if (export) {
        const char *runtime = getenv("MAPFORGE_RUNTIME_DIR");
        if (runtime)
            snprintf(ui.fields[0], 2048, "%s/hike-%.16s.gpx", runtime, str(v, "asset_hash"));
    }
    snprintf(ui.status, sizeof(ui.status), "%s",
             export ? "Enter a new absolute export filename."
                    : "Enter a source path, or choose Manual / Peak / Visit.");
    SDL_StartTextInput();
}
// Executes an explicit preview or its bound commit and updates visible status.
static void submit(AppState *app) {
    if (ui.exporting) {
        struct json_object *v = selected(app), *q = json_object_new_object();
        json_object_object_add(q, "operation", json_object_new_string("history.export"));
        json_object_object_add(q, "record_id", json_object_new_string(str(v, "id")));
        json_object_object_add(q, "destination", json_object_new_string(ui.fields[0]));
        struct json_object *r = history_command(app->pins_path, &app->pins_file, q);
        snprintf(ui.status, sizeof(ui.status), "%s",
                 *str(r, "error") ? str(r, "error") : "Original exported.");
        json_object_put(q);
        json_object_put(r);
        return;
    }
    if (!ui.preview) {
        const char *types[] = {"recorded", "recommended", "manual", "peak", "visit"};
        ui.request = json_object_new_object();
        char rid[96];
        snprintf(rid, sizeof(rid), "native-hike-%llu", (unsigned long long)SDL_GetPerformanceCounter());
        json_object_object_add(ui.request, "request_id", json_object_new_string(rid));
        json_object_object_add(ui.request, "operation", json_object_new_string("history.preview"));
        json_object_object_add(ui.request, "kind", json_object_new_string(types[ui.type]));
        const char *keys[] = {"source_path", "name", "visited_on", "notes"};
        for (int i = 0; i < 4; i++)
            if (*ui.fields[i] && (i != 0 || ui.type < 2))
                json_object_object_add(ui.request, keys[i], json_object_new_string(ui.fields[i]));
        if ((ui.type == 3 || ui.type == 4) && ui.visit_state >= 0)
            json_object_object_add(ui.request, "state",
                                   json_object_new_string(ui.visit_state == 2   ? "summited"
                                                          : ui.visit_state == 1 ? "visited"
                                                          : ui.visit_state == 3 ? "unvisited"
                                                                                : "wishlist"));
        json_object_object_add(ui.request, "evidence",
                               json_object_new_string(ui.type == 1 ? "Downloaded reference geometry"
                                                                   : "User-entered history"));
        if (ui.type == 3) {
            char *e1, *e2;
            double lat = strtod(ui.fields[4], &e1), lon = strtod(ui.fields[5], &e2);
            if (e1 == ui.fields[4] || e2 == ui.fields[5] || *e1 || *e2) {
                clear_preview();
                snprintf(ui.status, sizeof(ui.status), "Enter numeric peak latitude and longitude.");
                return;
            }
            json_object_object_add(ui.request, "lat", json_object_new_double(lat));
            json_object_object_add(ui.request, "lon", json_object_new_double(lon));
        }
        struct json_object *v = selected(app);
        if (ui.editing && v) {
            json_object_object_add(ui.request, "record_id", json_object_new_string(ui.form_id));
            json_object_object_add(ui.request, "expected_revision", json_object_new_int64(ui.form_revision));
        }
        if (ui.type == 4 && v && !strcmp(str(v, "kind"), "peak"))
            json_object_object_add(ui.request, "peak_id", json_object_new_string(str(v, "id")));
        ui.preview = history_command(app->pins_path, &app->pins_file, ui.request);
    } else {
        struct json_object *ct = NULL, *st = NULL;
        json_object_object_get_ex(ui.preview, "candidate_token", &ct);
        json_object_object_get_ex(ui.preview, "store_token", &st);
        if (!ct || !st) {
            snprintf(ui.status, sizeof(ui.status),
                     "Existing record or refused preview; cancel or change input.");
            return;
        }
        json_object_object_add(ui.request, "operation", json_object_new_string("history.commit"));
        json_object_object_add(ui.request, "candidate_token", json_object_get(ct));
        json_object_object_add(ui.request, "expected_store_token", json_object_get(st));
        struct json_object *r = history_command(app->pins_path, &app->pins_file, ui.request), *v = NULL,
                           *ok = NULL;
        json_object_object_get_ex(r, "ok", &ok);
        if (json_object_get_boolean(ok)) {
            json_object_object_get_ex(r, "record", &v);
            snprintf(app->ui_state_bridge.history_selected_id, 64, "%s", str(v, "id"));
            ui.form = false;
            if(!visible(app,v)) {
                HistoryViewSelection group=history_views_choice(app,!strcmp(str(v,"kind"),"recorded")?3:!strcmp(str(v,"kind"),"recommended")?4:!strcmp(str(v,"kind"),"peak")?5:6);
                history_views_toggle(app,group);
            }
            app_save_persisted_view_state(app);
        }
        snprintf(ui.status, sizeof(ui.status), "%s",
                 *str(r, "error") ? str(r, "error") : "History saved and read back.");
        json_object_put(r);
        clear_preview();
        return;
    }
    snprintf(ui.status, sizeof(ui.status), "%s",
             *str(ui.preview, "error") ? str(ui.preview, "error")
                                       : "Preview ready. Review source details, then Import.");
}
// Archives/restores the selected entry through revision and preview guards.
static void archive(AppState *app) {
    struct json_object *v = selected(app), *q = json_object_new_object(), *a = NULL, *rev = NULL;
    if (!v) {
        json_object_put(q);
        return;
    }
    json_object_object_get_ex(v, "archived", &a);
    json_object_object_get_ex(v, "revision", &rev);
    char id[96];
    snprintf(id, sizeof(id), "native-archive-%llu", (unsigned long long)SDL_GetPerformanceCounter());
    json_object_object_add(q, "request_id", json_object_new_string(id));
    json_object_object_add(q, "record_id", json_object_new_string(str(v, "id")));
    json_object_object_add(q, "expected_revision", json_object_get(rev));
    json_object_object_add(q, "action",
                           json_object_new_string(json_object_get_boolean(a) ? "restore" : "archive"));
    json_object_object_add(q, "operation", json_object_new_string("history.preview"));
    struct json_object *r = history_command(app->pins_path, &app->pins_file, q), *ct = NULL, *st = NULL;
    json_object_object_get_ex(r, "candidate_token", &ct);
    json_object_object_get_ex(r, "store_token", &st);
    if (ct && st) {
        json_object_object_add(q, "candidate_token", json_object_get(ct));
        json_object_object_add(q, "expected_store_token", json_object_get(st));
        json_object_object_add(q, "operation", json_object_new_string("history.commit"));
        json_object_put(r);
        r = history_command(app->pins_path, &app->pins_file, q);
    }
    snprintf(ui.status, sizeof(ui.status), "%s", *str(r, "error") ? str(r, "error") : "Archive state saved.");
    json_object_put(r);
    json_object_put(q);
}
// Commits one explicit status change with reversible in-session undo and revision guards.
static void set_progress(AppState *app,const char *progress,bool undo) {
    struct json_object *v=selected(app);
    if(undo) {
        struct json_object *a=catalog(app);v=NULL;
        for(size_t i=0;a&&i<json_object_array_length(a);i++)if(!strcmp(str(json_object_array_get_idx(a,i),"id"),ui.undo_id))v=json_object_array_get_idx(a,i);
    }
    if(!v)return;
    struct json_object *rev=NULL;json_object_object_get_ex(v,"revision",&rev);
    int64_t revision=json_object_get_int64(rev);
    if(undo && revision!=ui.undo_revision){snprintf(ui.status,sizeof(ui.status),"Undo refused: record changed since status edit");return;}
    struct json_object *previous=json_object_new_object();
    const char *keys[]={"progress","state","tracked","status_origin","status_evidence"};
    for(unsigned i=0;i<5;i++){struct json_object *value=NULL;if(json_object_object_get_ex(v,keys[i],&value))json_object_object_add(previous,keys[i],json_object_get(value));}
    char id[64];snprintf(id,sizeof(id),"%s",str(v,"id"));
    struct json_object *q=json_object_new_object();char rid[96];snprintf(rid,sizeof(rid),"native-status-%llu",(unsigned long long)SDL_GetPerformanceCounter());
    json_object_object_add(q,"request_id",json_object_new_string(rid));json_object_object_add(q,"record_id",json_object_new_string(id));
    json_object_object_add(q,"expected_revision",json_object_get(rev));json_object_object_add(q,"action",json_object_new_string(undo?"status_restore":"status"));
    if(undo)json_object_object_add(q,"status_snapshot",json_object_get(ui.undo_snapshot));
    else {json_object_object_add(q,"progress",json_object_new_string(progress));json_object_object_add(q,"status_evidence",json_object_new_string("User set status in Carta"));}
    json_object_object_add(q,"operation",json_object_new_string("history.preview"));
    struct json_object *r=history_command(app->pins_path,&app->pins_file,q),*ct=NULL,*st=NULL,*ok=NULL;
    json_object_object_get_ex(r,"candidate_token",&ct);json_object_object_get_ex(r,"store_token",&st);
    if(ct&&st){json_object_object_add(q,"candidate_token",json_object_get(ct));json_object_object_add(q,"expected_store_token",json_object_get(st));json_object_object_add(q,"operation",json_object_new_string("history.commit"));json_object_put(r);r=history_command(app->pins_path,&app->pins_file,q);}
    json_object_object_get_ex(r,"ok",&ok);
    if(json_object_get_boolean(ok)) {
        if(ui.undo_snapshot)json_object_put(ui.undo_snapshot);
        if(undo){ui.undo_id[0]=0;ui.undo_snapshot=NULL;}else{snprintf(ui.undo_id,sizeof(ui.undo_id),"%s",id);ui.undo_snapshot=json_object_get(previous);ui.undo_revision=revision+1;}
        struct json_object *current=selected(app);if(current&&!visible(app,current))app->ui_state_bridge.history_selected_id[0]=0;
        app_save_persisted_view_state(app);
    }
    snprintf(ui.status,sizeof(ui.status),"%s",*str(r,"error")?str(r,"error"):"Personal status saved · Undo available");json_object_put(r);json_object_put(q);json_object_put(previous);
}
// Adds an official installed peak as a planned personal goal without changing geographic coordinates.
static void choose_peak(AppState *app,int ordinal) {
    char name[128];double lat,lon;
    if(!terrain_vectors_peak_query(ui.fields[0],ui.peak_offset+ordinal,name,sizeof(name),&lat,&lon))return;
    struct json_object *a=catalog(app),*v=NULL;
    for(size_t i=0;a&&i<json_object_array_length(a);i++) {struct json_object *other=json_object_array_get_idx(a,i);if(!strcmp(str(other,"kind"),"peak")&&!strcmp(str(other,"name"),name)){v=other;break;}}
    if(v){snprintf(app->ui_state_bridge.history_selected_id,64,"%s",str(v,"id"));set_progress(app,"planned",false);}
    else {
        struct json_object *q=json_object_new_object();char rid[96],external[96];snprintf(rid,sizeof(rid),"native-goal-%llu",(unsigned long long)SDL_GetPerformanceCounter());snprintf(external,sizeof(external),"gnis-location:%.6f,%.6f",lat,lon);
        json_object_object_add(q,"request_id",json_object_new_string(rid));json_object_object_add(q,"kind",json_object_new_string("peak"));json_object_object_add(q,"name",json_object_new_string(name));json_object_object_add(q,"lat",json_object_new_double(lat));json_object_object_add(q,"lon",json_object_new_double(lon));json_object_object_add(q,"external_id",json_object_new_string(external));json_object_object_add(q,"evidence",json_object_new_string("USGS GNIS installed terrain name"));json_object_object_add(q,"progress",json_object_new_string("planned"));json_object_object_add(q,"operation",json_object_new_string("history.preview"));
        struct json_object *res=history_command(app->pins_path,&app->pins_file,q),*ct=NULL,*st=NULL;
        json_object_object_get_ex(res,"candidate_token",&ct);json_object_object_get_ex(res,"store_token",&st);
        if(ct&&st){json_object_object_add(q,"candidate_token",json_object_get(ct));json_object_object_add(q,"expected_store_token",json_object_get(st));json_object_object_add(q,"operation",json_object_new_string("history.commit"));json_object_put(res);res=history_command(app->pins_path,&app->pins_file,q);}
        struct json_object *saved=NULL;json_object_object_get_ex(res,"record",&saved);snprintf(app->ui_state_bridge.history_selected_id,64,"%s",str(saved,"id"));
        snprintf(ui.status,sizeof(ui.status),"%s",*str(res,"error")?str(res,"error"):"Peak added to To do");json_object_put(res);json_object_put(q);
    }
    ui.peak_picker=false;HistoryViewSelection goal_view=history_views_choice(app,2);if(history_views_find(app,goal_view)<0)history_views_toggle(app,goal_view);fit(app,false);app_save_persisted_view_state(app);
}
// Reports whether native history input owns pointer and keyboard events.
bool history_ui_modal(void) {
    return ui.form || ui.peak_picker || ui.browser.open || terrain_ui_active();
}
// Shares legend chip geometry with pointer handling below.
static SDL_FRect legend_chip(HistoryLayout layout, int n);
// Handles history browsing and explicit form preview/commit actions.
bool history_ui_event(AppState *app, const SDL_Event *e) {
    if (app->ui_state_bridge.utility_workspace != APP_UTILITY_HIKING_HISTORY)
        return false;
    if(terrain_ui_event(app,e))return true;
    if (ui.browser.open) {
        if (history_browser_event(&ui.browser,app,e)) {
            ui.scroll=0; ui.status[0]=0;
            if(selected(app)&&!visible(app,selected(app)))app->ui_state_bridge.history_selected_id[0]=0;
            app_save_persisted_view_state(app);
        }
        return true;
    }
    if(ui.peak_picker) {
        if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_ESCAPE){ui.peak_picker=false;return true;}
        if(e->type==SDL_TEXTINPUT){size_t n=strlen(ui.fields[0]);if(n+strlen(e->text.text)<128)strcat(ui.fields[0],e->text.text);ui.peak_offset=0;return true;}
        if(e->type==SDL_KEYDOWN && e->key.keysym.sym==SDLK_BACKSPACE){size_t n=strlen(ui.fields[0]);if(n)ui.fields[0][n-1]=0;ui.peak_offset=0;return true;}
        if(e->type==SDL_MOUSEBUTTONDOWN){SDL_FRect b=field_rect(app,0);for(int i=0;i<6;i++){SDL_FRect row={b.x,b.y+42+i*38,b.w,34};if(hit(row,e->button.x,e->button.y))choose_peak(app,i);}SDL_FRect prev={b.x,b.y+280,b.w*.48f,28},next={b.x+b.w*.52f,b.y+280,b.w*.48f,28};if(hit(prev,e->button.x,e->button.y))ui.peak_offset=fmax(0,ui.peak_offset-6);if(hit(next,e->button.x,e->button.y))ui.peak_offset+=6;return true;}
        return true;
    }
    if (e->type == SDL_DROPFILE) {
        open_form(app, false, false);
        snprintf(ui.fields[0], 2048, "%s", e->drop.file);
        SDL_free(e->drop.file);
        return true;
    }
    if (ui.form) {
        if (e->type == SDL_TEXTINPUT) {
            clear_preview();
            size_t n = strlen(ui.fields[ui.field]);
            if (n + strlen(e->text.text) < 2048)
                strcat(ui.fields[ui.field], e->text.text);
            return true;
        }
        if (e->type == SDL_KEYDOWN) {
            SDL_Keycode k = e->key.keysym.sym;
            if (k == SDLK_ESCAPE) {
                ui.form = false;
                clear_preview();
            } else if (k == SDLK_TAB)
                ui.field = (ui.field + 1) % 6;
            else if (k == SDLK_RETURN)
                submit(app);
            else if (k == SDLK_BACKSPACE) {
                clear_preview();
                size_t n = strlen(ui.fields[ui.field]);
                if (n) {
                    do
                        --n;
                    while (n && (ui.fields[ui.field][n] & 0xc0) == 0x80);
                    ui.fields[ui.field][n] = 0;
                }
            } else if ((e->key.keysym.mod & KMOD_CTRL) && k == SDLK_a) {
                clear_preview();
                ui.fields[ui.field][0] = 0;
            } else if ((e->key.keysym.mod & KMOD_CTRL) && k == SDLK_v) {
                clear_preview();
                char *s = SDL_GetClipboardText();
                if (s) {
                    snprintf(ui.fields[ui.field], 2048, "%s", s);
                    SDL_free(s);
                }
            }
            return true;
        }
        if (e->type == SDL_MOUSEBUTTONDOWN) {
            for (int i = 0; i < 6; i++)
                if (hit(field_rect(app, i), e->button.x, e->button.y)) {
                    ui.field = i;
                    return true;
                }
            SDL_FRect base = field_rect(app, 6);
            base.h = 28;
            if (hit(base, e->button.x, e->button.y)) {
                clear_preview();
                ui.type = (ui.type + 1) % 5;
                return true;
            }
            base.y += 34;
            if (hit(base, e->button.x, e->button.y)) {
                if (e->button.x > base.x + base.w * .72f && (ui.type == 3 || ui.type == 4)) {
                    clear_preview();
                    ui.visit_state = (ui.visit_state + 2) % 5 - 1;
                } else
                    submit(app);
            }
            return true;
        }
        return e->type == SDL_MOUSEWHEEL || e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEMOTION ||
               e->type == SDL_KEYUP;
    }
    SDL_FRect p = pane(app);
    HistoryLayout layout = history_layout(p, ui.tools, ui.inspect);
    int total=0; while(row(app,total))total++;
    ui.scroll=history_browser_scroll(layout.list,total,HISTORY_ROW_HEIGHT,ui.scroll,0);
    if(e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT) {
        for(size_t i=0;i<app->ui_state_bridge.history_views.count && i<3;i++)
            if(hit(legend_chip(layout,i),e->button.x,e->button.y)) {
                HistoryViewSelection *group=&app->ui_state_bridge.history_views.items[i];
                group->map_visible=!group->map_visible;app_save_persisted_view_state(app);return true;
            }
    }
    if(e->type==SDL_MOUSEBUTTONUP && ui.scroll_dragging){ui.scroll_dragging=false;return true;}
    if(e->type==SDL_MOUSEMOTION && ui.scroll_dragging) {
        SDL_FRect thumb=history_browser_thumb(layout.list,total,HISTORY_ROW_HEIGHT,ui.scroll);
        float travel=layout.list.h-thumb.h;
        if(travel>0)ui.scroll=history_browser_scroll(layout.list,total,HISTORY_ROW_HEIGHT,
            (e->motion.y-layout.list.y-ui.scroll_grab)/travel*(total*HISTORY_ROW_HEIGHT-layout.list.h),0);
        return true;
    }
    if (e->type == SDL_MOUSEWHEEL &&
        hit(p, app->ui_state_bridge.input.mouse_x, app->ui_state_bridge.input.mouse_y)) {
        if(hit(layout.list,app->ui_state_bridge.input.mouse_x,app->ui_state_bridge.input.mouse_y))
            ui.scroll=history_browser_scroll(layout.list,total,HISTORY_ROW_HEIGHT,ui.scroll,history_browser_wheel(e));
        return true;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && hit(p, e->button.x, e->button.y)) {
        if (e->button.button != SDL_BUTTON_LEFT) return true;
        for (int i = 0; i < 29; i++)
            if (hit(history_ui_action_rect(app, i), e->button.x, e->button.y)) {
                switch (i) {
                case 12: ui.tools = !ui.tools; break;
                case 28: terrain_ui_open(); break;
                case 13: ui.inspect = !ui.inspect; break;
                case 14: history_browser_open(&ui.browser,app);break;
                case 20:set_progress(app,"planned",false);break;
                case 21:set_progress(app,"completed",false);break;
                case 22:set_progress(app,"partial",false);break;
                case 23:set_progress(app,"not_done",false);break;
                case 24:set_progress(app,ui.undo_progress,true);break;
                
                case 26:app->ui_state_bridge.history_hide_geographic_labels=!app->ui_state_bridge.history_hide_geographic_labels;app_save_persisted_view_state(app);break;
                case 27:ui.peak_picker=true;ui.peak_offset=0;ui.fields[0][0]=0;SDL_StartTextInput();break;
                case 15: ui.archived = !ui.archived; ui.scroll = 0; break;
                case 16: app->ui_state_bridge.place_search_active = true; SDL_StartTextInput(); break;
                case 0:
                    open_form(app, false, false);
                    break;
                case 1:
                    fit(app, false);
                    break;
                case 2:
                    app->ui_state_bridge.history_selected_only=false;
                    fit(app, true);
                    break;
                case 3:
                    app->ui_state_bridge.history_selected_only = !app->ui_state_bridge.history_selected_only;
                    app_save_persisted_view_state(app);
                    break;
                case 4:
                    open_form(app, true, false);
                    break;
                case 5:
                    archive(app);
                    break;
                case 6:
                    open_form(app, false, true);
                    break;
                case 7:
                    ui.point = fmax(0, ui.point - 1);
                    break;
                case 8:
                    ui.point++;
                    break;
                case 10: terrain_cycle(); app_save_persisted_view_state(app); break;
                case 11: clear_cache();terrain_reload(app); break;
                case 9: {
                    struct json_object *v = selected(app), *q = json_object_new_object();
                    json_object_object_add(q, "operation", json_object_new_string("history.nearby"));
                    json_object_object_add(q, "record_id", json_object_new_string(str(v, "id")));
                    struct json_object *r = history_command(app->pins_path, &app->pins_file, q),
                                       *candidates = NULL;
                    json_object_object_get_ex(r, "candidates", &candidates);
                    if (candidates && json_object_array_length(candidates)) {
                        struct json_object *first = json_object_array_get_idx(candidates, 0), *d = NULL;
                        json_object_object_get_ex(first, "distance_m", &d);
                        snprintf(ui.status, sizeof(ui.status), "Review only: %s, %.0f m; %zu candidates",
                                 str(first, "name"), json_object_get_double(d),
                                 json_object_array_length(candidates));
                    } else
                        snprintf(ui.status, sizeof(ui.status), "%s",
                                 *str(r, "error") ? str(r, "error")
                                                  : "No peak points within 200 m. No visit inferred.");
                    json_object_put(q);
                    json_object_put(r);
                    break;
                }
                }
                return true;
            }
        SDL_FRect thumb=history_browser_thumb(layout.list,total,HISTORY_ROW_HEIGHT,ui.scroll);
        if(thumb.w && e->button.x>=layout.list.x+layout.list.w-12 && hit(layout.list,e->button.x,e->button.y)) {
            ui.scroll_dragging=true;ui.scroll_grab=hit(thumb,e->button.x,e->button.y)?e->button.y-thumb.y:thumb.h/2;
            float travel=layout.list.h-thumb.h;
            if(travel>0)ui.scroll=history_browser_scroll(layout.list,total,HISTORY_ROW_HEIGHT,
                (e->button.y-layout.list.y-ui.scroll_grab)/travel*(total*HISTORY_ROW_HEIGHT-layout.list.h),0);
            return true;
        }
        int n=history_browser_row(layout.list,ui.scroll,HISTORY_ROW_HEIGHT,e->button.x,e->button.y);
        if(n>=0) {
            struct json_object *v=row(app,n);
            if (v) {
                snprintf(app->ui_state_bridge.history_selected_id, 64, "%s", str(v, "id"));
                ui.point = 0;
                ui.status[0] = 0;
                fit(app, false);
            }
            return true;
        }
        return true;
    }
    return false;
}
// Draws themed text clipped to the history pane or modal.
static void label(AppState *app, float x, float y, const char *s, SDL_Color c, float width) {
    ui_draw_text_clipped(&app->renderer, (int)x, (int)y, s, c, .8f, (int)fmaxf(1, width));
}
// Draws a readable themed action cell.
static void cell(AppState *app, SDL_FRect r, const char *s, bool active) {
    MapForgeThemePalette p = app_ui_theme_palette();
    SDL_Color c = active ? p.button_active_primary : p.button_fill;
    renderer_set_draw_color(&app->renderer, c.r, c.g, c.b, c.a);
    renderer_fill_rect(&app->renderer, &r);
    SDL_Color fg = (299u * c.r + 587u * c.g + 114u * c.b) / 1000u > 160u ? (SDL_Color){20, 28, 38, 255}
                                                                         : (SDL_Color){235, 240, 248, 255};
    bool chooser=!strncmp(s,"Views:",6);
    label(app, r.x + 5, r.y + 4, s, fg, r.w - (chooser?24:10));
    if(chooser){
        renderer_set_draw_color(&app->renderer,fg.r,fg.g,fg.b,255);
        for(int i=0;i<3;i++)renderer_fill_rect(&app->renderer,&(SDL_FRect){r.x+r.w-13+i,r.y+r.h/2-1+i,6-2*i,1});
    }
}
// Draws bounded display geometry with original segment gaps intact.
void history_ui_map_draw(AppState *app) {
    app->history_pending=false;
    if (app->ui_state_bridge.utility_workspace != APP_UTILITY_HIKING_HISTORY)
        return;
    struct json_object *a = catalog(app);
    bool loaded = false;
    struct json_object *focus=selected(app);
    size_t selected_reserve=0;
    if(focus && history_ui_overlay_visible(app,focus)) {
        struct json_object *geometry=focus;
        for(size_t i=0;*str(focus,"recording_id") && a && i<json_object_array_length(a);i++) {
            struct json_object *other=json_object_array_get_idx(a,i);
            if(!strcmp(str(other,"id"),str(focus,"recording_id")))geometry=other;
        }
        if(!track(app,geometry,false) && can_load(geometry)){track(app,geometry,true);loaded=true;}
        HistoryTrack *prepared=track(app,geometry,false);
        if(prepared)selected_reserve=prepared->point_count<6000?prepared->point_count:6000;
    }
    size_t lines = 0;
    SDL_FRect view = app_map_viewport_rect(app);
    size_t records=a?json_object_array_length(a):0;
    for (size_t i=0;i<=records;i++) {
        // Reserve selected geometry first, but paint it last so shared paths cannot obscure its tint.
        struct json_object *v=i==records?focus:json_object_array_get_idx(a,i);
        if(!v || (i<records && v==focus))continue;
        const char *k = str(v, "kind");
        if (!history_ui_overlay_visible(app,v)) continue;
        if (!app->ui_state_bridge.history_selected_only && !representative(app,v,true)) continue;
        if (!strcmp(k, "peak")) {
            struct json_object *archived=NULL;json_object_object_get_ex(v,"archived",&archived);
            if(!ui.archived && json_object_get_boolean(archived))continue;
            struct json_object *lat = NULL, *lon = NULL;
            json_object_object_get_ex(v, "lat", &lat);
            json_object_object_get_ex(v, "lon", &lon);
            MercatorMeters p =
                mercator_from_latlon((LatLon){json_object_get_double(lat), json_object_get_double(lon)});
            float x, y;
            app_map_world_to_viewport_local(app, p.x, p.y, &x, &y);
            SDL_Color color=history_views_record_color(app,v);renderer_set_draw_color(&app->renderer,color.r,color.g,color.b,255);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){x - 4, y - 4, 8, 8});
            continue;
        }
        struct json_object *geometry=v;
        if(!strcmp(k,"recommended") && *str(v,"recording_id")) {
            for(size_t j=0;a&&j<json_object_array_length(a);j++){struct json_object *other=json_object_array_get_idx(a,j);if(!strcmp(str(other,"id"),str(v,"recording_id")))geometry=other;}
        }
        HistoryTrack *t = track(app, geometry, false);
        if (!t && !loaded) {
            t = track(app, geometry, true);
            loaded = true;
        }
        if (!t) {
            if(can_load(geometry))app->history_pending=true;
            continue;
        }
        bool recorded = !strcmp(str(geometry,"kind"), "recorded"), sel = v == selected(app);
        SDL_Color color=history_views_record_color(app,v);renderer_set_draw_color(&app->renderer,color.r,color.g,color.b,sel?255:180);
        float lastx = 0, lasty = 0;
        unsigned seg = UINT32_MAX, drawn = 0;
        for (size_t j = 0; j < t->point_count; j++) {
            HistoryPoint pt = t->points[j];
            MercatorMeters m = mercator_from_latlon((LatLon){pt.lat, pt.lon});
            float x, y;
            app_map_world_to_viewport_local(app, m.x, m.y, &x, &y);
            bool boundary = j + 1 == t->point_count || t->points[j + 1].segment != pt.segment;
            if (seg != pt.segment) {
                lastx = x;
                lasty = y;
                seg = pt.segment;
                drawn = 0;
                continue;
            }
            if (!boundary && hypotf(x - lastx, y - lasty) < 2.0f)
                continue;
            bool intersects = fmaxf(lastx, x) >= 0 && fminf(lastx, x) <= view.w && fmaxf(lasty, y) >= 0 &&
                              fminf(lasty, y) <= view.h;
            if (intersects && (recorded || drawn++ % 3 != 2) && lines < (sel?6000:6000-selected_reserve)) {
                // A dark edge makes every result readable over relief; selection stays slightly wider.
                float length=hypotf(x-lastx,y-lasty);
                if(length>=2){float width=sel?1.8f:1.3f,ox=-(y-lasty)/length*width,oy=(x-lastx)/length*width;
                    renderer_set_draw_color(&app->renderer,12,24,29,230);
                    renderer_draw_line(&app->renderer,lastx+ox,lasty+oy,x+ox,y+oy);
                    renderer_draw_line(&app->renderer,lastx-ox,lasty-oy,x-ox,y-oy);}
                renderer_set_draw_color(&app->renderer,color.r,color.g,color.b,255);
                if(length>=2) {
                    float width=sel?.65f:.35f,ox=-(y-lasty)/length*width,oy=(x-lastx)/length*width;
                    renderer_draw_line(&app->renderer,lastx+ox,lasty+oy,x+ox,y+oy);
                    renderer_draw_line(&app->renderer,lastx-ox,lasty-oy,x-ox,y-oy);
                }
                renderer_draw_line(&app->renderer, lastx, lasty, x, y);
                ++lines;
            }
            lastx = x;
            lasty = y;
            seg = pt.segment;
        }
        if (sel && t->point_count) {
            size_t j = (size_t)ui.point % t->point_count;
            MercatorMeters m = mercator_from_latlon((LatLon){t->points[j].lat, t->points[j].lon});
            float x, y;
            app_map_world_to_viewport_local(app, m.x, m.y, &x, &y);
            renderer_set_draw_color(&app->renderer, 255, 80, 90, 255);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){x - 4, y - 4, 8, 8});
        }
    }
}
// Blends a semantic surface with a small blue-grey tint for readable dark and light panes.
static SDL_Color surface(SDL_Color c, bool raised) {
    bool light = c.r + c.g + c.b > 450;
    SDL_Color tint = light ? (SDL_Color){220, 231, 233, 255} : (SDL_Color){40, 55, 63, 255};
    float amount = raised ? .50f : .22f;
    return (SDL_Color){c.r*(1-amount)+tint.r*amount, c.g*(1-amount)+tint.g*amount,
                       c.b*(1-amount)+tint.b*amount, 255};
}
// Fills a bounded panel surface through both renderer backends.
static void fill(AppState *app, SDL_FRect r, SDL_Color c) {
    renderer_set_draw_color(&app->renderer, c.r, c.g, c.b, c.a);
    renderer_fill_rect(&app->renderer, &r);
}
// Draws source elevation at screen resolution without joining segment or missing-data gaps.
static void profile_draw(AppState *app, HistoryTrack *t, SDL_FRect box) {
    double low = INFINITY, high = -INFINITY;
    for (size_t i=0; i<t->point_count; i++) if(isfinite(t->points[i].elevation)) {
        low=fmin(low,t->points[i].elevation); high=fmax(high,t->points[i].elevation);
    }
    if (!isfinite(low)) return;
    renderer_set_draw_color(&app->renderer, 54, 166, 154, 255);
    float lastx=0,lasty=0; unsigned segment=UINT32_MAX; bool have=false;
    for(size_t i=0;i<t->point_count;i++) {
        HistoryPoint pt=t->points[i];
        if(!isfinite(pt.elevation)){have=false;continue;}
        float x=box.x+box.w*(t->distance_m>0?pt.distance_m/t->distance_m:(double)i/fmax(1,t->point_count-1));
        float y=box.y+box.h*(1-(pt.elevation-low)/fmax(1,high-low));
        if(have && segment==pt.segment && (x-lastx>=2 || i+1==t->point_count)) {
            renderer_draw_line(&app->renderer,lastx,lasty,x,y);lastx=x;lasty=y;
        } else if(!have || segment!=pt.segment){lastx=x;lasty=y;}
        segment=pt.segment;have=true;
    }
}
// Returns one bounded legend chip; remaining groups stay available in the chooser.
static SDL_FRect legend_chip(HistoryLayout layout, int n) {
    float w=(layout.legend.w-8)/3;
    return (SDL_FRect){layout.legend.x+n*(w+4),layout.legend.y,w,layout.legend.h};
}
// Draws matching group tints and makes map-hidden groups explicit without removing their cards.
static void draw_legend(AppState *app, HistoryLayout layout, MapForgeThemePalette colors) {
    HistoryViewSet *views=&app->ui_state_bridge.history_views;
    if(!views->count){label(app,layout.legend.x,layout.legend.y+3,"Choose views to see hikes",colors.text_muted,layout.legend.w);return;}
    for(size_t i=0;i<views->count && i<3;i++) {
        HistoryViewSelection group=views->items[i];SDL_FRect box=legend_chip(layout,i);
        SDL_Color color=history_views_color(group);
        fill(app,box,history_views_tint(surface(colors.route_panel_fill,true),color,group.map_visible?.22f:.06f));
        fill(app,(SDL_FRect){box.x,box.y,3,box.h},group.map_visible?color:colors.text_muted);
        char name[129],text[160];history_views_name(app,group,name,sizeof(name));
        snprintf(text,sizeof(text),"%s%s",group.map_visible?"":"Off · ",name);
        ui_draw_text_clipped(&app->renderer,box.x+7,box.y+5,text,colors.text_primary,.65f,box.w-11);
    }
}
// Draws a concise route library, optional tools and an independently bounded selected-route card.
void history_ui_panel_draw(AppState *app) {
    if (app->ui_state_bridge.utility_workspace != APP_UTILITY_HIKING_HISTORY) return;
    struct json_object *current=selected(app);
    if(current&&!visible(app,current)){app->ui_state_bridge.history_selected_id[0]=0;app_save_persisted_view_state(app);}
    SDL_FRect p=pane(app); HistoryLayout layout=history_layout(p,ui.tools,ui.inspect);
    MapForgeThemePalette colors=app_ui_theme_palette();
    fill(app,p,surface(colors.route_panel_fill,false));
    label(app,p.x+12,p.y+9,"YOUR HIKES",colors.text_primary,p.w-24);
    char view_name[160];history_browser_view_label(app,view_name,sizeof(view_name));
    const char *buttons[]={"Import","Fit route","Fit results",app->ui_state_bridge.history_selected_only?"Show results":"Show selected",
        "Edit","Archive","Export GPX","Point -","Point +","Nearby peaks",terrain_style_label(),
        "Reload data","Tools","Details",view_name,ui.archived?"Hide archived":"Show archived","Search","","","", "Plan", "Done", "Partial", "Not done", "Undo", "", app->ui_state_bridge.history_hide_geographic_labels?"Names off":"Names on", "Add peak", "Terrain areas"};
    char search[192]; snprintf(search,sizeof(search),"%s",app->ui_state_bridge.place_search[0]?
        app->ui_state_bridge.place_search:"Search  Ctrl+F");
    buttons[16]=search;
    for(int i=0;i<29;i++) if(layout.actions[i].w>0 && i!=7 && i!=8 && i!=13 && (i<20 || i>24))
        cell(app,layout.actions[i],buttons[i],(i==12&&ui.tools)||(i==14&&ui.browser.open));
    draw_legend(app,layout,colors);
    int total=0; while(row(app,total))total++;
    if(strcmp(ui.scroll_query,app->ui_state_bridge.place_search)) {
        snprintf(ui.scroll_query,sizeof(ui.scroll_query),"%s",app->ui_state_bridge.place_search);ui.scroll=0;
    }
    ui.scroll=history_browser_scroll(layout.list,total,HISTORY_ROW_HEIGHT,ui.scroll,0);
    int first=(int)(ui.scroll/HISTORY_ROW_HEIGHT),last=first;
    if(renderer_set_viewport(&app->renderer,layout.list.x,layout.list.y,layout.list.w,layout.list.h)) {
    for(int i=first;i<total;i++) {
        float y=layout.list.y+i*HISTORY_ROW_HEIGHT-ui.scroll;if(y>=layout.list.y+layout.list.h)break;
        struct json_object *entry=row(app,i);if(!entry)break;last=i+1;
        SDL_FRect box={layout.list.x,y,layout.list.w-10,34};
        bool chosen=entry==selected(app);
        SDL_Color tint=history_views_record_color(app,entry);
        bool map_on=history_views_membership(app,entry,true)!=0;
        SDL_Color row_color=history_views_tint(chosen?surface(colors.button_active_primary,true):surface(colors.route_panel_fill,true),tint,chosen?.28f:map_on?.17f:.05f);
        fill(app,(SDL_FRect){0,box.y-layout.list.y,box.w,box.h},row_color);
        uint32_t membership=history_views_membership(app,entry,false);int stripe=0;
        for(size_t g=0;g<app->ui_state_bridge.history_views.count;g++)if(membership&(UINT32_C(1)<<g)) {
            HistoryViewSelection group=app->ui_state_bridge.history_views.items[g];
            SDL_Color marker=history_views_color(group);
            if(!group.map_visible)marker=history_views_tint(row_color,marker,.3f);
            fill(app,(SDL_FRect){stripe++*3,box.y-layout.list.y,2,box.h},marker);
        }
        float text_left=fmaxf(10,stripe*3+5);
        char name[256];snprintf(name,sizeof(name),"%s",str(entry,"name"));
        char *suffix=strstr(name," — recorded");if(suffix)*suffix=0;
        bool light=(299u*row_color.r+587u*row_color.g+114u*row_color.b)/1000u>160;
        SDL_Color ink=chosen?(light?(SDL_Color){24,43,47,255}:(SDL_Color){242,247,246,255}):colors.text_primary;
        label(app,box.x+text_left,box.y+3,name,ink,fmaxf(1,box.w-text_left-10));
        const char *kind=str(entry,"kind");
        char tag[128];snprintf(tag,sizeof(tag),"%s · %s",progress_label(app,entry),!strcmp(kind,"recorded")?"Recorded track":!strcmp(kind,"recommended")?(*str(entry,"recording_id")?"Route + recording":"Reference route"):!strcmp(kind,"peak")?"Peak":"Visit history");
        ui_draw_text_clipped(&app->renderer,box.x+text_left,box.y+20,tag,chosen?(light?(SDL_Color){61,84,89,255}:(SDL_Color){208,230,233,255}):colors.text_muted,.60f,fmaxf(1,box.w-text_left-10));
    }
    renderer_reset_viewport(&app->renderer);
    }
    SDL_FRect thumb=history_browser_thumb(layout.list,total,HISTORY_ROW_HEIGHT,ui.scroll);
    if(thumb.w)fill(app,thumb,colors.text_muted);
    char line[512];snprintf(line,sizeof(line),"%d–%d of %d hikes · map shows %s",total?first+1:0,last,total,
        app->ui_state_bridge.history_selected_only?"selection":"visible groups");
    if(layout.list.h>0)ui_draw_text_clipped(&app->renderer,layout.list.x,layout.list.y+layout.list.h+4,
        line,colors.text_muted,.60f,layout.list.w);
    SDL_FRect card=layout.detail;fill(app,card,surface(colors.route_panel_fill,true));
    struct json_object *v=selected(app);float x=card.x+10,y=card.y+10;
    if (v) {
    cell(app,layout.actions[13],ui.inspect?"Less":"Details",ui.inspect);
    char name[256];snprintf(name,sizeof(name),"%s",str(v,"name"));char *suffix=strstr(name," — recorded");if(suffix)*suffix=0;
    label(app,x,y,*name?name:"Select a route",colors.text_primary,card.w-92);y+=25;
    const char *kind=str(v,"kind");snprintf(line,sizeof(line),"%s  ·  %s",!strcmp(kind,"recorded")?"Your recording":
        !strcmp(kind,"recommended")?"Reference route":!strcmp(kind,"peak")?peak_state(app,v):"Visit",
        *str(v,"visited_on")?str(v,"visited_on"):"Date unknown");
    label(app,x,y,line,colors.text_muted,card.w-20);y+=24;
    if(!strcmp(kind,"peak"))buttons[21]="Summited";
    for(int i=20;i<=24;i++)cell(app,layout.actions[i],buttons[i],(i==20&&!strcmp(history_progress(app->pins_file.source_json,v),"planned")) || (i==21&&!strcmp(history_progress(app->pins_file.source_json,v),"completed")) || (i==22&&!strcmp(history_progress(app->pins_file.source_json,v),"partial")));
    label(app,x,card.y+card.h-27,progress_label(app,v),progress_color(app,v),card.w-80);
    struct json_object *geometry=v;
    if(*str(v,"recording_id")){struct json_object *a=catalog(app);for(size_t i=0;a&&i<json_object_array_length(a);i++){struct json_object *other=json_object_array_get_idx(a,i);if(!strcmp(str(other,"id"),str(v,"recording_id")))geometry=other;}}
    HistoryTrack *t=track(app,geometry,true);
    if(t && t->point_count) {
        snprintf(line,sizeof(line),"%.2f km  ·  %zu segments",t->distance_m/1000,t->segment_count);
        label(app,x,y,line,colors.text_primary,card.w-20);y+=27;
        if(!ui.inspect && card.h>190) {
            terrain_coverage_text(t->min_lat,t->min_lon,t->max_lat,t->max_lon,line,sizeof(line));
            ui_draw_text_clipped(&app->renderer,x,y,line,colors.text_muted,.60f,card.w-20);y+=18;
            profile_draw(app,t,(SDL_FRect){x,y,card.w-20,28});
        }
        if(ui.inspect && card.h>230) {
            for(int i=7;i<=8;i++)cell(app,layout.actions[i],buttons[i],false);
            HistoryPoint pt=t->points[(size_t)ui.point%t->point_count];
            y=card.y+172;snprintf(line,sizeof(line),"Point %zu / %zu  ·  segment %u",(size_t)ui.point%t->point_count+1,t->point_count,pt.segment+1);
            label(app,x,y,line,colors.text_muted,card.w-20);y+=21;
            snprintf(line,sizeof(line),"%.6f, %.6f",pt.lat,pt.lon);label(app,x,y,line,colors.text_muted,card.w-20);y+=21;
            if(isfinite(pt.elevation)){snprintf(line,sizeof(line),"Elevation %.1f m · source datum unspecified",pt.elevation);label(app,x,y,line,colors.text_muted,card.w-20);}
            if(isfinite(pt.time_s)){time_t epoch=(time_t)pt.time_s;struct tm tm;gmtime_r(&epoch,&tm);strftime(line,sizeof(line),"%Y-%m-%d %H:%M:%S UTC",&tm);label(app,x,card.y+236,line,colors.text_muted,card.w-20);}
        }
    } else if(*str(v,"source_url"))label(app,x,y,"Reference saved · geometry not downloaded",colors.text_muted,card.w-20);
    else if(!strcmp(kind,"peak")) label(app,x,y,"Peak location · Done explicitly means summited",colors.text_muted,card.w-20);
    if(!ui.inspect && !t && card.h>190)label(app,x,y+24,*str(v,"status_evidence")?str(v,"status_evidence"):"Reference data does not assert completion",colors.text_muted,card.w-20);
    } else {
        label(app,x,y,total?"Select a hike or peak":"No entries in this view",colors.text_primary,card.w-20);
        label(app,x,y+30,total?"Click an entry to fit its route and change status.":"Use the reference catalog or Tools / Add peak to plan.",colors.text_muted,card.w-20);
    }
    label(app,p.x+12,p.y+p.h-22,*ui.status?ui.status:"Group tints match routes · progress is shown on each card",colors.text_muted,p.w-24);
    if(ui.peak_picker) {
        SDL_FRect base=field_rect(app,0);fill(app,(SDL_FRect){base.x-8,76,base.w+16,356},colors.route_panel_fill);
        label(app,base.x,82,"Add a peak goal · choose an official name · Esc cancels",colors.text_primary,base.w);
        cell(app,base,*ui.fields[0]?ui.fields[0]:"Type a peak name",true);
        for(int i=0;i<6;i++){char name[128];double lat,lon;if(!terrain_vectors_peak_query(ui.fields[0],ui.peak_offset+i,name,sizeof(name),&lat,&lon))break;cell(app,(SDL_FRect){base.x,base.y+42+i*38,base.w,34},name,false);}
        cell(app,(SDL_FRect){base.x,base.y+280,base.w*.48f,28},"Previous",false);cell(app,(SDL_FRect){base.x+base.w*.52f,base.y+280,base.w*.48f,28},"Next",false);
    }
    if (ui.form) {
        SDL_FRect base = field_rect(app, 0), box = {base.x - 8, 76, base.w + 16, 424};
        renderer_set_draw_color(&app->renderer, colors.route_panel_fill.r, colors.route_panel_fill.g,
                                colors.route_panel_fill.b, 255);
        renderer_fill_rect(&app->renderer, &box);
        label(app, base.x, 82,
              ui.exporting ? "Export original GPX | Esc cancels" : "Add / edit history | Esc cancels",
              colors.text_primary, base.w);
        const char *names[] = {"Absolute GPX path (drop a file here)",
                               "Name",
                               "Hike date (optional)",
                               "Notes",
                               "Peak latitude",
                               "Peak longitude"};
        for (int i = 0; i < 6; i++) {
            SDL_FRect f = field_rect(app, i);
            label(app, f.x, f.y - 13, names[i], colors.text_muted, f.w);
            cell(app, f, *ui.fields[i] ? ui.fields[i] : "", ui.field == i);
        }
        const char *types[] = {"Type: Recorded track", "Type: Reference route",
                               "Type: Manual hike", "Type: Peak", "Type: Peak visit"};
        SDL_FRect f = field_rect(app, 6);
        f.h = 28;
        cell(app, f, types[ui.type], true);
        f.y += 34;
        if (ui.type == 3 || ui.type == 4) {
            SDL_FRect status = f;
            status.x += f.w * .73f;
            status.w = f.w * .27f;
            f.w *= .71f;
            cell(app, status,
                 ui.visit_state == 2   ? "Summited"
                 : ui.visit_state == 1 ? "Visited"
                 : ui.visit_state == 3 ? "Unvisited"
                 : ui.visit_state == 0 ? "Wishlist"
                                       : "Unreviewed",
                 true);
        }
        cell(app, f, ui.exporting ? "Export original" : ui.preview ? "Import / Save" : "Preview", true);
        if (ui.preview) {
            struct json_object *candidate = NULL;
            json_object_object_get_ex(ui.preview, "record", &candidate);
            struct json_object *m = NULL, *pc = NULL, *sc = NULL;
            json_object_object_get_ex(candidate, "metrics", &m);
            json_object_object_get_ex(m, "point_count", &pc);
            json_object_object_get_ex(m, "segment_count", &sc);
            snprintf(line, sizeof(line), "%s | %lld points / %lld segments", str(candidate, "kind"),
                     (long long)json_object_get_int64(pc), (long long)json_object_get_int64(sc));
            label(app, base.x, 478, line, colors.text_primary, base.w);
            struct json_object *ele = NULL, *times = NULL, *invalid = NULL, *reverse = NULL, *steps = NULL;
            json_object_object_get_ex(m, "elevation_points", &ele);
            json_object_object_get_ex(m, "time_points", &times);
            json_object_object_get_ex(m, "invalid_optional_fields", &invalid);
            json_object_object_get_ex(m, "time_reversals", &reverse);
            json_object_object_get_ex(m, "large_steps", &steps);
            snprintf(line, sizeof(line), "Elevation: %lld | source times: %lld | warnings: %lld",
                     (long long)json_object_get_int64(ele), (long long)json_object_get_int64(times),
                     (long long)(json_object_get_int64(invalid) + json_object_get_int64(reverse) +
                                 json_object_get_int64(steps)));
            label(app, base.x, 497, line, colors.text_muted, base.w);
        }
        label(app, base.x, ui.preview ? 518 : 500, ui.status, colors.text_muted, base.w);
    }
    history_browser_draw(&ui.browser,app);
    terrain_ui_draw(app);
}
// Releases the bounded display cache and form allocations at shutdown.
void history_ui_shutdown(void) {
    terrain_ui_close();
    for (size_t i = 0; i < ui.count; i++)
        history_track_free(&ui.tracks[i]);
    clear_preview();
    if(ui.undo_snapshot)json_object_put(ui.undo_snapshot);
    memset(&ui, 0, sizeof(ui));

}

// Labels visible peak markers without inferring completion or changing GNIS positions.
void history_ui_peak_labels_draw(AppState *app) {
 if(app->ui_state_bridge.utility_workspace!=APP_UTILITY_HIKING_HISTORY)return;
 struct json_object *a=catalog(app);SDL_FRect view=app_map_viewport_rect(app);SDL_FRect occupied[24];int count=0;
 for(size_t i=0;a&&i<json_object_array_length(a)&&count<24;i++) {
  struct json_object *v=json_object_array_get_idx(a,i),*lat=NULL,*lon=NULL;
  if(strcmp(str(v,"kind"),"peak") || !history_ui_overlay_visible(app,v))continue;
  struct json_object *archived=NULL;json_object_object_get_ex(v,"archived",&archived);if(!ui.archived&&json_object_get_boolean(archived))continue;
  json_object_object_get_ex(v,"lat",&lat);json_object_object_get_ex(v,"lon",&lon);MercatorMeters m=mercator_from_latlon((LatLon){json_object_get_double(lat),json_object_get_double(lon)});float x,y;app_map_world_to_screen(app,m.x,m.y,&x,&y);
  const char *name=str(v,"name");float width=fminf(210,ui_measure_text_width(name,.75f)+12);SDL_FRect box={x+8,y-13,width,24};
  if(box.x<view.x||box.y<view.y||box.x+box.w>view.x+view.w||box.y+box.h>view.y+view.h-52)continue;
  bool overlap=false;for(int j=0;j<count;j++)if(SDL_HasIntersectionF(&box,&occupied[j]))overlap=true;if(overlap)continue;occupied[count++]=box;
  renderer_set_draw_color(&app->renderer,27,34,38,230);renderer_fill_rect(&app->renderer,&box);ui_draw_text_clipped(&app->renderer,box.x+5,box.y+1,name,(SDL_Color){239,242,240,255},.75f,box.w-10);
 }
}
