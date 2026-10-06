#include "kit_ui_interaction.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static KitUiInteractionResult send(KitUiInteractionContext *ctx, KitUiInteractionControl *controls,
                                   uint32_t count, KitUiInteractionEventType type, float x, float y,
                                   KitUiInteractionKey key, uint32_t modifiers, int repeat) {
    KitUiInteractionResult result;
    KitUiInteractionEvent event = {type, x, y, key, modifiers, repeat};
    assert(kit_ui_interaction_route(ctx, controls, count, &event, &result).code == CORE_OK);
    return result;
}
#define POINTER(t,x,y) send(&ctx, c, 3u, t, x, y, KIT_UI_INTERACTION_KEY_NONE, 0u, 0)
#define KEY(t,k,m,r) send(&ctx, c, 3u, t, 0, 0, k, m, r)

int main(void) {
    KitUiInteractionControl c[] = {{11u,{10,10,40,24},1}, {22u,{60,10,40,24},0}, {33u,{110,10,40,24},1}};
    KitUiInteractionContext ctx = {0};
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_UP,20,20).activated_id);
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_DOWN,0,0).consumed);
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_UP,20,20).activated_id);
    assert(POINTER(KIT_UI_INTERACTION_POINTER_DOWN,20,20).consumed);
    assert(ctx.focused_id == 11u && ctx.captured_id == 11u);
    assert(POINTER(KIT_UI_INTERACTION_POINTER_MOVE,120,20).consumed);
    assert(!kit_ui_interaction_button_state(&ctx, &c[2],0).hovered);
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_UP,120,20).activated_id);
    assert(!ctx.captured_id);
    POINTER(KIT_UI_INTERACTION_POINTER_DOWN,20,20);
    POINTER(KIT_UI_INTERACTION_POINTER_MOVE,-20,20);
    assert(!kit_ui_interaction_button_state(&ctx,&c[0],0).pressed);
    POINTER(KIT_UI_INTERACTION_POINTER_MOVE,20,20);
    assert(kit_ui_interaction_button_state(&ctx,&c[0],0).pressed);
    assert(POINTER(KIT_UI_INTERACTION_POINTER_UP,20,20).activated_id == 11u);
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_UP,20,20).activated_id);
    POINTER(KIT_UI_INTERACTION_POINTER_DOWN,70,20);
    assert(!ctx.focused_id && !ctx.captured_id);
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_UP,70,20).activated_id);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_TAB,0,0);
    assert(ctx.focused_id == 11u);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_TAB,0,1);
    assert(ctx.focused_id == 11u);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_TAB,0,0);
    assert(ctx.focused_id == 33u);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_TAB,0,0);
    assert(ctx.focused_id == 11u);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_TAB,KIT_UI_INTERACTION_MOD_SHIFT,0);
    assert(ctx.focused_id == 33u);
    assert(!KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_ENTER,KIT_UI_INTERACTION_MOD_CTRL,0).consumed);
    assert(!KEY(KIT_UI_INTERACTION_KEY_UP,KIT_UI_INTERACTION_KEY_ENTER,0,0).activated_id);
    assert(KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_ENTER,0,0).consumed);
    assert(kit_ui_interaction_button_state(&ctx,&c[2],0).pressed);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_ENTER,0,1);
    assert(KEY(KIT_UI_INTERACTION_KEY_UP,KIT_UI_INTERACTION_KEY_ENTER,0,0).activated_id == 33u);
    assert(!KEY(KIT_UI_INTERACTION_KEY_UP,KIT_UI_INTERACTION_KEY_ENTER,0,0).activated_id);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_SPACE,0,0);
    assert(KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_ESCAPE,0,0).consumed);
    assert(!KEY(KIT_UI_INTERACTION_KEY_UP,KIT_UI_INTERACTION_KEY_SPACE,0,0).activated_id);
    KEY(KIT_UI_INTERACTION_KEY_DOWN,KIT_UI_INTERACTION_KEY_SPACE,0,0);
    c[2].enabled = 0;
    KitUiInteractionResult removed = KEY(KIT_UI_INTERACTION_KEY_UP,KIT_UI_INTERACTION_KEY_SPACE,0,0);
    assert(removed.consumed && !removed.activated_id && !ctx.focused_id);
    c[2].enabled = 1;
    POINTER(KIT_UI_INTERACTION_POINTER_DOWN,120,20);
    c[2].enabled = 0;
    assert(POINTER(KIT_UI_INTERACTION_POINTER_UP,120,20).consumed);
    c[2].enabled = 1;
    POINTER(KIT_UI_INTERACTION_POINTER_DOWN,20,20);
    KitUiInteractionContext before = ctx;
    KitUiInteractionResult result;
    KitUiInteractionEvent up = {KIT_UI_INTERACTION_POINTER_UP,20,20,0,0,0};
    c[1].id = 11u;
    assert(kit_ui_interaction_route(&ctx,c,3,&up,&result).code == CORE_ERR_INVALID_ARG);
    assert(!memcmp(&ctx,&before,sizeof(ctx)));
    c[1].id = 22u; c[0].bounds.width = NAN;
    assert(kit_ui_interaction_route(&ctx,c,3,&up,&result).code == CORE_ERR_INVALID_ARG);
    assert(!memcmp(&ctx,&before,sizeof(ctx)));
    c[0].bounds.width = 40;
    send(&ctx,c,3,KIT_UI_INTERACTION_CANCEL,0,0,0,0,0);
    assert(!ctx.focused_id && !ctx.pointer_owned);
    assert(!POINTER(KIT_UI_INTERACTION_POINTER_UP,20,20).activated_id);
    send(&ctx,NULL,0,KIT_UI_INTERACTION_KEY_DOWN,0,0,KIT_UI_INTERACTION_KEY_TAB,0,0);
    assert(!ctx.focused_id);
    /* A shared edge belongs only to the control beginning there. */
    c[1].bounds.x = 50; c[1].enabled = 1;
    POINTER(KIT_UI_INTERACTION_POINTER_DOWN,50,20);
    assert(POINTER(KIT_UI_INTERACTION_POINTER_UP,50,20).activated_id == 22u);
    KitUiInteractionControl tiny={99u,{10,10,1,1},1};
    ctx.focused_id=99u;
    KitRenderRect marker;
    assert(kit_ui_interaction_focus_marker(&ctx,&tiny,1,&marker));
    assert(marker.x>=10 && marker.y>=10 && marker.x+marker.width<=11 && marker.y+marker.height<=11);
    puts("kit_ui interaction: press origin, capture, disabled, focus, keyboard, cancel and validation pass");
    return 0;
}
