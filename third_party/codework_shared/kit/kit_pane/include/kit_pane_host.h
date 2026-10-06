#ifndef KIT_PANE_HOST_H
#define KIT_PANE_HOST_H
#include "kit_pane_composition.h"
#include "core_layout.h"
#ifdef __cplusplus
extern "C" {
#endif
/* A host owns domain objects and render resources. This state owns only stable
 * pane identity, the current geometry snapshot and input ownership. */
typedef enum KitPaneHostEventType {
    KIT_PANE_HOST_MOUNT, KIT_PANE_HOST_UNMOUNT, KIT_PANE_HOST_RESIZE,
    KIT_PANE_HOST_FOCUS, KIT_PANE_HOST_BLUR, KIT_PANE_HOST_CANCEL,
    KIT_PANE_HOST_POINTER_MOVE, KIT_PANE_HOST_POINTER_DOWN,
    KIT_PANE_HOST_POINTER_UP
} KitPaneHostEventType;
typedef struct KitPaneHostEvent {
    KitPaneHostEventType type;
    CorePaneId id;
    KitPaneRegion region;
    float x, y;
} KitPaneHostEvent;
typedef void (*KitPaneHostDispatch)(void *user, const KitPaneHostEvent *event);
typedef struct KitPaneHost {
    KitPaneComposition view;
    KitPanePointerOwner pointer;
    CorePaneId focused_id;
    int blocked;
} KitPaneHost;
void kit_pane_host_init(KitPaneHost *host);
/* Invalidate capture/focus before unmount. Disabled/empty panes cannot own
 * input. Takeover clears ownership; returning does not revive an old press. */
CoreResult kit_pane_host_sync(KitPaneHost *host, const KitPaneComposition *view,
    int blocked, KitPaneHostDispatch dispatch, void *user);
void kit_pane_host_cancel(KitPaneHost *host, KitPaneHostDispatch dispatch, void *user);
CorePaneId kit_pane_host_pointer(KitPaneHost *host, KitPaneHostEventType type,
    float x, float y, KitPaneHostDispatch dispatch, void *user);
CorePaneId kit_pane_host_keyboard_owner(const KitPaneHost *host);

/* Drag-sized transaction nested safely inside an existing authoring session.
 * The caller snapshots/restores its own ratios/topology on cancel; this object
 * snapshots the revision state. Runtime commits apply once; nested commits stay
 * in the draft until the outer authoring session is applied. */
typedef struct KitPaneLayoutEdit {
    CoreLayoutState before;
    int active, owns_authoring, changed;
} KitPaneLayoutEdit;
int kit_pane_layout_edit_begin(KitPaneLayoutEdit *edit, CoreLayoutState *layout);
int kit_pane_layout_edit_update(KitPaneLayoutEdit *edit, CoreLayoutState *layout, int changed);
int kit_pane_layout_edit_commit(KitPaneLayoutEdit *edit, CoreLayoutState *layout);
int kit_pane_layout_edit_cancel(KitPaneLayoutEdit *edit, CoreLayoutState *layout);

#define KIT_PANE_HEADER_ACTION_MAX 8u
typedef struct KitPaneHeaderAction { uint32_t id; float width; int enabled; } KitPaneHeaderAction;
typedef struct KitPaneHeaderSlot { uint32_t id; CorePaneRect bounds; int enabled; } KitPaneHeaderSlot;
typedef struct KitPaneHeaderLayout {
    CorePaneRect title;
    KitPaneHeaderSlot actions[KIT_PANE_HEADER_ACTION_MAX];
    uint32_t count;
} KitPaneHeaderLayout;
/* Right aligned actions in caller priority order. Reserve title_min first;
 * trailing actions that cannot fit are omitted, never overlap the title.
 * Padding/gap/width are in render coordinates (host applies its UI scale).
 * Action activation uses the host's existing kit_ui surface and domain IDs. */
CoreResult kit_pane_header_layout(KitPaneHeaderLayout *out, CorePaneRect header,
    float padding, float gap, float title_min,
    const KitPaneHeaderAction *actions, uint32_t count);
#ifdef __cplusplus
}
#endif
#endif
