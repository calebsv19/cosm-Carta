#include "kit_ui_surface.h"

#include <math.h>
#include <string.h>

static int same_key(KitUiSurfaceKey a, KitUiSurfaceKey b) {
    return a.domain == b.domain && a.value == b.value;
}
static int valid_rect(KitRenderRect r) {
    return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height) &&
        r.width > 0 && r.height > 0 && isfinite(r.x+r.width) && isfinite(r.y+r.height);
}
static int enabled_id(const KitUiInteractionControl *controls, uint32_t count, uint32_t id) {
    for (uint32_t i=0; i<count; ++i) if (controls[i].id==id && controls[i].enabled) return 1;
    return 0;
}
void kit_ui_surface_reset(KitUiSurface *s) { if (s) memset(s,0,sizeof(*s)); }
void kit_ui_surface_set_scope(KitUiSurface *s, uint32_t scope) {
    if (!s || s->scope==scope) return;
    s->scope=scope; s->count=0; s->next_count=0; s->activation_count=0;
    /* Cancel the old owner, but swallow its outstanding pointer/key release.
     * A modal transition must not release into a new background control. */
    KitUiInteractionEvent sync={.type=KIT_UI_INTERACTION_SYNC};
    KitUiInteractionResult result;
    (void)kit_ui_interaction_route(&s->interaction,NULL,0,&sync,&result);
}
void kit_ui_surface_begin(KitUiSurface *s, uint32_t scope) {
    if (!s) return;
    kit_ui_surface_set_scope(s,scope);
    s->next_count=0; s->collecting=1; s->claimed=0;
    s->collection_result=core_result_ok();
}
CoreResult kit_ui_surface_register(KitUiSurface *s, KitUiSurfaceKey key,
    KitRenderRect bounds, const KitRenderRect *clip, int enabled,
    KitUiInteractionControl *out) {
    KitUiInteractionControl local;
    if (!out) out=&local;
    memset(out,0,sizeof(*out));
    if (!s || !s->collecting)
        return (CoreResult){CORE_ERR_INVALID_ARG,"surface registration outside collection"};
    if (s->collection_result.code!=CORE_OK) return s->collection_result;
    if (!key.domain || !valid_rect(bounds) || (clip && !valid_rect(*clip))) {
        s->collection_result=(CoreResult){CORE_ERR_INVALID_ARG,"invalid surface key or geometry"};
        return s->collection_result;
    }
    if (clip) {
        float right=fminf(bounds.x+bounds.width,clip->x+clip->width);
        float bottom=fminf(bounds.y+bounds.height,clip->y+clip->height);
        bounds.x=fmaxf(bounds.x,clip->x); bounds.y=fmaxf(bounds.y,clip->y);
        bounds.width=right-bounds.x; bounds.height=bottom-bounds.y;
        if (bounds.width<=0 || bounds.height<=0) return core_result_ok();
    }
    for (uint32_t i=0; i<s->next_count; ++i) if (same_key(s->next_keys[i],key)) {
        s->collection_result=(CoreResult){CORE_ERR_INVALID_ARG,"duplicate surface semantic key"};
        return s->collection_result;
    }
    if (s->next_count==KIT_UI_INTERACTION_CONTROL_MAX) {
        s->collection_result=(CoreResult){CORE_ERR_INVALID_ARG,"surface control capacity exceeded"};
        return s->collection_result;
    }
    uint32_t id=0;
    for (uint32_t i=0; i<s->count; ++i) if (same_key(s->keys[i],key)) { id=s->controls[i].id; break; }
    if (!id) {
        /* A fresh handle cannot alias a still outstanding owner or activation. */
        do {
            id=++s->next_id;
            if (!id) continue;
            int used=id==s->interaction.captured_id || id==s->interaction.focused_id || id==s->interaction.key_owner_id;
            for (uint32_t i=0; i<s->count; ++i) used |= s->controls[i].id==id;
            for (uint32_t i=0; i<s->next_count; ++i) used |= s->next_controls[i].id==id;
            for (uint32_t i=0; i<s->activation_count; ++i) used |= s->activations[i]==id;
            if (!used) break;
        } while (1);
    }
    *out=(KitUiInteractionControl){id,bounds,enabled!=0};
    s->next_controls[s->next_count]=*out; s->next_keys[s->next_count++]=key;
    return core_result_ok();
}
CoreResult kit_ui_surface_end(KitUiSurface *s) {
    if (!s || !s->collecting) return (CoreResult){CORE_ERR_INVALID_ARG,"surface end outside collection"};
    s->collecting=0;
    if (s->collection_result.code!=CORE_OK) return s->collection_result;
    KitUiInteractionContext candidate=s->interaction;
    KitUiInteractionEvent sync={.type=KIT_UI_INTERACTION_SYNC};
    KitUiInteractionResult result;
    CoreResult valid=kit_ui_interaction_route(&candidate,s->next_controls,s->next_count,&sync,&result);
    if (valid.code!=CORE_OK) return valid;
    s->interaction=candidate; s->count=s->next_count;
    memcpy(s->controls,s->next_controls,s->count*sizeof(*s->controls));
    memcpy(s->keys,s->next_keys,s->count*sizeof(*s->keys));
    uint32_t remaining=0;
    for (uint32_t i=0; i<s->activation_count; ++i)
        if (enabled_id(s->controls,s->count,s->activations[i])) s->activations[remaining++]=s->activations[i];
    s->activation_count=remaining;
    return core_result_ok();
}
CoreResult kit_ui_surface_route(KitUiSurface *s, const KitUiInteractionEvent *event,
    KitUiInteractionResult *out) {
    if (!s || !out || s->collecting) return (CoreResult){CORE_ERR_INVALID_ARG,"invalid surface route phase"};
    KitUiInteractionContext candidate=s->interaction;
    CoreResult result=kit_ui_interaction_route(&candidate,s->controls,s->count,event,out);
    if (result.code!=CORE_OK) return result;
    if (out->activated_id && s->activation_count==KIT_UI_SURFACE_ACTIVATION_MAX) {
        memset(out,0,sizeof(*out));
        return (CoreResult){CORE_ERR_INVALID_ARG,"surface activation capacity exceeded"};
    }
    s->interaction=candidate;
    if (event->type==KIT_UI_INTERACTION_CANCEL) s->activation_count=0;
    if (out->activated_id) s->activations[s->activation_count++]=out->activated_id;
    return core_result_ok();
}
int kit_ui_surface_take_activation(KitUiSurface *s, uint32_t id) {
    if (!s || s->claimed || !id || !s->activation_count || s->activations[0]!=id) return 0;
    const KitUiInteractionControl *controls=s->collecting?s->next_controls:s->controls;
    uint32_t count=s->collecting?s->next_count:s->count;
    if (!enabled_id(controls,count,id)) return 0;
    --s->activation_count;
    memmove(s->activations,s->activations+1,s->activation_count*sizeof(*s->activations));
    s->claimed=1;
    return 1;
}
int kit_ui_surface_key(const KitUiSurface *s, uint32_t id, KitUiSurfaceKey *out) {
    if (!s || !out) return 0;
    for (uint32_t i=0; i<s->count; ++i) if (s->controls[i].id==id) { *out=s->keys[i]; return 1; }
    return 0;
}
int kit_ui_surface_pending(const KitUiSurface *s) { return s && s->activation_count!=0; }
