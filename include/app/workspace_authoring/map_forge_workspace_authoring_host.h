#ifndef MAP_FORGE_WORKSPACE_AUTHORING_HOST_H
#define MAP_FORGE_WORKSPACE_AUTHORING_HOST_H

#include <SDL2/SDL.h>
#include <stdint.h>

#include "core_base.h"
#include "kit_workspace_authoring.h"
#include "kit_workspace_authoring_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MapForgeWorkspaceAuthoringOverlayMode {
    MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES = 0,
    MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME = 1
} MapForgeWorkspaceAuthoringOverlayMode;

typedef struct MapForgeWorkspaceAuthoringHostState {
    uint8_t active;
    uint8_t key_c_down;
    uint8_t key_v_down;
    uint8_t entry_chord_armed_key;
    MapForgeWorkspaceAuthoringOverlayMode overlay_mode;
    uint32_t enter_count;
    uint32_t apply_count;
    uint32_t cancel_count;
    uint32_t overlay_cycle_count;
    uint32_t overlay_button_click_count;
    uint32_t font_theme_button_click_count;
    uint32_t font_theme_pending_changes;
    uint32_t last_font_theme_button_id;
    uint32_t font_theme_font_dirty;
    char font_theme_status[160];
    uint32_t add_stub_count;
    uint32_t consumed_event_count;
    uint32_t captured_runtime_event_count;
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint32_t last_pointer_x;
    uint32_t last_pointer_y;
    uint32_t last_pointer_ready;
    uint32_t last_overlay_button_id;
    uint32_t last_event_consumed;
    uint32_t last_event_entered;
    uint32_t last_event_exited;
    uint32_t last_event_accepted;
    uint32_t last_event_canceled;
    uint32_t baseline_valid;
    int baseline_font_zoom_step;
    char baseline_font_preset[64];
    char baseline_theme_preset[64];
} MapForgeWorkspaceAuthoringHostState;

void map_forge_workspace_authoring_host_reset(
    MapForgeWorkspaceAuthoringHostState *host);
int map_forge_workspace_authoring_host_active(
    const MapForgeWorkspaceAuthoringHostState *host);
int map_forge_workspace_authoring_host_surface_overlay_active(
    const MapForgeWorkspaceAuthoringHostState *host);
int map_forge_workspace_authoring_host_font_theme_overlay_active(
    const MapForgeWorkspaceAuthoringHostState *host);
CoreResult map_forge_workspace_authoring_host_enter(
    MapForgeWorkspaceAuthoringHostState *host);
CoreResult map_forge_workspace_authoring_host_apply(
    MapForgeWorkspaceAuthoringHostState *host);
CoreResult map_forge_workspace_authoring_host_cancel(
    MapForgeWorkspaceAuthoringHostState *host);
CoreResult map_forge_workspace_authoring_host_cycle_overlay(
    MapForgeWorkspaceAuthoringHostState *host);
void map_forge_workspace_authoring_host_set_viewport(
    MapForgeWorkspaceAuthoringHostState *host,
    uint32_t width,
    uint32_t height);
int map_forge_workspace_authoring_host_apply_overlay_button(
    MapForgeWorkspaceAuthoringHostState *host,
    KitWorkspaceAuthoringOverlayButtonId button_id);
int map_forge_workspace_authoring_host_apply_font_theme_button(
    MapForgeWorkspaceAuthoringHostState *host,
    KitWorkspaceAuthoringFontThemeButtonId button_id);
int map_forge_workspace_authoring_host_take_font_dirty(
    MapForgeWorkspaceAuthoringHostState *host);
int map_forge_workspace_authoring_host_last_event_accepted(
    const MapForgeWorkspaceAuthoringHostState *host);
int map_forge_workspace_authoring_host_handle_sdl_event(
    MapForgeWorkspaceAuthoringHostState *host,
    const SDL_Event *event,
    int text_entry_active);

#ifdef __cplusplus
}
#endif

#endif
