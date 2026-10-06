#include "kit_workspace_authoring_interaction.h"

uint32_t kit_workspace_authoring_font_theme_controls(
    const KitWorkspaceAuthoringFontThemeLayout *layout,
    KitUiInteractionControl *out_controls, uint32_t capacity) {
    if (!layout || !out_controls || capacity < KIT_WORKSPACE_AUTHORING_FONT_THEME_CONTROL_COUNT) return 0u;
    const KitRenderRect rects[] = {
        layout->font_preset_buttons[0], layout->font_preset_buttons[1], layout->font_preset_buttons[2],
        layout->text_size_dec_button, layout->text_size_inc_button, layout->text_size_reset_button,
        layout->theme_preset_buttons[0], layout->theme_preset_buttons[1], layout->theme_preset_buttons[2],
        layout->theme_preset_buttons[3], layout->theme_preset_buttons[4],
        layout->custom_theme_buttons[0], layout->custom_theme_buttons[1]
    };
    const uint32_t ids[] = {4u, 5u, 6u, 1u, 2u, 3u, 7u, 8u, 9u, 10u, 11u, 12u, 13u};
    uint32_t count = 0u;
    for (uint32_t i = 0; i < KIT_WORKSPACE_AUTHORING_FONT_THEME_CONTROL_COUNT; ++i) {
        /* Omit off-panel/clipped controls so small viewports cannot acquire
         * keyboard focus on an invisible action. The host may further filter. */
        KitRenderRect r = rects[i], p = layout->panel;
        if (r.width <= 0 || r.height <= 0 || r.x < p.x || r.y < p.y ||
            r.x + r.width > p.x + p.width || r.y + r.height > p.y + p.height) continue;
        out_controls[count++] = (KitUiInteractionControl){ids[i], r,
            kit_workspace_authoring_ui_font_theme_button_enabled((KitWorkspaceAuthoringFontThemeButtonId)ids[i])};
    }
    return count;
}
