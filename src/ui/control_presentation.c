#include "ui/control_surface.h"
#include "app/app_ui_internal.h"
#include "ui/font.h"
#include <math.h>

// Converts the host's resolved palette without changing theme persistence.
static CoreThemeColor color(SDL_Color c) { return (CoreThemeColor){c.r,c.g,c.b,c.a}; }
void carta_ui_control_draw(AppState *app, SDL_FRect r, const char *label,
                           bool selected, KitUiSurfaceKey key, float scale) {
    if (!app || !label || r.w<=0 || r.h<=0) return;
    KitUiSurface *s=&app->ui_state_bridge.controls;
    KitUiButtonSpec spec; kit_ui_button_spec_init(&spec,label);
    spec.state.selected=selected;
    if (key.domain) for (unsigned i=0;i<s->count;i++)
        if (s->keys[i].domain==key.domain && s->keys[i].value==key.value)
            spec.state=kit_ui_interaction_button_state(&s->interaction,&s->controls[i],selected);
    MapForgeThemePalette p=app_ui_theme_palette();
    KitUiButtonTheme theme={color(p.button_fill),color(p.button_active_primary),
        color(p.button_active_primary),color(p.button_active_success),
        color(p.button_outline),color(p.button_active_primary),
        color(p.text_primary),color(p.text_muted)};
    KitUiButtonStyle style; if (!kit_ui_button_style_resolve(&theme,&spec,&style)) return;
    // Preserve readable captions for both pale and dark selected host palettes.
    SDL_Color ink=(299u*style.fill.r+587u*style.fill.g+114u*style.fill.b)/1000u>160u ?
        (SDL_Color){20,28,38,255}:(SDL_Color){235,240,248,255};
    KitUiButtonAppearance appearance;
    kit_ui_button_appearance_preset(KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED,&appearance);
    float radius=kit_ui_corner_radius_clamp(appearance.corner_radius,r.w,r.h);
    renderer_set_draw_color(&app->renderer,style.outline.r,style.outline.g,style.outline.b,style.outline.a);
    renderer_fill_rounded_rect(&app->renderer,&r,radius);
    KitRenderRect inner=kit_ui_rect_inset((KitRenderRect){r.x,r.y,r.w,r.h},appearance.border_thickness);
    SDL_FRect inside={inner.x,inner.y,inner.width,inner.height};
    renderer_set_draw_color(&app->renderer,style.fill.r,style.fill.g,style.fill.b,style.fill.a);
    renderer_fill_rounded_rect(&app->renderer,&inside,
        kit_ui_corner_radius_for_inset(radius,appearance.border_thickness));
    bool chooser=key.domain==CARTA_CONTROL_HISTORY && key.value==14;
    int width=(int)fmaxf(1,r.w-2*appearance.padding_x-(chooser?14:0));
    int measured=ui_measure_text_width(label,scale), height=ui_font_line_height(scale);
    // Fit vertical font zoom within the existing hit rectangle, without resizing UI.
    if (height>r.h-4 && height>0) { scale*=fmaxf(4,r.h-4)/height; height=ui_font_line_height(scale); measured=ui_measure_text_width(label,scale); }
    KitUiButtonLayout layout;
    kit_ui_button_layout_init(&layout,fmaxf(appearance.padding_x,(r.w-measured)/2),fmaxf(2,(r.h-height)/2));
    int x,y;kit_ui_button_text_origin_i(&layout,(int)r.x,(int)r.y,&x,&y);
    // Drawing is synchronous: modal/search stack captions never enter a frame queue.
    ui_draw_text_clipped(&app->renderer,x,y,label,ink,scale,width);
    if (chooser) {
        renderer_set_draw_color(&app->renderer,ink.r,ink.g,ink.b,ink.a);
        for (int i=0;i<3;i++) renderer_fill_rect(&app->renderer,
            &(SDL_FRect){r.x+r.w-13+i,r.y+r.h/2-1+i,6-2*i,1});
    }
    if (spec.state.focused) {
        KitRenderRect marker;
        if (kit_ui_interaction_focus_marker(&s->interaction,s->controls,s->count,&marker)) {
            renderer_set_draw_color(&app->renderer,ink.r,ink.g,ink.b,ink.a);
            renderer_fill_rect(&app->renderer,&(SDL_FRect){marker.x,marker.y,marker.width,marker.height});
        }
    }
}
