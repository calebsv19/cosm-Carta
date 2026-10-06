#include "kit_ui_surface.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static KitUiInteractionControl add(KitUiSurface *s, uint32_t domain, uint64_t key, float x, int enabled) {
    KitUiInteractionControl c;
    assert(kit_ui_surface_register(s,(KitUiSurfaceKey){domain,key},(KitRenderRect){x,0,20,20},NULL,enabled,&c).code==CORE_OK);
    return c;
}
static KitUiInteractionResult route(KitUiSurface *s, KitUiInteractionEventType type, float x) {
    KitUiInteractionResult r;
    KitUiInteractionEvent e={.type=type,.x=x,.y=5};
    assert(kit_ui_surface_route(s,&e,&r).code==CORE_OK);
    return r;
}
int main(void) {
    KitUiSurface optional={0};
    kit_ui_surface_begin(&optional,1u);
    assert(kit_ui_surface_register(&optional,(KitUiSurfaceKey){1u,1u},
        (KitRenderRect){0,0,20,20},NULL,1,NULL).code==CORE_OK);
    assert(kit_ui_surface_end(&optional).code==CORE_OK && optional.count==1u);

    KitUiSurface s={0};
    kit_ui_surface_begin(&s,1);
    KitUiInteractionControl a=add(&s,1,UINT64_C(0x100000001),0,1);
    KitUiInteractionControl b=add(&s,1,1,30,1);
    assert(a.id!=b.id);
    assert(kit_ui_surface_end(&s).code==CORE_OK);
    assert(!route(&s,KIT_UI_INTERACTION_POINTER_UP,5).activated_id);
    route(&s,KIT_UI_INTERACTION_POINTER_DOWN,5);
    assert(!route(&s,KIT_UI_INTERACTION_POINTER_UP,35).activated_id);
    route(&s,KIT_UI_INTERACTION_POINTER_DOWN,5);
    kit_ui_surface_begin(&s,1);
    KitUiInteractionControl moved_b=add(&s,1,1,0,1);
    KitUiInteractionControl moved_a=add(&s,1,UINT64_C(0x100000001),30,1);
    assert(moved_a.id==a.id && moved_b.id==b.id);
    assert(kit_ui_surface_end(&s).code==CORE_OK);
    assert(route(&s,KIT_UI_INTERACTION_POINTER_UP,35).activated_id==a.id);
    route(&s,KIT_UI_INTERACTION_POINTER_DOWN,5); route(&s,KIT_UI_INTERACTION_POINTER_UP,5);
    assert(!kit_ui_surface_take_activation(&s,b.id));
    assert(kit_ui_surface_take_activation(&s,a.id));
    assert(!kit_ui_surface_take_activation(&s,b.id));
    kit_ui_surface_begin(&s,1); add(&s,1,1,0,1); add(&s,1,UINT64_C(0x100000001),30,1);
    assert(kit_ui_surface_take_activation(&s,b.id));
    assert(kit_ui_surface_end(&s).code==CORE_OK);
    route(&s,KIT_UI_INTERACTION_POINTER_DOWN,5);
    kit_ui_surface_begin(&s,2); KitUiInteractionControl modal=add(&s,2,1,0,1);
    assert(kit_ui_surface_end(&s).code==CORE_OK);
    KitUiInteractionResult canceled=route(&s,KIT_UI_INTERACTION_POINTER_UP,5);
    assert(canceled.consumed && !canceled.activated_id && !s.interaction.focused_id);
    KitUiInteractionEvent tab={.type=KIT_UI_INTERACTION_KEY_DOWN,.key=KIT_UI_INTERACTION_KEY_TAB};
    KitUiInteractionResult r;
    assert(kit_ui_surface_route(&s,&tab,&r).code==CORE_OK && s.interaction.focused_id==modal.id);
    KitUiInteractionEvent key={.type=KIT_UI_INTERACTION_KEY_DOWN,.key=KIT_UI_INTERACTION_KEY_SPACE};
    assert(kit_ui_surface_route(&s,&key,&r).code==CORE_OK);
    kit_ui_surface_begin(&s,3); add(&s,3,1,0,1); assert(kit_ui_surface_end(&s).code==CORE_OK);
    key.type=KIT_UI_INTERACTION_KEY_UP;
    assert(kit_ui_surface_route(&s,&key,&r).code==CORE_OK && r.consumed && !r.activated_id);
    KitUiSurface before=s;
    kit_ui_surface_begin(&s,3);
    KitUiInteractionControl c;
    assert(kit_ui_surface_register(&s,(KitUiSurfaceKey){3,2},(KitRenderRect){NAN,0,20,20},NULL,1,&c).code!=CORE_OK);
    assert(kit_ui_surface_end(&s).code!=CORE_OK);
    assert(s.count==before.count && s.controls[0].id==before.controls[0].id);
    kit_ui_surface_begin(&s,3);
    KitRenderRect clip={5,0,10,20};
    assert(kit_ui_surface_register(&s,(KitUiSurfaceKey){3,1},(KitRenderRect){0,0,20,20},&clip,1,&c).code==CORE_OK);
    assert(c.bounds.x==5 && c.bounds.width==10);
    assert(kit_ui_surface_register(&s,(KitUiSurfaceKey){3,2},(KitRenderRect){30,0,20,20},&clip,1,&c).code==CORE_OK && !c.id);
    assert(kit_ui_surface_end(&s).code==CORE_OK && s.count==1);
    kit_ui_surface_begin(&s,3); add(&s,3,1,0,1);
    assert(kit_ui_surface_register(&s,(KitUiSurfaceKey){3,1},(KitRenderRect){30,0,20,20},NULL,1,&c).code!=CORE_OK);
    assert(kit_ui_surface_end(&s).code!=CORE_OK);
    kit_ui_surface_begin(&s,3);
    for (uint32_t i=0;i<KIT_UI_INTERACTION_CONTROL_MAX;++i) add(&s,3,i,(float)i*30,1);
    assert(kit_ui_surface_register(&s,(KitUiSurfaceKey){3,999},(KitRenderRect){0,30,20,20},NULL,1,&c).code!=CORE_OK);
    assert(kit_ui_surface_end(&s).code!=CORE_OK);
    puts("kit_ui surface: stable 64-bit keys, reorder, capture, FIFO, modal cancellation, clipping and transactional capacity pass");
    return 0;
}
