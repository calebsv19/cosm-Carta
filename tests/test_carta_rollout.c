#include "app/app_ui_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_persist_state.h"
#include "ui/rollout.h"
#include "ui/font.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool modal;static unsigned saves,persists,invalidations;static float line_width;
bool app_place_tools_active(void){return modal;}
bool history_ui_modal(void){return false;}
void history_ui_cancel_transient(void){}
bool app_pin_editor_save_draft(AppState *a){(void)a;saves++;return true;}
void app_pin_panel_name_edit_deactivate(AppState *a){a->ui_state_bridge.pin_name_edit_active=false;carta_text_end(&a->ui_state_bridge.rollout.pin_name);}
void app_pin_panel_cancel_list_drag(AppState *a){a->ui_state_bridge.pin_list_drag_active=false;}
void app_save_persisted_view_state(const AppState *a){(void)a;persists++;}
void app_tile_viewport_invalidate(AppState *a){(void)a;invalidations++;}
bool app_runtime_apply_window_size(AppState *a,int w,int h){a->width=w;a->height=h;return true;}
MapForgeThemePalette app_ui_theme_palette(void){MapForgeThemePalette p={0};return p;}
void renderer_set_draw_color(Renderer *r,uint8_t x,uint8_t y,uint8_t z,uint8_t w){(void)r;(void)x;(void)y;(void)z;(void)w;}
void renderer_fill_rect(Renderer *r,const SDL_FRect *rect){(void)r;line_width=rect->w;}
void renderer_fill_rounded_rect(Renderer *r,const SDL_FRect *rect,float radius){(void)r;(void)rect;(void)radius;}
bool renderer_set_viewport(Renderer *r,int x,int y,int w,int h){r->viewport_enabled=true;r->viewport_x=x;r->viewport_y=y;r->width=w;r->height=h;return true;}
void renderer_reset_viewport(Renderer *r){r->viewport_enabled=false;}
int ui_measure_text_width(const char *s,float scale){return (int)(strlen(s)*8*scale);}
int ui_font_line_height(float scale){return (int)(20*scale);}
void ui_draw_text_clipped(Renderer *r,int x,int y,const char *s,SDL_Color c,float scale,int w){(void)r;(void)x;(void)y;(void)s;(void)c;(void)scale;(void)w;}
static SDL_Event key(SDL_Keycode k,int repeat){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;e.key.repeat=repeat;return e;}
static SDL_Event pointer(Uint32 type,int x,int y){SDL_Event e={0};e.type=type;if(type==SDL_MOUSEMOTION){e.motion.x=x;e.motion.y=y;}else {e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;}return e;}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO));
    AppState *a=calloc(1,sizeof(*a));assert(a);a->width=1280;a->height=720;
    AppUiState *u=&a->ui_state_bridge;u->left_pane_open=true;u->left_pane_section=APP_LEFT_PANE_SECTION_PINS;
    u->pin_editor_has_draft=u->pin_name_edit_active=true;strcpy(u->pin_editor_name_edit,"Aé🙂Z");
    assert(carta_text_bind(&u->rollout.pin_name,u->pin_editor_name_edit,sizeof(u->pin_editor_name_edit),true));
    SDL_Event e=key(SDLK_LEFT,0);assert(carta_text_hosts_event(a,&e));e=key(SDLK_BACKSPACE,0);assert(carta_text_hosts_event(a,&e));
    assert(!strcmp(u->pin_editor_name_edit,"AéZ"));
    u->native_text_owner=&u->rollout.pin_name;e.type=SDL_TEXTINPUT;strcpy(e.text.text,"X");
    carta_text_window_event(a,&e);assert(carta_text_hosts_event(a,&e));assert(!strcmp(u->pin_editor_name_edit,"AéXZ")); // Exactly one insertion through intake.
    e.type=SDL_TEXTEDITING;strcpy(e.edit.text,"候補");e.edit.length=2;assert(carta_text_hosts_event(a,&e));
    e=key(SDLK_RETURN,0);assert(carta_text_hosts_event(a,&e));assert(!saves);
    e=key(SDLK_ESCAPE,0);assert(carta_text_hosts_event(a,&e));assert(u->pin_name_edit_active);
    e=key(SDLK_RETURN,1);assert(carta_text_hosts_event(a,&e));assert(!saves);
    e=key(SDLK_RETURN,0);assert(carta_text_hosts_event(a,&e));assert(saves==1);
    modal=true;e.type=SDL_TEXTINPUT;strcpy(e.text.text,"wrong");assert(!carta_text_hosts_event(a,&e));assert(!strcmp(u->pin_editor_name_edit,"AéXZ"));modal=false;
    app_pin_panel_name_edit_deactivate(a);u->left_pane_section=APP_LEFT_PANE_SECTION_INGEST;a->ingest_edit_mode=true;strcpy(a->input_root_edit,"/tmp/é");
    assert(carta_text_bind(&u->rollout.ingest_root,a->input_root_edit,sizeof(a->input_root_edit),true));
    e=key(SDLK_BACKSPACE,0);assert(carta_text_hosts_event(a,&e));assert(!strcmp(a->input_root_edit,"/tmp/"));
    e=key(SDLK_RETURN,0);assert(carta_text_hosts_event(a,&e));assert(u->input.enter_pressed);assert(saves==1);
    e=key(SDLK_ESCAPE,0);assert(carta_text_hosts_event(a,&e));assert(!a->ingest_edit_mode && !strcmp(a->input_root_edit,"/tmp/"));
    u->left_pane_section=APP_LEFT_PANE_SECTION_PINS;app_pin_panel_layout(a);
    float divider=u->left_pane_rect.x+u->left_pane_rect.w,old=u->rollout.sidebar_ratio;
    e=pointer(SDL_MOUSEBUTTONDOWN,(int)divider+5,200);assert(carta_panes_event(a,&e));assert(u->rollout.splitter.drag_active && !u->workspace_authoring.active);
    e=pointer(SDL_MOUSEMOTION,(int)divider+100,200);assert(carta_panes_event(a,&e));assert(u->rollout.sidebar_ratio>old && invalidations);
    a->lifetime.persisted_state_ready=true;e=pointer(SDL_MOUSEBUTTONUP,(int)divider+100,200);assert(carta_panes_event(a,&e));assert(!u->rollout.splitter.drag_active && persists==1);
    carta_panes_draw(a);assert(line_width==1);
    divider=u->left_pane_rect.x+u->left_pane_rect.w;e=pointer(SDL_MOUSEBUTTONDOWN,(int)divider+5,200);assert(carta_panes_event(a,&e));
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;assert(!carta_panes_event(a,&e));assert(!u->rollout.splitter.drag_active);
    e=pointer(SDL_MOUSEBUTTONDOWN,(int)divider+10,200);assert(!carta_panes_event(a,&e)); // Thin chrome, generous but bounded hit box.
    modal=true;e=pointer(SDL_MOUSEBUTTONDOWN,(int)divider,200);assert(!carta_panes_event(a,&e));modal=false;
    e=pointer(SDL_MOUSEBUTTONDOWN,100,600);assert(!carta_panes_event(a,&e));assert(u->rollout.pointer.down && u->rollout.pointer.captured_id==2);
    e=pointer(SDL_MOUSEMOTION,1100,600);assert(!carta_panes_event(a,&e));assert(u->rollout.pointer.captured_id==2);
    e=pointer(SDL_MOUSEBUTTONUP,1100,600);assert(!carta_panes_event(a,&e));assert(!u->rollout.pointer.down);
    u->left_pane_open=false;app_pin_panel_layout(a);assert(u->rollout.panes.count==1);
    a->window=SDL_CreateWindow("carta test",0,0,1000,700,SDL_WINDOW_RESIZABLE);assert(a->window);
    assert(carta_window_refresh(a));assert(a->width==1000 && a->height==700);
    u->header_layer_opacity_dragging=true;u->pin_list_drag_active=true;u->input.mouse_buttons=1;
    a->route_state_bridge.dragging_start=true;e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    assert(!carta_window_event(a,&e));assert(!u->header_layer_opacity_dragging && !u->pin_list_drag_active && !u->input.mouse_buttons && !a->route_state_bridge.dragging_start);
    SDL_HideWindow(a->window);assert(!carta_window_refresh(a));e=pointer(SDL_MOUSEBUTTONDOWN,100,200);assert(carta_window_event(a,&e));SDL_ShowWindow(a->window);assert(carta_window_refresh(a));
    e=key(SDLK_F11,1);assert(carta_window_event(a,&e));assert(!(SDL_GetWindowFlags(a->window)&SDL_WINDOW_FULLSCREEN));
    SDL_DestroyWindow(a->window);free(a);SDL_Quit();return 0;
}
