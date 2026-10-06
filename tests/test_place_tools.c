#include "app/app_pin_panel_internal.h"
#include "app/app_place_tools_internal.h"
#include "app/place_repository.h"
#include "app/place_runtime.h"
#include "ui/font.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int ui_measure_text_width(const char *s,float scale){return (int)(strlen(s)*10*scale);}
int ui_font_line_height(float scale){return (int)(20*scale);}
void renderer_fill_rounded_rect(Renderer *r,const SDL_FRect *p,float radius){(void)r;assert(p->w>0 && radius>0);}
bool renderer_set_viewport(Renderer *r,int x,int y,int w,int h){r->viewport_enabled=true;r->viewport_x=x;r->viewport_y=y;r->width=w;r->height=h;return true;}
void renderer_reset_viewport(Renderer *r){r->viewport_enabled=false;}
// Captures draw-label geometry while leaving tests independent of a desktop window/GPU.
typedef struct Label {
    char text[1024];
    int x, y;
} Label;
static Label labels[300];
static size_t label_count;
SDL_FRect history_ui_form_control_rect(AppState *a,unsigned i){(void)a;(void)i;return (SDL_FRect){0};}
void history_ui_form_control_activate(AppState *a,unsigned i){(void)a;(void)i;assert(0);}
static unsigned route_opens;
bool history_ui_modal(void){return false;}
SDL_FRect history_ui_control_rect(AppState *a,unsigned i){(void)a;(void)i;return (SDL_FRect){0};}
void history_ui_activate(AppState *a,unsigned i){(void)a;(void)i;assert(0);}
void app_save_persisted_view_state(const AppState *a){(void)a;}
void app_places_begin_route_action(AppState *a,bool save){(void)a;(void)save;}
SDL_FRect app_places_route_button_rect(const AppState *a,uint32_t i){(void)a;(void)i;return (SDL_FRect){0};}
// Supplies predictable readable colors for native-dialog rendering tests.
MapForgeThemePalette app_ui_theme_palette(void) {
    MapForgeThemePalette p = {0};
    p.button_fill = (SDL_Color){20, 30, 40, 255};
    p.button_active_primary = (SDL_Color){220, 230, 240, 255};
    p.text_primary = (SDL_Color){230, 240, 250, 255};
    return p;
}
// Records the actual text geometry emitted by production draw functions.
void ui_draw_text_clipped(Renderer *r, int x, int y, const char *text, SDL_Color c, float scale, int width) {
    (void)r;
    (void)c;
    (void)scale;
    (void)width;
    assert(label_count < 300);
    Label *v = &labels[label_count++];
    snprintf(v->text, sizeof(v->text), "%s", text);
    v->x = x;
    v->y = y;
}
// Replaces GPU presentation only; all event/dialog/repository code remains production code.
void renderer_set_draw_color(Renderer *r, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
    (void)r;
    (void)red;
    (void)green;
    (void)blue;
    (void)alpha;
}
void renderer_fill_rect(Renderer *r, const SDL_FRect *rect) {
    (void)r;
    (void)rect;
}
void renderer_draw_rect(Renderer *r, const SDL_FRect *rect) {
    (void)r;
    (void)rect;
}
// Keeps the test's saved selection consistent with the real editor contract.
void app_pin_editor_select_saved_pin(AppState *app, int index) {
    app->ui_state_bridge.pin_selected_index = index;
    app->ui_state_bridge.pin_editor_draft = app->pins_file.pins[index];
    app->ui_state_bridge.pin_editor_has_draft = true;
    app->ui_state_bridge.pin_editor_is_new = false;
    snprintf(app->ui_state_bridge.pin_editor_name_edit, sizeof(app->ui_state_bridge.pin_editor_name_edit),
             "%s", app->pins_file.pins[index].name);
}
// Captures status without mutating records.
void app_pin_editor_set_status(AppState *app, const char *message) {
    snprintf(app->ui_state_bridge.pin_editor_status, sizeof(app->ui_state_bridge.pin_editor_status), "%s",
             message);
}
void app_pin_editor_set_statusf(AppState *app, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(app->ui_state_bridge.pin_editor_status, sizeof(app->ui_state_bridge.pin_editor_status), format,
              args);
    va_end(args);
}
// Models only the route invalidation and selection adapter seam.
void app_places_invalidate_route(AppState *app) {
    app->ui_state_bridge.pin_route_start_id[0] = 0;
    app->ui_state_bridge.pin_route_goal_id[0] = 0;
}
void app_pin_panel_name_edit_deactivate(AppState *app) { app->ui_state_bridge.pin_name_edit_active = false; }
void app_pin_editor_cancel_draft(AppState *app) { app->ui_state_bridge.pin_editor_has_draft = false; }
void app_places_open_route(AppState *app, const char *id) {
    (void)app;
    assert(id[0]);
    ++route_opens;
}
SDL_FRect app_map_viewport_rect(const AppState *app) {
    return (SDL_FRect){0, 68, (float)app->width, (float)app->height - 68};
}
float app_clampf(float value, float min, float max) { return value < min ? min : value > max ? max : value; }
// Sends actual SDL events through the production native-dialog input adapter.
static void click(AppState *app, float x, float y) {
    SDL_Event e = {0};
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (int)x;
    e.button.y = (int)y;
    assert(app_place_tools_handle_event(app, &e));
    e.type=SDL_MOUSEBUTTONUP;(void)app_place_tools_handle_event(app,&e);
}
static void key(AppState *app, SDL_Keycode code, SDL_Keymod mod) {
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = code;
    e.key.keysym.mod = mod;
    assert(app_place_tools_handle_event(app, &e));
}
static void text(AppState *app, const char *s) {
    SDL_Event e = {0};
    e.type = SDL_TEXTINPUT;
    snprintf(e.text.text, sizeof(e.text.text), "%s", s);
    assert(app_place_tools_handle_event(app, &e));
}
// Resolves visible controls by the production draw output, avoiding implementation-only hit coordinates.
static Label label(const char *name) {
    for (size_t i = 0; i < label_count; ++i)
        if (!strcmp(labels[i].text, name))
            return labels[i];
    fprintf(stderr, "Missing control: %s\n", name);
    abort();
}
static void draw(AppState *app) {
    label_count = 0;
    app_place_tools_draw(app);
}
static void button(AppState *app, const char *name) {
    Label v = label(name);
    click(app, v.x + 8, v.y + 5);
}
// Executes a production structured request and requires its success.
static struct json_object *command(AppState *app, const char *json) {
    struct json_object *q = json_tokener_parse(json),
                       *r = map_forge_places_command(app->pins_path, &app->pins_file, q), *ok = NULL;
    json_object_object_get_ex(r, "ok", &ok);
    if (!json_object_get_boolean(ok))
        fprintf(stderr, "%s\n", json_object_to_json_string(r));
    assert(json_object_get_boolean(ok));
    json_object_put(q);
    return r;
}
// Verifies cancel/save, draft guards, roles, filters, route management and recovery dialog sequencing.
int main(void) {
    char runtime[] = "/private/tmp/mapforge-ui-test-XXXXXX";
    assert(mkdtemp(runtime));
    assert(!setenv("MAPFORGE_RUNTIME_DIR", runtime, 1));
    SDL_Init(SDL_INIT_EVENTS);
    AppState app = {0};
    app.width = 1280;
    app.height = 720;
    char error[256] = {0};
    assert(map_forge_pins_load_workspace(&app.pins_file, app.pins_path, sizeof(app.pins_path), error,
                                         sizeof(error)));
    struct json_object *q =
        json_tokener_parse("{\"request_id\":\"ui-home\",\"name\":\"Synthetic home\",\"address\":\"A supplied "
                           "address\",\"location\":{\"lat\":47.66,\"lon\":-122.31,\"source\":\"synthetic\","
                           "\"quality\":\"manual\",\"input_address\":\"Original\",\"matched_address\":"
                           "\"Matched\"},\"pet\":{\"name\":\"Miso\"},\"services\":[\"dog_walking\"]}");
    json_object_object_add(q, "operation", json_object_new_string("intake.preview"));
    struct json_object *pv = map_forge_places_command(app.pins_path, &app.pins_file, q), *v = NULL;
    json_object_object_get_ex(pv, "store_token", &v);
    json_object_object_add(q, "expected_store_token", json_object_get(v));
    json_object_object_get_ex(pv, "candidate_token", &v);
    json_object_object_add(q, "candidate_token", json_object_get(v));
    json_object_object_add(q, "operation", json_object_new_string("intake.commit"));
    struct json_object *r = map_forge_places_command(app.pins_path, &app.pins_file, q);
    json_object_put(r);
    json_object_put(pv);
    json_object_put(q);
    app_pin_editor_select_saved_pin(&app, 0);
    uint64_t signature = app.pins_file.source_signature;
    app_place_tools_open(&app, 1);
    assert(app_place_tools_active());
    draw(&app);
    assert(label("Original address: Original").text[0]);
    assert(label("Map/provider address: Matched").text[0]);
    Label field = label("Home name");
    click(&app, field.x + 10, field.y + 26);
    key(&app, SDLK_a, KMOD_CTRL);
    text(&app, "Canceled edit");
    button(&app, "CANCEL");
    assert(!app_place_tools_active() && app.pins_file.source_signature == signature);
    app_place_tools_open(&app, 1);
    draw(&app);
    field = label("Home name");
    click(&app, field.x + 10, field.y + 26);
    key(&app, SDLK_a, KMOD_CTRL);
    text(&app, "Renamed home");
    button(&app, "SAVE DETAILS");
    assert(!app_place_tools_active());
    assert(!strcmp(app.pins_file.pins[0].name, "Renamed home"));
    assert(app.pins_file.pins[0].lat == 47.66);
    app.ui_state_bridge.pin_editor_draft.private_flag = false;
    app_place_tools_open(&app, 1);
    assert(!app_place_tools_active());
    app_pin_editor_select_saved_pin(&app, 0);
    app_place_tools_open(&app, 3);
    draw(&app);
    button(&app, "CREATE BACKUP");
    assert(tools.source[0]);
    draw(&app);
    button(&app, "PREVIEW RESTORE");
    assert(tools.restore);
    draw(&app);
    assert(label("RESTORE BACKUP").text[0]);
    button(&app, "RESTORE BACKUP");
    assert(!tools.restore);
    draw(&app);
    button(&app, "CLOSE");
    assert(!app_place_tools_active());
    app_pin_editor_select_saved_pin(&app, 0);
    app_place_tools_open(&app, 3);
    draw(&app);
    field = label("SQLite backup to restore (absolute path)");
    click(&app, field.x + 10, field.y + 26);
    key(&app, SDLK_a, KMOD_CTRL);
    for (int i = 0; i < 10; ++i)
        text(&app, "abcdefghijklmnop");
    assert(strlen(tools.edit) == 160);
    key(&app, SDLK_BACKSPACE, KMOD_NONE);
    assert(strlen(tools.edit) == 159);
    key(&app, SDLK_RETURN, KMOD_NONE);
    assert(strlen(tools.source) == 159);
    app_place_tools_close();
    app.ui_state_bridge.left_pane_open = true;
    app.ui_state_bridge.left_pane_section = APP_LEFT_PANE_SECTION_PINS;
    app.ui_state_bridge.pin_pane_status_rect = (SDL_FRect){10, 350, 340, 32};
    app.ui_state_bridge.left_pane_rect = (SDL_FRect){0, 68, 360, 652};
    label_count = 0;
    app_place_tools_draw_toolbar(&app);
    button(&app, "SITTING");
    assert(app_utility_pin_count(&app) == 0);
    label_count = 0;
    app_place_tools_draw_toolbar(&app);
    button(&app, "ALL PLACES");
    assert(app_utility_pin_count(&app) == 1);
    label_count = 0;
    app_place_tools_draw_toolbar(&app);
    button(&app, "CENTER HOME");
    assert(app.view_state_bridge.camera.zoom == 17);
    /* Native route selection/rename/archive is independent of graph availability. */
    struct json_object *plans =
        json_tokener_parse("[{\"id\":\"synthetic-route\",\"name\":\"A synthetic "
                           "route\",\"from_place_id\":\"missing-a\",\"to_place_id\":\"missing-b\",\"region\":"
                           "\"unavailable\",\"mode\":\"walk\",\"objective\":\"distance\"}]");
    json_object_object_add(app.pins_file.source_json, "route_plans", plans);
    assert(map_forge_pins_save(app.pins_path, &app.pins_file, error, sizeof(error)));
    app_place_tools_open(&app, 2);
    draw(&app);
    button(&app, "A synthetic route");
    key(&app, SDLK_a, KMOD_CTRL);
    text(&app, "Renamed route");
    draw(&app);
    button(&app, "RENAME");
    draw(&app);
    button(&app, "Renamed route");
    button(&app, "ARCHIVE");
    draw(&app);
    button(&app, "ACTIVE ROUTES");
    draw(&app);
    button(&app, "[archived] Renamed route");
    draw(&app);
    button(&app, "RESTORE");
    draw(&app);
    button(&app, "Renamed route");
    button(&app, "OPEN");
    assert(route_opens == 1 && !app_place_tools_active());
    r = command(&app, "{\"operation\":\"routes.list\"}");
    struct json_object *rows = NULL, *intent = NULL;
    json_object_object_get_ex(r, "routes", &rows);
    assert(json_object_array_length(rows) == 1);
    json_object_object_get_ex(json_object_array_get_idx(rows, 0), "saved_intent", &intent);
    assert(!strcmp(json_object_get_string(json_object_object_get(intent, "from_place_id")), "missing-a"));
    json_object_put(r);
    app_place_tools_close();
    map_forge_pins_file_free(&app.pins_file);
    SDL_Quit();
    printf("native place tools: cancel/save, provenance, draft guards, restore preview, long text, filters "
           "and focus passed\n");
    return 0;
}
