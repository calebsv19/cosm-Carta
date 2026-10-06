#include "kit_pane_host.h"
#include <math.h>
CoreResult kit_pane_header_layout(KitPaneHeaderLayout *out, CorePaneRect h,
    float pad,float gap,float title_min,const KitPaneHeaderAction *a,uint32_t n) {
    if (!out || n>KIT_PANE_HEADER_ACTION_MAX || (n && !a) ||
        !isfinite(h.x)||!isfinite(h.y)||!isfinite(h.width)||!isfinite(h.height)||
        !isfinite(pad)||!isfinite(gap)||!isfinite(title_min)||
        h.width<0||h.height<0||pad<0||gap<0||title_min<0)
        return (CoreResult){CORE_ERR_INVALID_ARG,"invalid pane header layout"};
    for(uint32_t i=0;i<n;i++) {
        if(!a[i].id||!isfinite(a[i].width)||a[i].width<=0)
            return (CoreResult){CORE_ERR_INVALID_ARG,"invalid pane action"};
        for(uint32_t j=0;j<i;j++) if(a[j].id==a[i].id)
            return (CoreResult){CORE_ERR_INVALID_ARG,"duplicate pane action"};
    }
    KitPaneHeaderLayout next={0};
    float w=fmaxf(0,h.width-2*pad),height=fmaxf(0,h.height-2*pad),used=0;
    for(uint32_t i=0;i<n;i++) {
        float candidate=used+a[i].width+gap;
        if(height<=0 || candidate+title_min>w) break;
        used=candidate;next.count++;
    }
    next.title=(CorePaneRect){h.x+fminf(pad,h.width/2),h.y+fminf(pad,h.height/2),w-used,height};
    float x=next.title.x+next.title.width+gap;
    for(uint32_t i=0;i<next.count;i++) {
        next.actions[i]=(KitPaneHeaderSlot){a[i].id,{x,next.title.y,a[i].width,height},a[i].enabled};
        x+=a[i].width+gap;
    }
    *out=next;return core_result_ok();
}
