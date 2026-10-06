#include "kit_pane_composition.h"
#include <math.h>
#include <string.h>
static int valid(CorePaneRect r) {
    return isfinite(r.x)&&isfinite(r.y)&&isfinite(r.width)&&isfinite(r.height)&&
        r.width>=0&&r.height>=0&&isfinite(r.x+r.width)&&isfinite(r.y+r.height);
}
static CorePaneRect intersect(CorePaneRect a,CorePaneRect b) {
    float x=fmaxf(a.x,b.x),y=fmaxf(a.y,b.y);
    return (CorePaneRect){x,y,fmaxf(0,fminf(a.x+a.width,b.x+b.width)-x),
        fmaxf(0,fminf(a.y+a.height,b.y+b.height)-y)};
}
static int contains(CorePaneRect r,float x,float y) {
    return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;
}
static CorePaneRect inset(CorePaneRect r,float n) {
    float dx=fminf(n,r.width*.5f),dy=fminf(n,r.height*.5f);
    return (CorePaneRect){r.x+dx,r.y+dy,r.width-2*dx,r.height-2*dy};
}
CoreResult kit_pane_composition_build(KitPaneComposition *out,
    const KitPaneCompositionSpec *specs,uint32_t count,CorePaneRect viewport) {
    if(!out||count>KIT_PANE_COMPOSITION_MAX||(!specs&&count)||!valid(viewport))
        return (CoreResult){CORE_ERR_INVALID_ARG,"invalid pane composition"};
    KitPaneComposition next={0};next.count=count;next.viewport=viewport;
    for(uint32_t i=0;i<count;++i) {
        const KitPaneCompositionSpec *s=&specs[i];
        if(!s->id||!valid(s->bounds)||!isfinite(s->border)||!isfinite(s->header_height)||
           !isfinite(s->padding)||s->border<0||s->header_height<0||s->padding<0)
            return (CoreResult){CORE_ERR_INVALID_ARG,"invalid pane descriptor"};
        for(uint32_t j=0;j<i;++j)if(specs[j].id==s->id)
            return (CoreResult){CORE_ERR_INVALID_ARG,"duplicate pane identity"};
        KitPaneCompositionEntry *p=&next.entries[i];p->id=s->id;p->enabled=s->enabled;
        p->shell=s->bounds;p->header=inset(s->bounds,s->border);
        p->header.height=fminf(p->header.height,s->header_height);
        p->content=inset(s->bounds,s->border);p->content.y+=p->header.height;
        p->content.height-=p->header.height;p->content=inset(p->content,s->padding);
        p->visible_shell=intersect(p->shell,viewport);
        p->visible_header=intersect(p->header,viewport);
        p->visible_content=intersect(p->content,viewport);
    }
    *out=next;return core_result_ok();
}
const KitPaneCompositionEntry *kit_pane_composition_find(const KitPaneComposition *v,CorePaneId id) {
    if(v&&v->count<=KIT_PANE_COMPOSITION_MAX)for(uint32_t i=0;i<v->count;++i)
        if(v->entries[i].id==id)return &v->entries[i];
    return NULL;
}
KitPaneHit kit_pane_composition_hit(const KitPaneComposition *v,float x,float y) {
    if(v&&v->count<=KIT_PANE_COMPOSITION_MAX&&isfinite(x)&&isfinite(y))for(uint32_t i=v->count;i>0;--i) {
        const KitPaneCompositionEntry *p=&v->entries[i-1];
        if(contains(p->visible_shell,x,y)) {
            if(!p->enabled)return (KitPaneHit){0,KIT_PANE_REGION_NONE};
            KitPaneRegion region=contains(p->visible_content,x,y)?KIT_PANE_REGION_CONTENT:
                contains(p->visible_header,x,y)?KIT_PANE_REGION_HEADER:KIT_PANE_REGION_SHELL;
            return (KitPaneHit){p->id,region};
        }
    }
    return (KitPaneHit){0,KIT_PANE_REGION_NONE};
}
CorePaneId kit_pane_pointer_route(KitPanePointerOwner *o,const KitPaneComposition *v,
    float x,float y,int pressed,int released,int blocked) {
    if(!o)return 0;
    const KitPaneCompositionEntry *p=kit_pane_composition_find(v,o->captured_id);
    if(blocked||!v||!isfinite(x)||!isfinite(y)||
       (o->down&&(!p||!p->enabled||p->visible_shell.width<=0||p->visible_shell.height<=0))) {
        memset(o,0,sizeof(*o));return 0;
    }
    if(pressed&&!o->down){o->down=1;o->captured_id=kit_pane_composition_hit(v,x,y).id;}
    CorePaneId id=o->down?o->captured_id:kit_pane_composition_hit(v,x,y).id;
    if(released){id=o->down?o->captured_id:0;memset(o,0,sizeof(*o));}
    return id;
}
