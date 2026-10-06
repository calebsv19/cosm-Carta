#ifndef KIT_WORKSPACE_AUTHORING_INTERACTION_H
#define KIT_WORKSPACE_AUTHORING_INTERACTION_H
#include "kit_workspace_authoring_ui.h"
#include "kit_ui_interaction.h"

#define KIT_WORKSPACE_AUTHORING_FONT_THEME_CONTROL_COUNT 13u

#ifdef __cplusplus
extern "C" {
#endif

/* Registration follows visual reading order. IDs reuse the existing semantic
 * button IDs and enabled policy. Product-specific additions/modal controls
 * belong to the host's own input scope. */
uint32_t kit_workspace_authoring_font_theme_controls(
    const KitWorkspaceAuthoringFontThemeLayout *layout,
    KitUiInteractionControl *out_controls, uint32_t capacity);
#ifdef __cplusplus
}
#endif
#endif
