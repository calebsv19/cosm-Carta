#include "app/app_place_tools_internal.h"
#include "ui/control_surface.h"
// Visible-only inventory; app-owned policy still receives accepted control geometry.
void tools_button(AppState *app,SDL_FRect r,const char *label,bool active,uint64_t action) {
    if(tools.button_count>=256)return;
    unsigned i=tools.button_count++;
    tools.buttons[i]=r;tools.button_keys[i]=((uint64_t)tools.kind<<32)|action;
    carta_ui_control_draw(app,r,label,active,(KitUiSurfaceKey){CARTA_CONTROL_PLACE_FORM,tools.button_keys[i]},.78f);
}
bool app_place_tools_text_active(void){return tools.kind && tools.field>=0;}
unsigned app_place_tools_control_count(void) {return tools.kind && tools.controls_ready?tools.button_count:0;}
SDL_FRect app_place_tools_control_rect(unsigned i) {return i<app_place_tools_control_count()?tools.buttons[i]:(SDL_FRect){0};}
uint64_t app_place_tools_control_key(unsigned i) {return i<app_place_tools_control_count()?tools.button_keys[i]:0;}
void app_place_tools_control_activate(AppState *app,uint64_t key) {
    for(unsigned i=0;i<app_place_tools_control_count();i++)if(tools.button_keys[i]==key) {
        tools.controls_ready=false;
        tools_activate(app,key&UINT64_C(0xffffffff));
        return;
    }
}
