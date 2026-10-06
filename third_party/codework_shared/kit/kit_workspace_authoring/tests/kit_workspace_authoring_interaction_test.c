#include "kit_workspace_authoring_interaction.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    KitUiInteractionControl controls[13];
    assert(kit_workspace_authoring_ui_font_theme_build_layout(NULL,1440,1000,&layout));
    uint32_t count = kit_workspace_authoring_font_theme_controls(&layout,controls,13);
    assert(count == 13u && controls[0].id == 4u && controls[3].id == 1u);
    assert(!controls[2].enabled);
    KitUiInteractionContext ctx = {0};
    KitUiInteractionResult result;
    KitUiInteractionEvent tab = {.type=KIT_UI_INTERACTION_KEY_DOWN,.key=KIT_UI_INTERACTION_KEY_TAB};
    assert(kit_ui_interaction_route(&ctx,controls,count,&tab,&result).code==CORE_OK && ctx.focused_id==4u);
    assert(kit_ui_interaction_route(&ctx,controls,count,&tab,&result).code==CORE_OK && ctx.focused_id==5u);
    assert(kit_ui_interaction_route(&ctx,controls,count,&tab,&result).code==CORE_OK && ctx.focused_id==1u);
    assert(!kit_workspace_authoring_font_theme_controls(&layout,controls,12));
    layout.panel.height=140;
    count=kit_workspace_authoring_font_theme_controls(&layout,controls,13);
    assert(count < 13u);
    for(uint32_t i=0;i<count;++i) assert(controls[i].bounds.y+controls[i].bounds.height <= layout.panel.y+layout.panel.height);
    puts("workspace authoring interaction: ordered IDs, disabled skip and clipped control exclusion pass");
    return 0;
}
