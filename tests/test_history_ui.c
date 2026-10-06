#include "app/app_map_viewport_internal.h"
#include "app/app_persist_state.h"
#include "app/app_ui_internal.h"
#include "app/history.h"
#include "app/history_status.h"
#include "app/history_ui.h"
#include "app/history_layout.h"
#include "app/app_utility_workspace.h"
#include "app/history_browser.h"
// Opaque shared draw backend remains unused by the pure scroll evaluator.
typedef struct KitRenderBackendOps KitRenderBackendOps;
#include "app/terrain.h"
#include "app/terrain_ui.h"
#include "map/mercator.h"
#include "ui/font.h"
#include "ui/control_surface.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned lines, persisted, labels, terrain_removals;
static int first_row_y;
static bool capture_rows;
static SDL_Color draw_color,last_line_color;
// Supplies deterministic font metrics and records native rounded presentation.
static unsigned rounded_controls;
void renderer_fill_rounded_rect(Renderer *r,const SDL_FRect *p,float radius) {
    (void)r;assert(p->w>0 && p->h>0 && radius>0);rounded_controls++;
}
int ui_font_line_height(float scale) { return (int)(20*scale); }
bool app_place_tools_active(void) { return false; }
// Captures the production history geometry and refuses any synthetic gap connector.
void renderer_draw_line(Renderer *r, float x0, float y0, float x1, float y1) {
    (void)r;
    assert(hypotf(x1 - x0, y1 - y0) < 1);
    last_line_color=draw_color;
    lines++;
}
// Replaces presentation while preserving production event and storage behavior.
void renderer_set_draw_color(Renderer *r, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    (void)r;
    draw_color=(SDL_Color){a,b,c,d};
}
void renderer_fill_rect(Renderer *r, const SDL_FRect *p) {
    (void)r;
    (void)p;
}
void ui_draw_text_clipped(Renderer *r, int x, int y, const char *s, SDL_Color c, float scale, int w) {
    (void)r;
    (void)x;
    (void)y;
    (void)s;
    (void)c;
    (void)scale;
    assert(w > 0);
    if(capture_rows && !strcmp(s,"Native import"))first_row_y=y;
    labels++;
}
// Supplies a deterministic native theme for history form verification.
MapForgeThemePalette app_ui_theme_palette(void) {
    MapForgeThemePalette p = {0};
    p.button_fill = (SDL_Color){220, 220, 220, 255};
    return p;
}
// Keeps projected gap distances independently observable at coarse scale.
bool app_map_world_to_viewport_local(const AppState *a, float x, float y, float *sx, float *sy) {
    (void)a;
    *sx = x / 1000;
    *sy = y / 1000;
    return true;
}
SDL_FRect app_map_viewport_rect(const AppState *a) {
    return (SDL_FRect){360, 68, a->width - 360, a->height - 68};
}
void app_save_persisted_view_state(const AppState *a) {
    (void)a;
    persisted++;
}
// Sends a native pointer event to production hit testing.
static void click(AppState *a, int x, int y) {
    SDL_Event e = {0};
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x;
    e.button.y = y;
    assert(history_ui_event(a, &e));
    e.type=SDL_MOUSEBUTTONUP; (void)history_ui_event(a,&e);
}
// Activates the production layout contract without copying its geometry.
static void action(AppState *a, int n) {
    SDL_FRect r=history_ui_action_rect(a,n);assert(r.w>0);click(a,r.x+5,r.y+5);
}
// Exercises the actual chooser through keyboard actions, not hidden old filter buttons.
static void choose_view(AppState *a,int n) {
    action(a,14);assert(history_ui_modal());
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_HOME;assert(history_ui_event(a,&e));e.key.keysym.sym=SDLK_DOWN;
    for(int i=0;i<n;i++)assert(history_ui_event(a,&e));
    e.key.keysym.sym=SDLK_RETURN;e.key.keysym.mod=KMOD_SHIFT;assert(history_ui_event(a,&e));
    e.key.keysym.sym=SDLK_ESCAPE;e.key.keysym.mod=0;assert(history_ui_event(a,&e));assert(!history_ui_modal());
}
// Supplies real viewport clipping ownership without a GPU in this production-event fixture.
bool renderer_set_viewport(Renderer *r,int x,int y,int w,int h) {
    if(w<=0 || h<=0)return false;r->viewport_enabled=true;r->viewport_x=x;r->viewport_y=y;return true;
}
void renderer_reset_viewport(Renderer *r){r->viewport_enabled=false;}
// Types a source/name into the production form, with no repository-side shortcut.
static void type(AppState *a, const char *text) {
    for (size_t n = 0; n < strlen(text);) {
        SDL_Event e = {0};
        e.type = SDL_TEXTINPUT;
        size_t chunk = fmin(20, strlen(text) - n);
        memcpy(e.text.text, text + n, chunk);
        e.text.text[chunk] = 0;
        assert(history_ui_event(a, &e));
        n += chunk;
    }
}
int main(void) {
    char dir[] = "/private/tmp/carta-history-ui-XXXXXX";
    assert(mkdtemp(dir));
    assert(!setenv("MAPFORGE_RUNTIME_DIR", dir, 1));
    SDL_Init(0);
    AppState *a = calloc(1, sizeof(*a));
    assert(a);
    a->width = 1000;
    a->height = 700;
    a->ui_state_bridge.utility_workspace = 2;
    a->ui_state_bridge.left_pane_rect = (SDL_FRect){0, 68, 360, 632};
    map_forge_pins_file_init(&a->pins_file);
    char err[256] = {0};
    assert(
        map_forge_pins_load_workspace(&a->pins_file, a->pins_path, sizeof(a->pins_path), err, sizeof(err)));
    char path[1024];
    snprintf(path, sizeof(path), "%s/source.gpx", dir);
    FILE *f = fopen(path, "w");
    assert(f);
    fputs("<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" version=\"1.1\"><trk><name>Native "
          "import</name><trkseg><trkpt lat=\"0\" lon=\"0\"/><trkpt lat=\"0\" "
          "lon=\"0.001\"/></trkseg><trkseg><trkpt lat=\"1\" lon=\"1\"/><trkpt lat=\"1\" "
          "lon=\"1.001\"/></trkseg></trk></gpx>",
          f);
    fclose(f);
    assert(history_ui_control_rect(a,13).w==0 && history_ui_control_rect(a,20).w==0);
    SDL_FRect import=history_ui_action_rect(a,0);SDL_Event press={0};
    press.type=SDL_MOUSEBUTTONDOWN;press.button.button=SDL_BUTTON_LEFT;
    press.button.x=import.x+4;press.button.y=import.y+4;
    assert(history_ui_event(a,&press));assert(!history_ui_modal());
    press.type=SDL_MOUSEBUTTONUP;press.button.x=999;
    assert(history_ui_event(a,&press));assert(!history_ui_modal());
    action(a, 0);
    assert(history_ui_modal());
    // A visible toolbar tab cannot take over an existing import modal.
    SDL_FRect tab=app_utility_workspace_rect(a,0);press.type=SDL_MOUSEBUTTONDOWN;
    press.button.x=tab.x+4;press.button.y=tab.y+4;
    assert(!carta_ui_controls_event(a,&press));assert(a->ui_state_bridge.utility_workspace==2);

    type(a, path);
    click(a, 200, 435);
    assert(history_ui_modal());
    struct json_object *h = NULL;
    assert(!json_object_object_get_ex(a->pins_file.source_json, "hiking", &h));
    click(a, 200, 435);
    assert(!history_ui_modal());
    assert(json_object_object_get_ex(a->pins_file.source_json, "hiking", &h));
    assert(json_object_array_length(h) == 1);
    history_views_single(a,3,NULL); // An unclassified recording belongs in Activities.
    history_ui_map_draw(a);
    assert(lines == 2);
    assert(persisted > 0);
    history_ui_panel_draw(a);
    assert(labels > 8);
    // Pointer selection fits the complete source bounds with margins inside the actual map viewport.
    HistoryLayout layout=history_layout(a->ui_state_bridge.left_pane_rect,false,false);
    a->view_state_bridge.camera.x=0;a->view_state_bridge.camera.y=0;
    click(a,layout.list.x+5,layout.list.y+5);
    MercatorMeters end=mercator_from_latlon((LatLon){1,1.001});
    double ppm=exp2(a->view_state_bridge.camera.zoom)*256/mercator_world_size_meters();
    SDL_FRect view=app_map_viewport_rect(a);
    assert(end.x*ppm<=view.w-79 && end.y*ppm<=view.h-99);
    assert(a->view_state_bridge.camera.x>0 && a->view_state_bridge.camera.y>0);
    for(int i=0;i<17;i++) {SDL_FRect b=layout.actions[i];if(b.w>0)assert(b.x>=0&&b.x+b.w<=360&&b.y+b.h<=700);}

    // Selection changes invalidate a press rather than acting on a replacement record.
    char old_id[64];snprintf(old_id,sizeof(old_id),"%s",a->ui_state_bridge.history_selected_id);
    SDL_FRect details=history_ui_control_rect(a,13);assert(details.w>0);
    SDL_Event pending={0};pending.type=SDL_MOUSEBUTTONDOWN;pending.button.button=SDL_BUTTON_LEFT;
    pending.button.x=details.x+4;pending.button.y=details.y+4;
    assert(history_ui_event(a,&pending));assert(a->ui_state_bridge.controls.interaction.captured_id);
    a->ui_state_bridge.history_selected_id[0]=0;
    pending.type=SDL_MOUSEBUTTONUP;assert(history_ui_event(a,&pending));
    assert(!a->ui_state_bridge.controls.interaction.captured_id);
    snprintf(a->ui_state_bridge.history_selected_id,64,"%s",old_id);
    assert(history_ui_control_rect(a,7).w==0); // Details was not toggled by that release.
    assert(rounded_controls>0);

    // Escape cancels an edited form without changing the persisted catalog.
    action(a, 12); action(a, 4);
    assert(history_ui_modal());
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_ESCAPE;
    assert(history_ui_event(a, &e));
    assert(!history_ui_modal());
    assert(json_object_array_length(h) == 1);
    // Editing a sourced peak name cannot silently create a visit assertion.
    struct json_object *q =
        json_tokener_parse("{\"operation\":\"history.preview\",\"request_id\":\"native-peak\",\"kind\":"
                           "\"peak\",\"name\":\"Unreviewed peak\",\"lat\":47.4,\"lon\":-121.5}");
    struct json_object *preview = history_command(a->pins_path, &a->pins_file, q), *ct = NULL, *st = NULL;
    json_object_object_get_ex(preview, "candidate_token", &ct);
    json_object_object_get_ex(preview, "store_token", &st);
    json_object_object_add(q, "operation", json_object_new_string("history.commit"));
    json_object_object_add(q, "candidate_token", json_object_get(ct));
    json_object_object_add(q, "expected_store_token", json_object_get(st));
    struct json_object *saved = history_command(a->pins_path, &a->pins_file, q), *record = NULL, *id = NULL;
    json_object_object_get_ex(saved, "record", &record);
    json_object_object_get_ex(record, "id", &id);
    snprintf(a->ui_state_bridge.history_selected_id, 64, "%s", json_object_get_string(id));
    json_object_put(q);
    json_object_put(preview);
    json_object_put(saved);
    action(a, 4);
    assert(history_ui_modal());
    click(a, 200, 166);
    type(a, " revised");
    click(a, 200, 435);
    click(a, 200, 435);
    assert(!history_ui_modal());
    json_object_object_get_ex(a->pins_file.source_json, "hiking", &h);
    record = json_object_array_get_idx(h, 1);
    struct json_object *state = NULL;
    assert(!json_object_object_get_ex(record, "state", &state));
    // Startup restoration must replace the unrelated street camera with the selected peak extent.
    a->view_state_bridge.camera.x = 0;
    a->view_state_bridge.camera.y = 0;
    history_ui_restore_view(a);
    MercatorMeters restored = mercator_from_latlon((LatLon){47.4, -121.5});
    assert(fabs(a->view_state_bridge.camera.x - restored.x) < 2);
    assert(fabs(a->view_state_bridge.camera.y - restored.y) < 2);
    // All/Completed/To do filters use personal progress; unknown peaks never bypass the map filter.
    history_views_single(a,1,NULL);
    assert(!history_ui_record_visible(a,record));assert(!history_ui_overlay_visible(a,record));
    history_views_single(a,0,NULL);
    assert(!history_ui_record_visible(a,record));
    history_views_single(a,5,NULL);
    assert(history_ui_record_visible(a,record));assert(!history_ui_overlay_visible(a,record));
    action(a,20); // Explicitly plan the selected peak using native guarded commands.
    json_object_object_get_ex(a->pins_file.source_json,"hiking",&h);record=json_object_array_get_idx(h,1);
    assert(!strcmp(history_progress(a->pins_file.source_json,record),"planned"));
    choose_view(a,2);assert(history_ui_record_visible(a,record));assert(history_ui_overlay_visible(a,record));
    action(a,21); // Completion moves the entry out of To do and clears its hidden selection.
    json_object_object_get_ex(a->pins_file.source_json,"hiking",&h);record=json_object_array_get_idx(h,1);
    assert(!strcmp(history_progress(a->pins_file.source_json,record),"completed"));assert(!a->ui_state_bridge.history_selected_id[0]);assert(!history_ui_overlay_visible(a,record));
    action(a,24); // Undo targets its guarded identity even after filtering hid it.
    json_object_object_get_ex(a->pins_file.source_json,"hiking",&h);record=json_object_array_get_idx(h,1);
    assert(!strcmp(history_progress(a->pins_file.source_json,record),"planned"));
    choose_view(a,1);assert(!history_ui_record_visible(a,record));choose_view(a,0);assert(history_ui_record_visible(a,record));
    action(a,26);assert(!history_ui_geographic_labels_enabled(a));action(a,26);assert(history_ui_geographic_labels_enabled(a));
    // A peak goal can be selected by official name without asking the user for coordinates.
    action(a,27);assert(history_ui_modal());click(a,200,158);assert(!history_ui_modal());
    json_object_object_get_ex(a->pins_file.source_json,"hiking",&h);assert(json_object_array_length(h)==3);
    struct json_object *goal=json_object_array_get_idx(h,2);assert(!strcmp(history_progress(a->pins_file.source_json,goal),"planned"));
    // Actual modal controls capture map input and require a second action before removal.
    action(a,28);assert(terrain_ui_active()&&history_ui_modal());
    SDL_FRect button=terrain_ui_control_rect(a,TERRAIN_UI_REMOVE,1);click(a,button.x+3,button.y+3);
    button=terrain_ui_control_rect(a,TERRAIN_UI_CANCEL,0);click(a,button.x+3,button.y+3);assert(!terrain_removals);
    button=terrain_ui_control_rect(a,TERRAIN_UI_REMOVE,1);click(a,button.x+3,button.y+3);
    button=terrain_ui_control_rect(a,TERRAIN_UI_CONFIRM,0);click(a,button.x+3,button.y+3);assert(terrain_removals==1);
    history_ui_panel_draw(a);
    button=terrain_ui_control_rect(a,TERRAIN_UI_CLOSE,0);click(a,button.x+3,button.y+3);assert(!terrain_ui_active());
    action(a,28);button=terrain_ui_control_rect(a,TERRAIN_UI_SHOW,1);click(a,button.x+3,button.y+3);
    assert(!terrain_ui_active()&&fabs(a->view_state_bridge.camera.x-3000)<1);
    // Shared precise scrolling moves fractional pixels, clamps at both ends and handles shrinking views.
    SDL_FRect viewport={0,0,200,100};
    assert(fabsf(history_browser_scroll(viewport,10,38,0,-.125f)-3)<.001f);
    assert(history_browser_scroll(viewport,10,38,0,100)==0);
    assert(history_browser_scroll(viewport,10,38,0,-100)==280);
    assert(history_browser_scroll(viewport,1,38,280,0)==0);
    assert(history_browser_row(viewport,20,38,20,20)==1);
    assert(history_browser_row(viewport,20,38,20,101)==-1);
    SDL_Event wheel={0};wheel.type=SDL_MOUSEWHEEL;wheel.wheel.y=-1;wheel.wheel.preciseY=-.125f;
    assert(fabsf(history_browser_wheel(&wheel)+.125f)<.001f);
    wheel.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;assert(fabsf(history_browser_wheel(&wheel)-.125f)<.001f);
    // Build unique sources only in this disposable repository and import via production guarded commands.
    for(int i=0;i<9;i++) {
        FILE *source=fopen(path,"w");assert(source);
        fprintf(source,"<gpx version=\"1.1\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><name>Scroll %d</name><trkseg><trkpt lat=\"0\" lon=\"0\"/><trkpt lat=\"0\" lon=\"0.001\"/></trkseg></trk></gpx>",i);fclose(source);
        struct json_object *q=json_object_new_object();char req[64];snprintf(req,sizeof(req),"multiple-%d",i);
        json_object_object_add(q,"operation",json_object_new_string("history.preview"));
        json_object_object_add(q,"request_id",json_object_new_string(req));json_object_object_add(q,"kind",json_object_new_string("recorded"));
        json_object_object_add(q,"source_path",json_object_new_string(path));
        struct json_object *r=history_command(a->pins_path,&a->pins_file,q),*ct=NULL,*st=NULL;
        assert(json_object_object_get_ex(r,"candidate_token",&ct));assert(json_object_object_get_ex(r,"store_token",&st));
        json_object_object_add(q,"candidate_token",json_object_get(ct));json_object_object_add(q,"expected_store_token",json_object_get(st));
        json_object_object_add(q,"operation",json_object_new_string("history.commit"));json_object_put(r);
        r=history_command(a->pins_path,&a->pins_file,q);struct json_object *ok=NULL;assert(json_object_object_get_ex(r,"ok",&ok)&&json_object_get_boolean(ok));json_object_put(r);json_object_put(q);
    }
    choose_view(a,3);a->ui_state_bridge.history_selected_only=false;a->ui_state_bridge.history_selected_id[0]=0;
    action(a,11); // Explicit reload clears display cache and failures; one new asset per map frame.
    lines=0;history_ui_map_draw(a);assert(a->history_pending && lines==2);
    for(int i=0;i<12 && a->history_pending;i++)history_ui_map_draw(a);
    assert(!a->history_pending);lines=0;history_ui_map_draw(a);assert(lines==11 && !a->history_pending);
    action(a,3);assert(a->ui_state_bridge.history_selected_only);lines=0;history_ui_map_draw(a);assert(!lines&&!a->history_pending);
    action(a,2);assert(!a->ui_state_bridge.history_selected_only);lines=0;history_ui_map_draw(a);assert(lines==11);
    // Missing original is terminal so Results cannot keep an idle app spinning.
    json_object_object_get_ex(a->pins_file.source_json,"hiking",&h);struct json_object *bad=json_object_array_get_idx(h,json_object_array_length(h)-1);
    json_object_object_add(bad,"asset_hash",json_object_new_string("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
    action(a,11);for(int i=0;i<15;i++)history_ui_map_draw(a);assert(!a->history_pending);
    // Precise event produces a partial row; clicking that clipped row resolves the correct stable ID.
    action(a,12); // close tools
    layout=history_layout(a->ui_state_bridge.left_pane_rect,false,false);
    a->ui_state_bridge.input.mouse_x=layout.list.x+15;a->ui_state_bridge.input.mouse_y=layout.list.y+15;
    capture_rows=true;history_ui_panel_draw(a);int initial=first_row_y;
    wheel.wheel.direction=SDL_MOUSEWHEEL_NORMAL;assert(history_ui_event(a,&wheel));history_ui_panel_draw(a);
    assert(first_row_y==initial-3);
    click(a,layout.list.x+15,layout.list.y+2);struct json_object *first=json_object_array_get_idx(h,0),*first_id=NULL;assert(json_object_object_get_ex(first,"id",&first_id));assert(!strcmp(a->ui_state_bridge.history_selected_id,json_object_get_string(first_id)));
    capture_rows=false;
    // Selecting a saved list clears the previous progress/source filter without rewriting status.
    struct json_object *groups=json_tokener_parse("[{\"id\":\"fixture-list\",\"name\":\"Future hikes\"}]");
    json_object_object_add(a->pins_file.source_json,"hiking_collections",groups);
    json_object_object_add(bad,"collections",json_tokener_parse("[\"fixture-list\"]"));
    json_object_object_add(bad,"progress",json_object_new_string("not_done"));
    choose_view(a,1);choose_view(a,7);assert(a->ui_state_bridge.history_filter==0);
    assert(!strcmp(a->ui_state_bridge.history_collection_id,"fixture-list"));assert(history_ui_record_visible(a,bad));
    choose_view(a,1);assert(!a->ui_state_bridge.history_collection_id[0]);assert(!history_ui_record_visible(a,bad));
    // Actual modal additive toggles preserve union membership; map visibility is independent.
    history_views_single(a,1,NULL);
    json_object_object_add(first,"progress",json_object_new_string("partial"));
    json_object_object_add(first,"status_origin",json_object_new_string("user"));
    json_object_object_add(first,"collections",json_tokener_parse("[\"fixture-list\"]"));
    action(a,14);SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_HOME;
    assert(history_ui_event(a,&key));key.key.keysym.sym=SDLK_DOWN;
    for(int i=0;i<2;i++)assert(history_ui_event(a,&key));
    key.key.keysym.sym=SDLK_SPACE;assert(history_ui_event(a,&key));assert(history_ui_modal());
    assert(a->ui_state_bridge.history_views.count==2);
    assert(history_ui_record_visible(a,first)); // User partial is included by To do + partial.
    key.key.keysym.sym=SDLK_END;assert(history_ui_event(a,&key));key.key.keysym.sym=SDLK_SPACE;assert(history_ui_event(a,&key));
    assert(a->ui_state_bridge.history_views.count==3 && history_ui_record_visible(a,bad));
    assert(history_views_membership(a,first,false)==6); // Both To do and named list, exactly once.
    SDL_Color before_color=history_views_record_color(a,first);
    key.key.keysym.sym=SDLK_m;assert(history_ui_event(a,&key));
    assert(history_ui_record_visible(a,bad) && !history_ui_overlay_visible(a,bad));
    assert(history_ui_overlay_visible(a,first)); // Overlap remains shown by To do.
    assert(history_views_membership(a,first,true)==2);
    key.key.keysym.sym=SDLK_m;assert(history_ui_event(a,&key));
    key.key.keysym.sym=SDLK_c;assert(history_ui_event(a,&key));
    HistoryViewSet expected=a->ui_state_bridge.history_views;
    struct json_object *prefs=json_object_new_object();history_views_save(a,prefs);
    memset(&a->ui_state_bridge.history_views,0,sizeof(HistoryViewSet));history_views_load(a,prefs);
    assert(a->ui_state_bridge.history_views.count==expected.count);
    for(size_t i=0;i<expected.count;i++) {
        assert(a->ui_state_bridge.history_views.items[i].filter==expected.items[i].filter);
        assert(!strcmp(a->ui_state_bridge.history_views.items[i].collection,expected.items[i].collection));
        assert(a->ui_state_bridge.history_views.items[i].color==expected.items[i].color);
        assert(a->ui_state_bridge.history_views.items[i].map_visible==expected.items[i].map_visible);
    }
    SDL_Color after_color=history_views_record_color(a,first);
    assert(before_color.r==after_color.r && before_color.g==after_color.g && before_color.b==after_color.b);
    // Hide To do to transfer overlap color ownership to the remaining visible saved list.
    a->ui_state_bridge.history_views.items[1].map_visible=false;
    after_color=history_views_record_color(a,first);
    SDL_Color group_color=history_views_color(a->ui_state_bridge.history_views.items[2]);
    assert(after_color.r==group_color.r && after_color.g==group_color.g && after_color.b==group_color.b);
    key.key.keysym.sym=SDLK_ESCAPE;assert(history_ui_event(a,&key));
    // Map rendering uses that same tint, and hiding every group ends finite preparation.
    a->ui_state_bridge.history_selected_id[0]=0;lines=0;history_ui_map_draw(a);
    assert(lines==2 && last_line_color.r==group_color.r && last_line_color.g==group_color.g && last_line_color.b==group_color.b);
    a->ui_state_bridge.history_views.items[2].map_visible=false;lines=0;history_ui_map_draw(a);assert(!lines && !a->history_pending);
    // A linked reference/activity draws once across overlapping views; the selected tint paints last.
    struct json_object *paired=json_object_new_object();
    json_object_object_add(paired,"id",json_object_new_string("fixture-paired-route"));
    json_object_object_add(paired,"name",json_object_new_string("Paired route"));
    json_object_object_add(paired,"kind",json_object_new_string("recommended"));
    json_object_object_add(paired,"progress",json_object_new_string("completed"));
    json_object_object_add(paired,"recording_id",json_object_get(first_id));
    struct json_object *field=NULL;
    assert(json_object_object_get_ex(first,"metrics",&field));json_object_object_add(paired,"metrics",json_object_get(field));
    assert(json_object_object_get_ex(first,"asset_hash",&field));json_object_object_add(paired,"asset_hash",json_object_get(field));
    json_object_array_add(h,paired);
    history_views_single(a,3,NULL);history_views_toggle(a,history_views_choice(a,1));
    lines=0;history_ui_map_draw(a);assert(lines==10); // two first-source legs + eight other available sources.
    snprintf(a->ui_state_bridge.history_selected_id,sizeof(a->ui_state_bridge.history_selected_id),"fixture-paired-route");
    a->ui_state_bridge.history_views.items[0].map_visible=false;
    SDL_Color selected_color=history_views_record_color(a,paired);
    lines=0;history_ui_map_draw(a);assert(lines==2 && last_line_color.r==selected_color.r && last_line_color.g==selected_color.g && last_line_color.b==selected_color.b);
    a->ui_state_bridge.history_views.items[0].map_visible=true;
    // Completed-only selected route differs from the recorded-only sources behind it.
    a->ui_state_bridge.history_views.items[0].filter=1;a->ui_state_bridge.history_views.items[0].color=7;
    a->ui_state_bridge.history_views.items[1].filter=3;a->ui_state_bridge.history_views.items[1].color=3;
    selected_color=history_views_record_color(a,paired);
    lines=0;history_ui_map_draw(a);assert(lines==10 && last_line_color.r==selected_color.r && last_line_color.g==selected_color.g && last_line_color.b==selected_color.b);
    // Explicit empty selection survives restoration; malformed/duplicate entries cannot add hidden intersections.
    json_object_object_add(prefs,"history_views",json_tokener_parse("[]"));history_views_load(a,prefs);
    assert(a->ui_state_bridge.history_views.initialized && !a->ui_state_bridge.history_views.count && !history_ui_record_visible(a,first));
    json_object_object_add(prefs,"history_views",json_tokener_parse("[{\"filter\":3},{\"filter\":3},{\"filter\":99},{\"filter\":-1,\"collection\":\"\"}]"));
    history_views_load(a,prefs);assert(a->ui_state_bridge.history_views.count==1);
    json_object_put(prefs);
    // Legacy selected collection migrates by stable ID even if its old status filter was combined.
    memset(&a->ui_state_bridge.history_views,0,sizeof(HistoryViewSet));a->ui_state_bridge.history_filter=1;
    snprintf(a->ui_state_bridge.history_collection_id,sizeof(a->ui_state_bridge.history_collection_id),"fixture-list");
    prefs=json_object_new_object();history_views_load(a,prefs);json_object_put(prefs);
    assert(a->ui_state_bridge.history_views.count==1 && a->ui_state_bridge.history_views.items[0].filter==-1);
    assert(history_ui_record_visible(a,bad) && !strcmp(history_progress(a->pins_file.source_json,bad),"not_done"));
    // A narrow history pane still consumes list controls and renders bounded labels.
    a->width = 500;
    a->ui_state_bridge.left_pane_rect.w = 225;
    history_ui_panel_draw(a);
    history_ui_shutdown();
    map_forge_pins_file_free(&a->pins_file);
    free(a);
    SDL_Quit();
    puts("native history: explicit preview/import, gap-free geometry, cancel preservation, stable selection "
         "and narrow-pane rendering passed");
    return 0;
}

// Supplies terrain presentation preferences without introducing GPU state to history tests.
void terrain_cycle(void){}
const char *terrain_style_label(void){return "Terrain: Topo";}
bool terrain_reload(AppState *a){(void)a;return true;}
bool app_map_world_to_screen(const AppState *a,float x,float y,float *sx,float *sy){return app_map_world_to_viewport_local(a,x,y,sx,sy);}
int ui_measure_text_width(const char *s,float z){return (int)(strlen(s)*8*z);}

// Keeps the history test independent of terrain residency.
bool terrain_active(AppState *a){(void)a;return false;}

// Supplies a deterministic official peak picker without loading GPU terrain in UI tests.
bool terrain_vectors_peak_query(const char *query,size_t ordinal,char *name,size_t cap,double *lat,double *lon){(void)query;if(ordinal)return false;snprintf(name,cap,"Fixture goal peak");*lat=47.4;*lon=-121.5;return true;}

// Keeps geographic coverage explicit without loading terrain in the production-control test.
// No terrain is installed in this fixture; source geometry must still draw and fit.
bool terrain_vectors_covers(double a,double b,double c,double d){(void)a;(void)b;(void)c;(void)d;return false;}

// Supplies a protected baseline to exercise actual terrain panel navigation without file mutations.
const TerrainCatalog *terrain_area_catalog(void){
    static TerrainCatalog c={.count=2,.areas={{.id="baseline",.name="Baseline",.extent={-1000,-1000,1000,1000}},
        {.id="fixture",.name="Fixture terrain",.extent={2000,2000,4000,4000},.removable=true}}};return &c;
}
bool terrain_remove_area(AppState *a,size_t i,char *e,size_t n){(void)a;assert(i==1);terrain_removals++;snprintf(e,n,"Fixture refused");return false;}
void terrain_coverage_text(double a,double b,double c,double d,char *out,size_t cap){(void)a;(void)b;(void)c;(void)d;snprintf(out,cap,"Background unavailable");}

// Shared scroll evaluation does not use drawing backends in this renderer-stub fixture.
const KitRenderBackendOps *kit_render_backend_null_ops(void){return NULL;}
const KitRenderBackendOps *kit_render_backend_vk_ops(void){return NULL;}
