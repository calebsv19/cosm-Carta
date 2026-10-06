#ifndef KIT_UI_FOCUS_SCOPE_H
#define KIT_UI_FOCUS_SCOPE_H
#include "kit_ui_surface.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One active modal over one host scope. Hosts own text target validity, drawing
 * and lifecycle. Save semantic button keys, never snapshot-local handles. */
typedef struct KitUiFocusScope {
    uint32_t scope, return_scope;
    KitUiSurfaceKey return_button;
    int initialized, modal, restore_pending;
} KitUiFocusScope;
CoreResult kit_ui_focus_scope_sync(KitUiFocusScope *focus,KitUiSurface *surface,uint32_t scope,int modal);
void kit_ui_focus_scope_restore(KitUiFocusScope *focus,KitUiSurface *surface);
void kit_ui_focus_scope_cancel(KitUiFocusScope *focus,KitUiSurface *surface);
void kit_ui_focus_scope_text(KitUiSurface *surface);
#ifdef __cplusplus
}
#endif
#endif
