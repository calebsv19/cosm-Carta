#include "kit_pane_host.h"
#include <string.h>

static int eligible(const KitPaneCompositionEntry *p) {
    return p && p->enabled && p->visible_shell.width > 0 && p->visible_shell.height > 0;
}
static void emit(KitPaneHostDispatch dispatch, void *user, KitPaneHostEventType type,
                 CorePaneId id, KitPaneRegion region, float x, float y) {
    if (dispatch && id) {
        KitPaneHostEvent e = {type, id, region, x, y};
        dispatch(user, &e);
    }
}
void kit_pane_host_init(KitPaneHost *h) { if (h) memset(h, 0, sizeof(*h)); }
void kit_pane_host_cancel(KitPaneHost *h, KitPaneHostDispatch dispatch, void *user) {
    if (!h) return;
    CorePaneId capture = h->pointer.captured_id, focus = h->focused_id;
    h->pointer = (KitPanePointerOwner){0}; h->focused_id = 0;
    emit(dispatch,user,KIT_PANE_HOST_CANCEL,capture,KIT_PANE_REGION_NONE,0,0);
    emit(dispatch,user,KIT_PANE_HOST_BLUR,focus,KIT_PANE_REGION_NONE,0,0);
}
CoreResult kit_pane_host_sync(KitPaneHost *h, const KitPaneComposition *view,
    int blocked, KitPaneHostDispatch dispatch, void *user) {
    if (!h || !view || view->count > KIT_PANE_COMPOSITION_MAX)
        return (CoreResult){CORE_ERR_INVALID_ARG,"invalid pane host snapshot"};
    /* Composition snapshots must come from the validated builder. */
    if (blocked) kit_pane_host_cancel(h,dispatch,user);
    else {
        if (!eligible(kit_pane_composition_find(view,h->pointer.captured_id))) {
            CorePaneId old=h->pointer.captured_id; h->pointer=(KitPanePointerOwner){0};
            emit(dispatch,user,KIT_PANE_HOST_CANCEL,old,KIT_PANE_REGION_NONE,0,0);
        }
        if (!eligible(kit_pane_composition_find(view,h->focused_id))) {
            CorePaneId old=h->focused_id; h->focused_id=0;
            emit(dispatch,user,KIT_PANE_HOST_BLUR,old,KIT_PANE_REGION_NONE,0,0);
        }
    }
    for (uint32_t i=0;i<h->view.count;i++) {
        CorePaneId id=h->view.entries[i].id;
        if (!kit_pane_composition_find(view,id))
            emit(dispatch,user,KIT_PANE_HOST_UNMOUNT,id,KIT_PANE_REGION_NONE,0,0);
    }
    KitPaneComposition old=h->view;
    h->view=*view; h->blocked=!!blocked;
    for (uint32_t i=0;i<view->count;i++) {
        const KitPaneCompositionEntry *p=&view->entries[i];
        const KitPaneCompositionEntry *was=kit_pane_composition_find(&old,p->id);
        if (!was) emit(dispatch,user,KIT_PANE_HOST_MOUNT,p->id,KIT_PANE_REGION_NONE,0,0);
        else if (memcmp(&was->shell,&p->shell,sizeof(p->shell)) ||
                 memcmp(&was->visible_content,&p->visible_content,sizeof(p->visible_content)))
            emit(dispatch,user,KIT_PANE_HOST_RESIZE,p->id,KIT_PANE_REGION_NONE,0,0);
    }
    return core_result_ok();
}
CorePaneId kit_pane_host_pointer(KitPaneHost *h, KitPaneHostEventType type,
    float x, float y, KitPaneHostDispatch dispatch, void *user) {
    if (!h || (type!=KIT_PANE_HOST_POINTER_MOVE && type!=KIT_PANE_HOST_POINTER_DOWN &&
               type!=KIT_PANE_HOST_POINTER_UP)) return 0;
    CorePaneId id=kit_pane_pointer_route(&h->pointer,&h->view,x,y,
        type==KIT_PANE_HOST_POINTER_DOWN,type==KIT_PANE_HOST_POINTER_UP,h->blocked);
    if (type==KIT_PANE_HOST_POINTER_DOWN && id!=h->focused_id) {
        CorePaneId old=h->focused_id; h->focused_id=id;
        emit(dispatch,user,KIT_PANE_HOST_BLUR,old,KIT_PANE_REGION_NONE,x,y);
        emit(dispatch,user,KIT_PANE_HOST_FOCUS,id,KIT_PANE_REGION_NONE,x,y);
    }
    KitPaneHit hit=kit_pane_composition_hit(&h->view,x,y);
    emit(dispatch,user,type,id,hit.id==id?hit.region:KIT_PANE_REGION_NONE,x,y);
    return id;
}
CorePaneId kit_pane_host_keyboard_owner(const KitPaneHost *h) {
    return h && !h->blocked ? h->focused_id : 0;
}

int kit_pane_layout_edit_begin(KitPaneLayoutEdit *e, CoreLayoutState *s) {
    if (!e || !s || e->active || !s->active_revision) return 0;
    e->before=*s; e->owns_authoring=s->mode==CORE_LAYOUT_MODE_RUNTIME; e->changed=0;
    if (e->owns_authoring && !core_layout_enter_authoring(s)) return 0;
    e->active=1; return 1;
}
int kit_pane_layout_edit_update(KitPaneLayoutEdit *e, CoreLayoutState *s, int changed) {
    if (!e || !s || !e->active || s->mode!=CORE_LAYOUT_MODE_AUTHORING ||
        s->active_revision!=e->before.active_revision) return 0;
    if (changed) { if (!core_layout_mark_draft_changed(s)) return 0; e->changed=1; }
    return 1;
}
int kit_pane_layout_edit_commit(KitPaneLayoutEdit *e, CoreLayoutState *s) {
    if (!kit_pane_layout_edit_update(e,s,0)) return 0;
    if (e->owns_authoring && !core_layout_apply_authoring(s)) return 0;
    e->active=0; return 1;
}
int kit_pane_layout_edit_cancel(KitPaneLayoutEdit *e, CoreLayoutState *s) {
    if (!kit_pane_layout_edit_update(e,s,0)) return 0;
    *s=e->before; e->active=0; return 1;
}
