#include "app/workspace_authoring/map_forge_workspace_authoring_host.h"

#include <string.h>

#include "kit_workspace_authoring_ui.h"
#include "ui/shared_theme_font_adapter.h"

static CoreResult map_forge_workspace_authoring_invalid(const char *message) {
    CoreResult result = { CORE_ERR_INVALID_ARG, message };
    return result;
}

static uint32_t map_forge_workspace_authoring_mod_bits(SDL_Keymod mods) {
    uint32_t bits = 0u;
    if ((mods & KMOD_SHIFT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    if ((mods & KMOD_ALT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    if ((mods & KMOD_CTRL) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    if ((mods & KMOD_GUI) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    return bits;
}

static KitWorkspaceAuthoringKey map_forge_workspace_authoring_key_from_sdl_keysym(
    const SDL_Keysym *keysym) {
    if (!keysym) return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    switch (keysym->scancode) {
        case SDL_SCANCODE_C:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDL_SCANCODE_V:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        default:
            break;
    }
    switch (keysym->sym) {
        case SDLK_TAB:
            return KIT_WORKSPACE_AUTHORING_KEY_TAB;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return KIT_WORKSPACE_AUTHORING_KEY_ENTER;
        case SDLK_ESCAPE:
            return KIT_WORKSPACE_AUTHORING_KEY_ESCAPE;
        case SDLK_c:
            return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_v:
            return KIT_WORKSPACE_AUTHORING_KEY_V;
        default:
            return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static void map_forge_workspace_authoring_note_consumed(
    MapForgeWorkspaceAuthoringHostState *host,
    int runtime_event) {
    if (!host) return;
    host->last_event_consumed = 1u;
    host->consumed_event_count += 1u;
    if (runtime_event) {
        host->captured_runtime_event_count += 1u;
    }
}

static void map_forge_workspace_authoring_host_set_status(
    MapForgeWorkspaceAuthoringHostState *host,
    const char *status) {
    if (!host) return;
    if (!status) status = "";
    strncpy(host->font_theme_status, status, sizeof(host->font_theme_status) - 1u);
    host->font_theme_status[sizeof(host->font_theme_status) - 1u] = '\0';
}

static void map_forge_workspace_authoring_host_capture_baseline(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host) return;
    host->baseline_font_zoom_step = mapforge_shared_font_zoom_step();
    if (!mapforge_shared_font_current_preset(host->baseline_font_preset,
                                             sizeof(host->baseline_font_preset))) {
        strncpy(host->baseline_font_preset, "ide", sizeof(host->baseline_font_preset) - 1u);
        host->baseline_font_preset[sizeof(host->baseline_font_preset) - 1u] = '\0';
    }
    if (!mapforge_shared_theme_current_preset(host->baseline_theme_preset,
                                              sizeof(host->baseline_theme_preset))) {
        strncpy(host->baseline_theme_preset, "midnight_contrast", sizeof(host->baseline_theme_preset) - 1u);
        host->baseline_theme_preset[sizeof(host->baseline_theme_preset) - 1u] = '\0';
    }
    host->baseline_valid = 1u;
    host->font_theme_pending_changes = 0u;
    host->font_theme_font_dirty = 0u;
    host->font_theme_status[0] = '\0';
}

static void map_forge_workspace_authoring_host_restore_baseline(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host || !host->baseline_valid) return;
    if (host->baseline_theme_preset[0]) {
        (void)mapforge_shared_theme_set_preset(host->baseline_theme_preset);
    }
    if (host->baseline_font_preset[0]) {
        (void)mapforge_shared_font_set_preset(host->baseline_font_preset);
    }
    (void)mapforge_shared_font_set_zoom_step(host->baseline_font_zoom_step);
    host->font_theme_font_dirty = 1u;
    host->font_theme_pending_changes = 0u;
    map_forge_workspace_authoring_host_set_status(host, "Draft restored.");
}

void map_forge_workspace_authoring_host_reset(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host) return;
    memset(host, 0, sizeof(*host));
    host->overlay_mode = MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES;
}

int map_forge_workspace_authoring_host_active(
    const MapForgeWorkspaceAuthoringHostState *host) {
    return host && host->active ? 1 : 0;
}

int map_forge_workspace_authoring_host_surface_overlay_active(
    const MapForgeWorkspaceAuthoringHostState *host) {
    if (!map_forge_workspace_authoring_host_active(host)) return 0;
    return host->overlay_mode == MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES ? 1 : 0;
}

int map_forge_workspace_authoring_host_font_theme_overlay_active(
    const MapForgeWorkspaceAuthoringHostState *host) {
    if (!map_forge_workspace_authoring_host_active(host)) return 0;
    return host->overlay_mode == MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME ? 1 : 0;
}

CoreResult map_forge_workspace_authoring_host_enter(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host) return map_forge_workspace_authoring_invalid("null authoring host");
    if (!map_forge_workspace_authoring_host_active(host)) {
        host->active = 1u;
        host->overlay_mode = MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES;
        host->enter_count += 1u;
        map_forge_workspace_authoring_host_capture_baseline(host);
    }
    host->last_event_entered = 1u;
    return core_result_ok();
}

CoreResult map_forge_workspace_authoring_host_apply(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host) return map_forge_workspace_authoring_invalid("null authoring host");
    if (map_forge_workspace_authoring_host_active(host)) {
        host->active = 0u;
        host->apply_count += 1u;
        host->last_event_accepted = 1u;
    }
    host->baseline_valid = 0u;
    host->font_theme_pending_changes = 0u;
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES;
    host->last_event_exited = 1u;
    return core_result_ok();
}

CoreResult map_forge_workspace_authoring_host_cancel(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host) return map_forge_workspace_authoring_invalid("null authoring host");
    if (map_forge_workspace_authoring_host_active(host)) {
        map_forge_workspace_authoring_host_restore_baseline(host);
        host->active = 0u;
        host->cancel_count += 1u;
        host->last_event_canceled = 1u;
    }
    host->baseline_valid = 0u;
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES;
    host->last_event_exited = 1u;
    return core_result_ok();
}

CoreResult map_forge_workspace_authoring_host_cycle_overlay(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host) return map_forge_workspace_authoring_invalid("null authoring host");
    if (!map_forge_workspace_authoring_host_active(host)) {
        return core_result_ok();
    }
    host->overlay_mode =
        host->overlay_mode == MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES
            ? MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
            : MAP_FORGE_WORKSPACE_AUTHORING_OVERLAY_SURFACES;
    host->overlay_cycle_count += 1u;
    return core_result_ok();
}

void map_forge_workspace_authoring_host_set_viewport(
    MapForgeWorkspaceAuthoringHostState *host,
    uint32_t width,
    uint32_t height) {
    if (!host) return;
    host->viewport_width = width;
    host->viewport_height = height;
}

int map_forge_workspace_authoring_host_apply_overlay_button(
    MapForgeWorkspaceAuthoringHostState *host,
    KitWorkspaceAuthoringOverlayButtonId button_id) {
    if (!host || !map_forge_workspace_authoring_host_active(host)) return 0;
    host->last_overlay_button_id = (uint32_t)button_id;
    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE:
            (void)map_forge_workspace_authoring_host_cycle_overlay(host);
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY:
            (void)map_forge_workspace_authoring_host_apply(host);
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL:
            (void)map_forge_workspace_authoring_host_cancel(host);
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD:
            host->add_stub_count += 1u;
            host->overlay_button_click_count += 1u;
            return 1;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE:
        default:
            break;
    }
    return 0;
}

int map_forge_workspace_authoring_host_apply_font_theme_button(
    MapForgeWorkspaceAuthoringHostState *host,
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    KitWorkspaceAuthoringFontThemeAction action;
    const char *preset_name = NULL;
    if (!host || !map_forge_workspace_authoring_host_font_theme_overlay_active(host)) return 0;
    if (!kit_workspace_authoring_ui_font_theme_button_enabled(button_id)) return 0;

    action = kit_workspace_authoring_ui_font_theme_action_for_button(button_id);
    host->last_font_theme_button_id = (uint32_t)button_id;

    switch (action.type) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC:
            (void)mapforge_shared_font_step_by(-1);
            host->font_theme_font_dirty = 1u;
            map_forge_workspace_authoring_host_set_status(host, "Text size decreased.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC:
            (void)mapforge_shared_font_step_by(1);
            host->font_theme_font_dirty = 1u;
            map_forge_workspace_authoring_host_set_status(host, "Text size increased.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET:
            (void)mapforge_shared_font_reset_zoom_step();
            host->font_theme_font_dirty = 1u;
            map_forge_workspace_authoring_host_set_status(host, "Text size reset.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET:
            preset_name = core_font_preset_name(action.font_preset_id);
            if (!mapforge_shared_font_set_preset(preset_name)) return 0;
            host->font_theme_font_dirty = 1u;
            map_forge_workspace_authoring_host_set_status(host, "Font preset previewed.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET:
            preset_name = core_theme_preset_name(action.theme_preset_id);
            if (!mapforge_shared_theme_set_preset(preset_name)) return 0;
            map_forge_workspace_authoring_host_set_status(host, "Theme preset previewed.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS:
            map_forge_workspace_authoring_host_set_status(host, action.custom_status_text);
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE:
        default:
            return 0;
    }

    host->font_theme_button_click_count += 1u;
    host->font_theme_pending_changes += 1u;
    return 1;
}

int map_forge_workspace_authoring_host_take_font_dirty(
    MapForgeWorkspaceAuthoringHostState *host) {
    if (!host || !host->font_theme_font_dirty) return 0;
    host->font_theme_font_dirty = 0u;
    return 1;
}

int map_forge_workspace_authoring_host_last_event_accepted(
    const MapForgeWorkspaceAuthoringHostState *host) {
    return host && host->last_event_accepted ? 1 : 0;
}

static int map_forge_workspace_authoring_host_handle_overlay_click(
    MapForgeWorkspaceAuthoringHostState *host,
    int x,
    int y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    KitWorkspaceAuthoringOverlayButtonId hit = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
    uint32_t count = 0u;
    if (!host || !map_forge_workspace_authoring_host_active(host)) return 0;
    if (host->viewport_width == 0u) return 0;

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        (int)host->viewport_width,
        1,
        map_forge_workspace_authoring_host_surface_overlay_active(host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    hit = kit_workspace_authoring_ui_overlay_hit_test(buttons, count, (float)x, (float)y);
    return map_forge_workspace_authoring_host_apply_overlay_button(host, hit);
}

static int map_forge_workspace_authoring_host_handle_font_theme_click(
    MapForgeWorkspaceAuthoringHostState *host,
    int x,
    int y) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    KitWorkspaceAuthoringFontThemeButtonId hit;
    if (!host || !map_forge_workspace_authoring_host_font_theme_overlay_active(host)) return 0;
    if (host->viewport_width == 0u || host->viewport_height == 0u) return 0;
    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL,
                                                            (int)host->viewport_width,
                                                            (int)host->viewport_height,
                                                            &layout)) {
        return 0;
    }
    hit = kit_workspace_authoring_ui_font_theme_hit_button(&layout, (float)x, (float)y);
    return map_forge_workspace_authoring_host_apply_font_theme_button(host, hit);
}

int map_forge_workspace_authoring_host_handle_sdl_event(
    MapForgeWorkspaceAuthoringHostState *host,
    const SDL_Event *event,
    int text_entry_active) {
    KitWorkspaceAuthoringKey key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    uint32_t mod_bits = 0u;

    if (!host || !event) {
        return 0;
    }
    host->last_event_consumed = 0u;
    host->last_event_entered = 0u;
    host->last_event_exited = 0u;
    host->last_event_accepted = 0u;
    host->last_event_canceled = 0u;

    if (event->type == SDL_KEYUP) {
        key = map_forge_workspace_authoring_key_from_sdl_keysym(&event->key.keysym);
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            host->key_c_down = 0u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            host->key_v_down = 0u;
        }
        if (host->entry_chord_armed_key == (uint8_t)key) {
            host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
        }
        return 0;
    }

    if (event->type == SDL_MOUSEMOTION &&
        map_forge_workspace_authoring_host_active(host)) {
        host->last_pointer_x = event->motion.x > 0 ? (uint32_t)event->motion.x : 0u;
        host->last_pointer_y = event->motion.y > 0 ? (uint32_t)event->motion.y : 0u;
        host->last_pointer_ready = 1u;
        map_forge_workspace_authoring_note_consumed(host, 1);
        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        map_forge_workspace_authoring_host_active(host)) {
        int overlay_hit = 0;
        host->last_pointer_x = event->button.x > 0 ? (uint32_t)event->button.x : 0u;
        host->last_pointer_y = event->button.y > 0 ? (uint32_t)event->button.y : 0u;
        host->last_pointer_ready = 1u;
        if (map_forge_workspace_authoring_host_font_theme_overlay_active(host)) {
            overlay_hit = map_forge_workspace_authoring_host_handle_font_theme_click(
                host,
                event->button.x,
                event->button.y);
        } else {
            overlay_hit = map_forge_workspace_authoring_host_handle_overlay_click(
                host,
                event->button.x,
                event->button.y);
        }
        map_forge_workspace_authoring_note_consumed(host, overlay_hit ? 0 : 1);
        return 1;
    }

    if (event->type != SDL_KEYDOWN) {
        if (map_forge_workspace_authoring_host_active(host)) {
            map_forge_workspace_authoring_note_consumed(host, 1);
            return 1;
        }
        return 0;
    }

    key = map_forge_workspace_authoring_key_from_sdl_keysym(&event->key.keysym);
    mod_bits = map_forge_workspace_authoring_mod_bits((SDL_Keymod)event->key.keysym.mod);

    if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
        host->key_c_down = 1u;
    } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
        host->key_v_down = 1u;
    }

    if (!text_entry_active &&
        (mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u &&
        (mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                     KIT_WORKSPACE_AUTHORING_MOD_CTRL |
                     KIT_WORKSPACE_AUTHORING_MOD_GUI)) == 0u &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C ||
         key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
        if (!kit_workspace_authoring_entry_chord_pressed(key,
                                                         mod_bits,
                                                         host->key_c_down,
                                                         host->key_v_down)) {
            map_forge_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        if (host->entry_chord_armed_key == (uint8_t)key) {
            map_forge_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        host->entry_chord_armed_key = (uint8_t)key;
        if (map_forge_workspace_authoring_host_active(host)) {
            (void)map_forge_workspace_authoring_host_cancel(host);
        } else {
            (void)map_forge_workspace_authoring_host_enter(host);
        }
        map_forge_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    if (!map_forge_workspace_authoring_host_active(host)) {
        return 0;
    }

    switch (key) {
        case KIT_WORKSPACE_AUTHORING_KEY_TAB:
            (void)map_forge_workspace_authoring_host_cycle_overlay(host);
            map_forge_workspace_authoring_note_consumed(host, 1);
            return 1;
        case KIT_WORKSPACE_AUTHORING_KEY_ENTER:
            (void)map_forge_workspace_authoring_host_apply(host);
            map_forge_workspace_authoring_note_consumed(host, 1);
            return 1;
        case KIT_WORKSPACE_AUTHORING_KEY_ESCAPE:
            (void)map_forge_workspace_authoring_host_cancel(host);
            map_forge_workspace_authoring_note_consumed(host, 1);
            return 1;
        default:
            map_forge_workspace_authoring_note_consumed(host, 1);
            return 1;
    }
}
