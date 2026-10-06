#ifndef KIT_PANE_COMPOSITION_H
#define KIT_PANE_COMPOSITION_H
#include "core_base.h"
#include "core_pane.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KIT_PANE_COMPOSITION_MAX 64u
/* Geometry is in the host's render coordinates. IDs survive resize/reorder.
 * Shells may overlap: last entry is topmost. Half-open hit regions share no edge.
 * No labels, domain objects, render resources or topology are retained. */
typedef struct KitPaneCompositionSpec {
    CorePaneId id;
    CorePaneRect bounds;
    float border, header_height, padding;
    int enabled;
} KitPaneCompositionSpec;
typedef struct KitPaneCompositionEntry {
    CorePaneId id;
    CorePaneRect shell, header, content;
    CorePaneRect visible_shell, visible_header, visible_content;
    int enabled;
} KitPaneCompositionEntry;
typedef struct KitPaneComposition {
    KitPaneCompositionEntry entries[KIT_PANE_COMPOSITION_MAX];
    uint32_t count;
    CorePaneRect viewport;
} KitPaneComposition;
typedef enum KitPaneRegion { KIT_PANE_REGION_NONE, KIT_PANE_REGION_SHELL,
    KIT_PANE_REGION_HEADER, KIT_PANE_REGION_CONTENT } KitPaneRegion;
typedef struct KitPaneHit { CorePaneId id; KitPaneRegion region; } KitPaneHit;
typedef struct KitPanePointerOwner { CorePaneId captured_id; int down; } KitPanePointerOwner;
CoreResult kit_pane_composition_build(KitPaneComposition *out,
    const KitPaneCompositionSpec *specs,uint32_t count,CorePaneRect viewport);
const KitPaneCompositionEntry *kit_pane_composition_find(const KitPaneComposition *view,CorePaneId id);
KitPaneHit kit_pane_composition_hit(const KitPaneComposition *view,float x,float y);
/* Press establishes ownership; release returns that owner even outside its
 * bounds. Modal/splitter takeover, hidden/disabled owner, focus loss cancel it.
 * The host decides whether a release activates a particular control. */
CorePaneId kit_pane_pointer_route(KitPanePointerOwner *owner,
    const KitPaneComposition *view,float x,float y,int pressed,int released,int blocked);
#ifdef __cplusplus
}
#endif
#endif
