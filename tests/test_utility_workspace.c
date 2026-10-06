#include "app/app_utility_workspace.h"
#include "app/place_runtime.h"
#include "app/app_ui_internal.h"
#include "ui/control_surface.h"
#include "ui/font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool modal;
static unsigned persisted,rounded;
static int caption_x,caption_y,caption_clip;
// Isolates product dialogs/history while routing real toolbar/control contracts.
bool app_place_tools_active(void) { return modal; }
bool history_ui_modal(void) { return false; }
SDL_FRect history_ui_control_rect(AppState *a,unsigned n) { (void)a;(void)n;return (SDL_FRect){0}; }
void history_ui_activate(AppState *a,unsigned n) { (void)a;(void)n;assert(0); }
void app_save_persisted_view_state(const AppState *a) { (void)a;persisted++; }
MapForgeThemePalette app_ui_theme_palette(void) {
    MapForgeThemePalette p={0};p.button_fill=(SDL_Color){210,215,220,255};
    p.button_active_primary=(SDL_Color){30,90,180,255};return p;
}
void renderer_set_draw_color(Renderer *r,uint8_t a,uint8_t b,uint8_t c,uint8_t d) { (void)r;(void)a;(void)b;(void)c;(void)d; }
void renderer_fill_rounded_rect(Renderer *r,const SDL_FRect *p,float radius) { (void)r;assert(p->w>0&&p->h>0&&radius>0);rounded++; }
void renderer_fill_rect(Renderer *r,const SDL_FRect *p) { (void)r;(void)p; }
int ui_measure_text_width(const char *s,float scale) { return (int)(strlen(s)*10*scale); }
int ui_font_line_height(float scale) { return (int)(20*scale); }
void ui_draw_text_clipped(Renderer *r,int x,int y,const char *s,SDL_Color c,float scale,int width) {
    (void)r;(void)s;(void)c;(void)scale;caption_x=x;caption_y=y;caption_clip=width;
}
// Exercises release ownership, geometry/modal cancellation and keyboard semantics.
static void controls_contract(AppState *a) {
    SDL_FRect r=app_utility_workspace_rect(a,1);
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=r.x+5;e.button.y=r.y+5;
    app_utility_workspace_select(a,0);a->lifetime.persisted_state_ready=true;
    unsigned before=persisted;
    assert(carta_ui_controls_event(a,&e));assert(a->ui_state_bridge.utility_workspace==0);
    e.type=SDL_MOUSEBUTTONUP;e.button.x=r.x+r.w+5;
    assert(carta_ui_controls_event(a,&e));assert(a->ui_state_bridge.utility_workspace==0 && persisted==before);
    e.type=SDL_MOUSEBUTTONDOWN;e.button.x=r.x+5;assert(carta_ui_controls_event(a,&e));
    a->width+=50;e.type=SDL_MOUSEBUTTONUP;assert(carta_ui_controls_event(a,&e));
    assert(a->ui_state_bridge.utility_workspace==0);a->width-=50;
    e.type=SDL_MOUSEBUTTONDOWN;assert(carta_ui_controls_event(a,&e));modal=true;
    carta_ui_controls_sync(a);modal=false;e.type=SDL_MOUSEBUTTONUP;assert(carta_ui_controls_event(a,&e));
    assert(a->ui_state_bridge.utility_workspace==0);
    e.type=SDL_MOUSEBUTTONDOWN;assert(carta_ui_controls_event(a,&e));
    SDL_Event loss={0};loss.type=SDL_WINDOWEVENT;loss.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    (void)carta_ui_controls_event(a,&loss);e.type=SDL_MOUSEBUTTONUP;assert(carta_ui_controls_event(a,&e));
    assert(a->ui_state_bridge.utility_workspace==0);
    e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_TAB;e.key.keysym.mod=0;
    assert(carta_ui_controls_event(a,&e));assert(carta_ui_controls_event(a,&e));
    e.key.keysym.sym=SDLK_RETURN;assert(carta_ui_controls_event(a,&e));
    assert(a->ui_state_bridge.utility_workspace==0);e.key.repeat=1;assert(carta_ui_controls_event(a,&e));
    e.type=SDL_KEYUP;e.key.repeat=0;assert(carta_ui_controls_event(a,&e));
    assert(a->ui_state_bridge.utility_workspace==1 && persisted==before+1);
    assert(!carta_ui_controls_event(a,&e));assert(persisted==before+1);
    a->ui_state_bridge.place_search_active=true;e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_TAB;
    assert(!carta_ui_controls_event(a,&e));a->ui_state_bridge.place_search_active=false;
    carta_ui_control_draw(a,(SDL_FRect){10,10,100,26},"Map",false,(KitUiSurfaceKey){0},.8f);
    assert(rounded==2 && caption_x==48 && caption_y==15 && caption_clip==80);
}
// Verifies navigation consumption, collection indexing, responsive hit areas and draft preservation.
int main(void) {
    AppState app = {0};
    app.width = 1000;
    app.height = 720;
    MapForgePin places[3] = {{0}};
    places[0].collection_mask = 1;
    places[1].collection_mask = 2;
    places[2].collection_mask = 8 | 16;
    app.pins_file.pins = places;
    app.pins_file.pin_count = 3;
    strcpy(app.ui_state_bridge.pin_editor_draft.name, "Unsaved draft");
    app.ui_state_bridge.pin_editor_has_draft = true;
    app.ui_state_bridge.pin_name_cursor_index = 5;
    SDL_Event event = {0};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    SDL_FRect rect = app_utility_workspace_rect(&app, 1);
    event.button.x = (int)rect.x + 5;
    event.button.y = (int)rect.y + 5;
    assert(app_utility_workspace_handle_event(&app, &event));
    assert(app.ui_state_bridge.utility_workspace == 0);
    event.type=SDL_MOUSEBUTTONUP;assert(app_utility_workspace_handle_event(&app,&event));
    event.type=SDL_MOUSEBUTTONDOWN;
    assert(app.ui_state_bridge.utility_workspace == 1);
    assert(app.ui_state_bridge.left_pane_open);
    assert(app_utility_pin_count(&app) == 1);
    assert(app_utility_pin_index(&app, 0) == 1);
    app.ui_state_bridge.service_filter = 4;
    assert(app_utility_pin_count(&app) == 0);
    app.ui_state_bridge.service_filter = 2;
    assert(app_utility_pin_count(&app) == 1);
    app_utility_workspace_select(&app, 2);
    assert(app.ui_state_bridge.service_filter == 0);
    assert(app_utility_pin_index(&app, 0) == 2);
    assert(strcmp(app.ui_state_bridge.pin_editor_draft.name, "Unsaved draft") == 0 &&
           app.ui_state_bridge.pin_name_cursor_index == 5);
    app_utility_workspace_select(&app, 3);
    assert(app.ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_INGEST);
    app_utility_workspace_select(&app, 0);
    assert(app_utility_pin_count(&app) == 3);
    strcpy(places[1].id, "service-place");
    strcpy(places[1].address, "100 Example Street");
    app.pins_file.source_json = json_tokener_parse(
        "{\"pets\":[{\"id\":\"pet1\",\"name\":\"Miso\"}],\"pet_places\":[{\"place_id\":\"service-place\","
        "\"pet_id\":\"pet1\"}],\"services\":[{\"place_id\":\"service-place\",\"kind\":\"pet_sitting\"}]}");
    strcpy(app.ui_state_bridge.place_search, "miso");
    assert(app_utility_pin_count(&app) == 1 && app_utility_pin_index(&app, 0) == 1);
    strcpy(app.ui_state_bridge.place_search, "pet_sitting");
    assert(app_utility_pin_count(&app) == 1);
    strcpy(app.ui_state_bridge.place_search, "Example Street");
    assert(app_utility_pin_count(&app) == 1);
    strcpy(app.ui_state_bridge.place_search, "absent");
    assert(app_utility_pin_count(&app) == 0);
    app.ui_state_bridge.place_search[0] = 0;
    json_object_put(app.pins_file.source_json);
    app.pins_file.source_json = NULL;
    assert(app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_draft = places[1];
    app.ui_state_bridge.pin_editor_name_edit[0] = 0;
    assert(!app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_draft.private_flag = !places[1].private_flag;
    assert(app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_draft = places[1];
    strcpy(app.ui_state_bridge.pin_editor_name_edit, "unsaved name");
    assert(app_places_has_unsaved_draft(&app));
    app.ui_state_bridge.pin_editor_name_edit[0] = 0;
    assert(!app_places_has_unsaved_draft(&app));
    event.button.y = 100;
    assert(!app_utility_workspace_handle_event(&app, &event));
    app.width = 500;
    rect = app_utility_workspace_rect(&app, 3);
    assert(rect.x + rect.w <= 500);
    app.width=1000; controls_contract(&app);
    puts("utility workspace: navigation, filters, draft preservation and input consumption passed");
    return 0;
}
