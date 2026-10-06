#include "kit_pane_composition.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    KitPaneCompositionSpec s[]={ {11,{0,0,100,100},1,20,4,1},{22,{100,0,100,100},1,0,4,1} };
    KitPaneComposition v={0};assert(kit_pane_composition_build(&v,s,2,(CorePaneRect){0,0,180,80}).code==CORE_OK);
    assert(v.entries[0].header.height==20&&v.entries[0].content.y==25&&v.entries[0].content.width==90);
    assert(v.entries[1].visible_content.width==75&&v.entries[1].visible_content.height==75);
    assert(kit_pane_composition_hit(&v,99,50).id==11);
    assert(kit_pane_composition_hit(&v,100,50).id==22);
    assert(!kit_pane_composition_hit(&v,180,50).id);
    assert(kit_pane_composition_hit(&v,5,5).region==KIT_PANE_REGION_HEADER);
    KitPanePointerOwner owner={0};assert(kit_pane_pointer_route(&owner,&v,50,50,1,0,0)==11);
    assert(kit_pane_pointer_route(&owner,&v,150,50,0,0,0)==11);
    assert(kit_pane_pointer_route(&owner,&v,190,50,0,1,0)==11&&!owner.down);
    assert(!kit_pane_pointer_route(&owner,&v,150,50,0,1,0));
    assert(kit_pane_pointer_route(&owner,&v,50,50,1,0,0)==11);
    assert(!kit_pane_pointer_route(&owner,&v,150,50,0,0,1)&&!owner.down);
    (void)kit_pane_pointer_route(&owner,&v,50,50,1,0,0);s[0].enabled=0;
    assert(kit_pane_composition_build(&v,s,2,v.viewport).code==CORE_OK);
    assert(!kit_pane_pointer_route(&owner,&v,50,50,0,1,0));
    KitPaneComposition before=v;s[1].id=11;
    assert(kit_pane_composition_build(&v,s,2,v.viewport).code!=CORE_OK&&!memcmp(&v,&before,sizeof(v)));
    s[1].id=22;s[1].bounds.x=NAN;
    assert(kit_pane_composition_build(&v,s,2,v.viewport).code!=CORE_OK&&!memcmp(&v,&before,sizeof(v)));
    s[1].bounds=(CorePaneRect){1,1,0,0};s[1].padding=100;
    assert(kit_pane_composition_build(&v,s,2,v.viewport).code==CORE_OK&&v.entries[1].content.width==0);
    s[0].bounds=(CorePaneRect){0,0,100,100};s[0].enabled=1;s[1].bounds=s[0].bounds;s[1].enabled=0;
    assert(kit_pane_composition_build(&v,s,2,v.viewport).code==CORE_OK&&!kit_pane_composition_hit(&v,50,50).id);
    assert(kit_pane_composition_build(&v,NULL,0,v.viewport).code==CORE_OK&&!v.count);
    puts("kit_pane composition: geometry, clipping, seams, occlusion, capture, cancel, atomic failure passed");return 0;
}
