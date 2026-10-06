#include "kit_ui_focus_order.h"
#include <string.h>
static int same(KitUiSurfaceKey a,KitUiSurfaceKey b){return a.domain==b.domain&&a.value==b.value;}
static CoreResult validate(const KitUiFocusOrder *f,const KitUiFocusItem *items,uint32_t n) {
    if(!f||(!items&&n)||n>KIT_UI_INTERACTION_CONTROL_MAX)return (CoreResult){CORE_ERR_INVALID_ARG,"invalid focus order"};
    for(uint32_t i=0;i<n;++i){
        if(!items[i].key.domain||(items[i].kind!=KIT_UI_FOCUS_FIELD&&items[i].kind!=KIT_UI_FOCUS_BUTTON))
            return (CoreResult){CORE_ERR_INVALID_ARG,"invalid focus item"};
        for(uint32_t j=0;j<i;++j)if(same(items[i].key,items[j].key))
            return (CoreResult){CORE_ERR_INVALID_ARG,"duplicate focus key"};
    }
    return core_result_ok();
}
CoreResult kit_ui_focus_order_sync(KitUiFocusOrder *f,uint32_t scope,const KitUiFocusItem *items,uint32_t n) {
    CoreResult r=validate(f,items,n);if(r.code!=CORE_OK)return r;
    if(f->scope!=scope){memset(f,0,sizeof(*f));f->scope=scope;}
    if(f->active){
        for(uint32_t i=0;i<n;++i)if(items[i].enabled&&items[i].kind==f->kind&&same(items[i].key,f->key))return r;
        f->active=0;f->key=(KitUiSurfaceKey){0,0};
    }
    return r;
}
CoreResult kit_ui_focus_order_step(KitUiFocusOrder *f,uint32_t scope,const KitUiFocusItem *items,uint32_t n,int backwards) {
    CoreResult r=kit_ui_focus_order_sync(f,scope,items,n);if(r.code!=CORE_OK||!n)return r;
    int at=backwards?0:-1;
    if(f->active)for(uint32_t i=0;i<n;++i)if(same(items[i].key,f->key)){at=(int)i;break;}
    for(uint32_t i=0;i<n;++i){at=(at+(backwards?-1:1)+(int)n)%(int)n;
        if(items[at].enabled){f->key=items[at].key;f->kind=items[at].kind;f->active=1;return r;}}
    f->active=0;f->key=(KitUiSurfaceKey){0,0};return r;
}
