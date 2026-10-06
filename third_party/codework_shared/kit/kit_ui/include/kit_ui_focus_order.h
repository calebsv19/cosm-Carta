#ifndef KIT_UI_FOCUS_ORDER_H
#define KIT_UI_FOCUS_ORDER_H
#include "kit_ui_surface.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum KitUiFocusKind { KIT_UI_FOCUS_BUTTON, KIT_UI_FOCUS_FIELD } KitUiFocusKind;
typedef struct KitUiFocusItem { KitUiSurfaceKey key; KitUiFocusKind kind; int enabled; } KitUiFocusItem;
typedef struct KitUiFocusOrder { KitUiSurfaceKey key; KitUiFocusKind kind; uint32_t scope; int active; } KitUiFocusOrder;
/* Host supplies visible order in one scope. No text buffer or action dispatch.
 * Invalid/duplicate descriptors leave state unchanged. Sync clears vanished
 * owners; scope changes reset ownership; stepping wraps over enabled entries. */
CoreResult kit_ui_focus_order_sync(KitUiFocusOrder *focus,uint32_t scope,const KitUiFocusItem *items,uint32_t count);
CoreResult kit_ui_focus_order_step(KitUiFocusOrder *focus,uint32_t scope,const KitUiFocusItem *items,uint32_t count,int backwards);
#ifdef __cplusplus
}
#endif
#endif
