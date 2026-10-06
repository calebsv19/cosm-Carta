#include "kit_ui_focus_scope.h"
#include <string.h>
void kit_ui_focus_scope_text(KitUiSurface *s) { if(s) {s->interaction.focused_id=0;s->interaction.key_owner_id=0;} }
void kit_ui_focus_scope_cancel(KitUiFocusScope *f,KitUiSurface *s) {
    if(f)memset(f,0,sizeof(*f));
    /* Surface preserves outstanding release ownership when scope changes. */
    if(s) {KitUiInteractionEvent e={0};KitUiInteractionResult r;e.type=KIT_UI_INTERACTION_CANCEL;(void)kit_ui_surface_route(s,&e,&r);}
}
CoreResult kit_ui_focus_scope_sync(KitUiFocusScope *f,KitUiSurface *s,uint32_t scope,int modal) {
    if(!f||!s)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid focus scope"};
    if(!f->initialized) {f->initialized=1;f->scope=scope;f->modal=modal;kit_ui_surface_set_scope(s,scope);return core_result_ok();}
    if(scope==f->scope && !!modal==f->modal)return core_result_ok();
    if(modal && f->modal)return (CoreResult){CORE_ERR_INVALID_ARG,"nested modal scope requires explicit host policy"};
    if(modal) {
        f->return_scope=f->scope;f->return_button=(KitUiSurfaceKey){0,0};
        (void)kit_ui_surface_key(s,s->interaction.focused_id,&f->return_button);f->restore_pending=0;
    } else if(f->modal)f->restore_pending=scope==f->return_scope && f->return_button.domain!=0;
    else {f->restore_pending=0;f->return_button=(KitUiSurfaceKey){0,0};}
    kit_ui_surface_set_scope(s,scope);f->scope=scope;f->modal=!!modal;return core_result_ok();
}
void kit_ui_focus_scope_restore(KitUiFocusScope *f,KitUiSurface *s) {
    if(!f||!s||!f->restore_pending||s->collecting)return;
    for(uint32_t i=0;i<s->count;++i) if(s->controls[i].enabled &&
        s->keys[i].domain==f->return_button.domain && s->keys[i].value==f->return_button.value) {
        s->interaction.focused_id=s->controls[i].id;break;
    }
    f->restore_pending=0;
}
