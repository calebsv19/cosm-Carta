#include "ui/rollout.h"
#include "app/app_internal.h"
#include "app/app_utility_workspace.h"
#include <math.h>
#include <string.h>
// Matches existing pane identities; normal map projection remains app-owned.
static CorePaneRect bounds(const AppState *app) {
    return (CorePaneRect){0,APP_HEADER_HEIGHT,app->width,fmaxf(0,app->height-APP_HEADER_HEIGHT)};
}
void carta_panes_layout(AppState *app) {
    CartaUiRollout *r=&app->ui_state_bridge.rollout;
    if(!r->pane_initialized) {
        if(!isfinite(r->sidebar_ratio) || r->sidebar_ratio<=0)r->sidebar_ratio=.28f;
        kit_pane_splitter_interaction_init(&r->splitter,14);
        r->pane_initialized=true;
    }
    memset(r->nodes,0,sizeof(r->nodes));
    r->nodes[0]=(CorePaneNode){.type=CORE_PANE_NODE_SPLIT,.id=1,.axis=CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01=r->sidebar_ratio,.child_a=1,.child_b=2,.constraints={280,480}};
    r->nodes[1]=(CorePaneNode){.type=CORE_PANE_NODE_LEAF,.id=2};
    r->nodes[2]=(CorePaneNode){.type=CORE_PANE_NODE_LEAF,.id=3};
    CorePaneRect b=bounds(app);CorePaneLeafRect leaves[2];uint32_t count=0;
    KitPaneCompositionSpec specs[2]={{.id=3,.bounds=b,.enabled=1}};uint32_t n=1;
    if(app->ui_state_bridge.left_pane_open && b.width>1 && b.height>1 &&
       core_pane_solve(r->nodes,3,0,b,leaves,2,&count) && count==2) {
        CorePaneRect left=leaves[0].rect;
        app->ui_state_bridge.left_pane_rect=(SDL_FRect){left.x,left.y,left.width,left.height};
        if(app->ui_state_bridge.utility_workspace==APP_UTILITY_HIKING_HISTORY)specs[0].bounds=leaves[1].rect;
        specs[n++]=(KitPaneCompositionSpec){.id=2,.bounds=left,.header_height=42,.padding=10,.enabled=1};
    } else kit_pane_splitter_interaction_end_drag(&r->splitter);
    (void)kit_pane_composition_build(&r->panes,specs,n,(CorePaneRect){0,0,app->width,app->height});
    (void)kit_pane_pointer_route(&r->pointer,&r->panes,0,0,0,0,0);
}
