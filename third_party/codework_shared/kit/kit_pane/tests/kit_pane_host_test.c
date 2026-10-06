#include "kit_pane_host.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static KitPaneHostEvent events[64];static unsigned count;
static void dispatch(void *u,const KitPaneHostEvent *e){(void)u;assert(count<64);events[count++]=*e;}
int main(void){
 KitPaneHost h;kit_pane_host_init(&h);KitPaneComposition v;
 KitPaneCompositionSpec specs[]={{7,{0,0,100,100},1,20,2,1},{19,{100,0,100,100},1,20,2,1}};
 assert(kit_pane_composition_build(&v,specs,2,(CorePaneRect){0,0,200,100}).code==CORE_OK);
 assert(kit_pane_host_sync(&h,&v,0,dispatch,0).code==CORE_OK);assert(count==2);
 assert(kit_pane_host_pointer(&h,KIT_PANE_HOST_POINTER_DOWN,10,30,dispatch,0)==7);
 assert(kit_pane_host_keyboard_owner(&h)==7);
 assert(kit_pane_host_pointer(&h,KIT_PANE_HOST_POINTER_MOVE,140,40,dispatch,0)==7);
 assert(kit_pane_host_pointer(&h,KIT_PANE_HOST_POINTER_UP,140,40,dispatch,0)==7);
 assert(events[count-1].region==KIT_PANE_REGION_NONE);
 kit_pane_host_pointer(&h,KIT_PANE_HOST_POINTER_DOWN,10,30,dispatch,0);
 unsigned before=count;kit_pane_host_sync(&h,&v,1,dispatch,0);
 assert(events[before].type==KIT_PANE_HOST_CANCEL&&events[before+1].type==KIT_PANE_HOST_BLUR);
 kit_pane_host_sync(&h,&v,0,dispatch,0);
 assert(!kit_pane_host_pointer(&h,KIT_PANE_HOST_POINTER_UP,10,30,dispatch,0));
 kit_pane_host_pointer(&h,KIT_PANE_HOST_POINTER_DOWN,140,30,dispatch,0);
 specs[1].enabled=0;kit_pane_composition_build(&v,specs,2,(CorePaneRect){0,0,200,100});
 kit_pane_host_sync(&h,&v,0,dispatch,0);assert(!h.pointer.down&&!h.focused_id);
 kit_pane_composition_build(&v,specs,1,(CorePaneRect){0,0,200,100});
 kit_pane_host_sync(&h,&v,0,dispatch,0);assert(events[count-1].type==KIT_PANE_HOST_UNMOUNT);
 specs[0].bounds.width=80;kit_pane_composition_build(&v,specs,1,(CorePaneRect){0,0,200,100});
 kit_pane_host_sync(&h,&v,0,dispatch,0);assert(events[count-1].type==KIT_PANE_HOST_RESIZE);
 CoreLayoutState s;core_layout_state_init(&s);KitPaneLayoutEdit edit={0};
 assert(kit_pane_layout_edit_begin(&edit,&s));assert(!kit_pane_layout_edit_begin(&edit,&s));
 assert(kit_pane_layout_edit_commit(&edit,&s)&&s.active_revision==1&&!s.rebuild_required);
 assert(kit_pane_layout_edit_begin(&edit,&s));assert(kit_pane_layout_edit_update(&edit,&s,1));
 assert(kit_pane_layout_edit_cancel(&edit,&s)&&s.active_revision==1&&!s.has_pending_changes);
 assert(kit_pane_layout_edit_begin(&edit,&s));kit_pane_layout_edit_update(&edit,&s,1);
 assert(kit_pane_layout_edit_commit(&edit,&s)&&s.active_revision==2&&s.rebuild_required);
 core_layout_acknowledge_rebuild(&s);assert(core_layout_enter_authoring(&s));core_layout_mark_draft_changed(&s);
 CoreLayoutState outer=s;assert(kit_pane_layout_edit_begin(&edit,&s));kit_pane_layout_edit_update(&edit,&s,1);
 assert(kit_pane_layout_edit_cancel(&edit,&s)&&!memcmp(&s,&outer,sizeof(s)));
 assert(kit_pane_layout_edit_begin(&edit,&s));kit_pane_layout_edit_update(&edit,&s,1);
 assert(kit_pane_layout_edit_commit(&edit,&s)&&s.mode==CORE_LAYOUT_MODE_AUTHORING&&s.active_revision==2);
 assert(core_layout_apply_authoring(&s)&&s.active_revision==3);
 assert(kit_pane_layout_edit_begin(&edit,&s));core_layout_apply_external_revision(&s,CORE_LAYOUT_REVISION_SOURCE_SNAPSHOT_IMPORT,1,0);
 assert(!kit_pane_layout_edit_cancel(&edit,&s)); /* stale edit must not overwrite new revision */
 KitPaneHeaderLayout slots;KitPaneHeaderAction actions[]={{23,40,1},{4,45,0}};
 assert(kit_pane_header_layout(&slots,(CorePaneRect){0,0,200,30},4,6,50,actions,2).code==CORE_OK);
 assert(slots.count==2&&slots.actions[0].id==23&&!slots.actions[1].enabled);
 assert(slots.title.x+slots.title.width+6==slots.actions[0].bounds.x);
 assert(slots.actions[1].bounds.x+45==196);
 assert(kit_pane_header_layout(&slots,(CorePaneRect){0,0,110,30},4,6,50,actions,2).code==CORE_OK&&slots.count==1);
 assert(kit_pane_header_layout(&slots,(CorePaneRect){0,0,20,4},4,6,50,actions,2).code==CORE_OK&&slots.count==0);
 KitPaneHeaderLayout saved=slots;actions[1].id=23;
 assert(kit_pane_header_layout(&slots,(CorePaneRect){0,0,200,30},4,6,50,actions,2).code!=CORE_OK&&!memcmp(&saved,&slots,sizeof(slots)));
 puts("kit_pane host lifecycle / takeover / nested revision transactions / bounded header slots pass");return 0;
}
