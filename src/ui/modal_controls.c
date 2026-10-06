#include "ui/modal_controls.h"
#include "kit_ui_interaction_sdl.h"
#include <string.h>
void carta_modal_controls_begin(KitUiSurface *s,unsigned scope){kit_ui_surface_begin(s,scope);}
void carta_modal_controls_add(KitUiSurface *s,uint64_t action,SDL_FRect r,SDL_FRect clip,bool enabled) {
    (void)kit_ui_surface_register(s,(KitUiSurfaceKey){1,action},(KitRenderRect){r.x,r.y,r.w,r.h},
        &(KitRenderRect){clip.x,clip.y,clip.w,clip.h},enabled,NULL);
}
void carta_modal_controls_end(KitUiSurface *s) {
    bool changed=s->count!=s->next_count;
    for(unsigned i=0;!changed && i<s->count;i++)changed=
        memcmp(&s->controls[i],&s->next_controls[i],sizeof(s->controls[i])) ||
        memcmp(&s->keys[i],&s->next_keys[i],sizeof(s->keys[i]));
    if(changed) {
        KitUiInteractionResult result;KitUiInteractionEvent cancel={.type=KIT_UI_INTERACTION_SYNC};
        (void)kit_ui_interaction_route(&s->interaction,NULL,0,&cancel,&result);s->activation_count=0;
    }
    if(kit_ui_surface_end(s).code!=CORE_OK)kit_ui_surface_set_scope(s,0);
}
bool carta_modal_controls_event(KitUiSurface *s,const SDL_Event *e,uint64_t *action,uint32_t *modifiers) {
    *action=UINT64_MAX;*modifiers=0;
    KitUiInteractionEvent input;KitUiInteractionResult result;
    if(!kit_ui_interaction_event_from_sdl(e,&input))return false;
    if(kit_ui_surface_route(s,&input,&result).code!=CORE_OK)return false;
    if(result.activated_id) {
        KitUiSurfaceKey key;
        if(kit_ui_surface_key(s,result.activated_id,&key) && kit_ui_surface_take_activation(s,result.activated_id))
            {*action=key.value;*modifiers=input.modifiers;}
    }
    return result.consumed!=0;
}
