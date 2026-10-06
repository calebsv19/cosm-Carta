#ifndef KIT_UI_SURFACE_H
#define KIT_UI_SURFACE_H

#include "kit_ui_interaction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional caller-owned snapshots for immediate drawing hosts. Keys contain
 * host meaning; IDs are opaque collision-free handles, never row positions. */
#define KIT_UI_SURFACE_ACTIVATION_MAX 32u
typedef struct KitUiSurfaceKey { uint32_t domain; uint64_t value; } KitUiSurfaceKey;
typedef struct KitUiSurface {
    KitUiInteractionContext interaction;
    KitUiInteractionControl controls[KIT_UI_INTERACTION_CONTROL_MAX];
    KitUiSurfaceKey keys[KIT_UI_INTERACTION_CONTROL_MAX];
    KitUiInteractionControl next_controls[KIT_UI_INTERACTION_CONTROL_MAX];
    KitUiSurfaceKey next_keys[KIT_UI_INTERACTION_CONTROL_MAX];
    uint32_t count, next_count, scope, next_id;
    uint32_t activations[KIT_UI_SURFACE_ACTIVATION_MAX], activation_count;
    int collecting, claimed;
    CoreResult collection_result;
} KitUiSurface;

void kit_ui_surface_reset(KitUiSurface *surface);
void kit_ui_surface_set_scope(KitUiSurface *surface, uint32_t scope);
void kit_ui_surface_begin(KitUiSurface *surface, uint32_t scope);
/* out_control is optional; registration failures poison this collection. */
CoreResult kit_ui_surface_register(KitUiSurface *surface, KitUiSurfaceKey key,
    KitRenderRect bounds, const KitRenderRect *clip, int enabled,
    KitUiInteractionControl *out_control);
CoreResult kit_ui_surface_end(KitUiSurface *surface);
CoreResult kit_ui_surface_route(KitUiSurface *surface,
    const KitUiInteractionEvent *event, KitUiInteractionResult *out_result);
int kit_ui_surface_take_activation(KitUiSurface *surface, uint32_t id);
int kit_ui_surface_key(const KitUiSurface *surface, uint32_t id, KitUiSurfaceKey *out_key);
int kit_ui_surface_pending(const KitUiSurface *surface);

#ifdef __cplusplus
}
#endif

#endif
