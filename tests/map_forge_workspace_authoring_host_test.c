#include "app/workspace_authoring/map_forge_workspace_authoring_host.h"
#include "ui/shared_theme_font_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static SDL_Event key_event(Uint32 type, SDL_Scancode scancode, SDL_Keycode key, SDL_Keymod mod) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = key;
    event.key.keysym.mod = mod;
    return event;
}

static SDL_Event mouse_event(Uint32 type, int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    if (type == SDL_MOUSEMOTION) {
        event.motion.type = type;
        event.motion.x = x;
        event.motion.y = y;
    } else {
        event.button.type = type;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = x;
        event.button.y = y;
    }
    return event;
}

static void test_entry_chord_and_exit(void) {
    MapForgeWorkspaceAuthoringHostState host;
    SDL_Event plain_c = key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_NONE);
    SDL_Event alt_c = key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    SDL_Event alt_v = key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    SDL_Event enter = key_event(SDL_KEYDOWN, SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_NONE);

    map_forge_workspace_authoring_host_reset(&host);
    assert(!map_forge_workspace_authoring_host_handle_sdl_event(&host, &plain_c, 0));
    assert(!map_forge_workspace_authoring_host_active(&host));

    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 0));
    assert(!map_forge_workspace_authoring_host_active(&host));
    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 0));
    assert(map_forge_workspace_authoring_host_active(&host));
    assert(map_forge_workspace_authoring_host_surface_overlay_active(&host));
    assert(host.enter_count == 1u);

    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &enter, 0));
    assert(!map_forge_workspace_authoring_host_active(&host));
    assert(host.apply_count == 1u);
}

static void test_reserved_capture_and_overlay_cycle(void) {
    MapForgeWorkspaceAuthoringHostState host;
    SDL_Event alt_c = key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    SDL_Event alt_v = key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    SDL_Event tab = key_event(SDL_KEYDOWN, SDL_SCANCODE_TAB, SDLK_TAB, KMOD_NONE);
    SDL_Event click = mouse_event(SDL_MOUSEBUTTONDOWN, 400, 320);
    SDL_Event esc = key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, KMOD_NONE);

    map_forge_workspace_authoring_host_reset(&host);
    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 0));
    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 0));
    assert(map_forge_workspace_authoring_host_active(&host));

    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &tab, 0));
    assert(map_forge_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(host.overlay_cycle_count == 1u);

    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(host.captured_runtime_event_count >= 2u);

    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &esc, 0));
    assert(!map_forge_workspace_authoring_host_active(&host));
    assert(host.cancel_count == 1u);
}

static void test_overlay_button_hit_testing(void) {
    MapForgeWorkspaceAuthoringHostState host;
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count;
    int add_x = 0;
    int add_y = 0;
    int mode_x = 0;
    int mode_y = 0;
    SDL_Event add_click;
    SDL_Event mode_click;

    map_forge_workspace_authoring_host_reset(&host);
    map_forge_workspace_authoring_host_set_viewport(&host, 1280u, 720u);
    assert(map_forge_workspace_authoring_host_enter(&host).code == CORE_OK);

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        1280,
        1,
        map_forge_workspace_authoring_host_surface_overlay_active(&host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (uint32_t i = 0u; i < count; ++i) {
        if (buttons[i].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD) {
            add_x = (int)(buttons[i].rect.x + buttons[i].rect.width * 0.5f);
            add_y = (int)(buttons[i].rect.y + buttons[i].rect.height * 0.5f);
        } else if (buttons[i].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE) {
            mode_x = (int)(buttons[i].rect.x + buttons[i].rect.width * 0.5f);
            mode_y = (int)(buttons[i].rect.y + buttons[i].rect.height * 0.5f);
        }
    }
    assert(add_x > 0 && add_y > 0);
    assert(mode_x > 0 && mode_y > 0);

    add_click = mouse_event(SDL_MOUSEBUTTONDOWN, add_x, add_y);
    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &add_click, 0));
    assert(host.add_stub_count == 1u);
    assert(host.overlay_button_click_count == 1u);
    assert(host.last_overlay_button_id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD);
    assert(map_forge_workspace_authoring_host_surface_overlay_active(&host));

    mode_click = mouse_event(SDL_MOUSEBUTTONDOWN, mode_x, mode_y);
    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &mode_click, 0));
    assert(host.overlay_button_click_count == 2u);
    assert(host.last_overlay_button_id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE);
    assert(map_forge_workspace_authoring_host_font_theme_overlay_active(&host));
}

static void test_font_theme_button_hit_testing(void) {
    MapForgeWorkspaceAuthoringHostState host;
    KitWorkspaceAuthoringFontThemeLayout layout;
    SDL_Event click;
    int inc_x = 0;
    int inc_y = 0;
    int base_step = 0;

    map_forge_workspace_authoring_host_reset(&host);
    map_forge_workspace_authoring_host_set_viewport(&host, 1280u, 720u);
    assert(map_forge_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(map_forge_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(map_forge_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(kit_workspace_authoring_ui_font_theme_build_layout(NULL, 1280, 720, &layout));

    inc_x = (int)(layout.text_size_inc_button.x + layout.text_size_inc_button.width * 0.5f);
    inc_y = (int)(layout.text_size_inc_button.y + layout.text_size_inc_button.height * 0.5f);
    assert(inc_x > 0 && inc_y > 0);

    (void)mapforge_shared_font_reset_zoom_step();
    base_step = mapforge_shared_font_zoom_step();
    click = mouse_event(SDL_MOUSEBUTTONDOWN, inc_x, inc_y);
    assert(map_forge_workspace_authoring_host_handle_sdl_event(&host, &click, 0));
    assert(host.font_theme_button_click_count == 1u);
    assert(host.font_theme_pending_changes == 1u);
    assert(host.last_font_theme_button_id == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC);
    assert(mapforge_shared_font_zoom_step() == base_step + 1);
    assert(map_forge_workspace_authoring_host_take_font_dirty(&host));
    assert(!map_forge_workspace_authoring_host_take_font_dirty(&host));
    (void)mapforge_shared_font_reset_zoom_step();
}

static void test_font_theme_cancel_restores_entry_baseline(void) {
    MapForgeWorkspaceAuthoringHostState host;
    char preset_name[64] = {0};
    char theme_name[64] = {0};

    (void)mapforge_shared_font_set_preset("ide");
    (void)mapforge_shared_font_reset_zoom_step();
    (void)mapforge_shared_theme_set_preset("midnight_contrast");

    map_forge_workspace_authoring_host_reset(&host);
    assert(map_forge_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(map_forge_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);

    assert(map_forge_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC));
    assert(map_forge_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT));
    assert(map_forge_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_STANDARD_GREY));
    assert(mapforge_shared_font_zoom_step() == 1);
    assert(mapforge_shared_font_current_preset(preset_name, sizeof(preset_name)));
    assert(strcmp(preset_name, "daw_default") == 0);
    assert(mapforge_shared_theme_current_preset(theme_name, sizeof(theme_name)));
    assert(strcmp(theme_name, "standard_grey") == 0);

    assert(map_forge_workspace_authoring_host_cancel(&host).code == CORE_OK);
    assert(!map_forge_workspace_authoring_host_active(&host));
    assert(mapforge_shared_font_zoom_step() == 0);
    assert(mapforge_shared_font_current_preset(preset_name, sizeof(preset_name)));
    assert(strcmp(preset_name, "ide") == 0);
    assert(mapforge_shared_theme_current_preset(theme_name, sizeof(theme_name)));
    assert(strcmp(theme_name, "midnight_contrast") == 0);
    assert(map_forge_workspace_authoring_host_take_font_dirty(&host));
}

static void test_font_theme_apply_accepts_draft(void) {
    MapForgeWorkspaceAuthoringHostState host;
    char preset_name[64] = {0};

    (void)mapforge_shared_font_set_preset("ide");
    (void)mapforge_shared_font_reset_zoom_step();
    (void)mapforge_shared_theme_set_preset("midnight_contrast");

    map_forge_workspace_authoring_host_reset(&host);
    assert(map_forge_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(map_forge_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(map_forge_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC));
    assert(map_forge_workspace_authoring_host_apply_font_theme_button(
        &host,
        KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT));

    assert(map_forge_workspace_authoring_host_apply(&host).code == CORE_OK);
    assert(!map_forge_workspace_authoring_host_active(&host));
    assert(map_forge_workspace_authoring_host_last_event_accepted(&host));
    assert(mapforge_shared_font_zoom_step() == 1);
    assert(mapforge_shared_font_current_preset(preset_name, sizeof(preset_name)));
    assert(strcmp(preset_name, "daw_default") == 0);

    (void)mapforge_shared_font_set_preset("ide");
    (void)mapforge_shared_font_reset_zoom_step();
    (void)mapforge_shared_theme_set_preset("midnight_contrast");
}

static void test_text_entry_blocks_entry(void) {
    MapForgeWorkspaceAuthoringHostState host;
    SDL_Event alt_c = key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    SDL_Event alt_v = key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);

    map_forge_workspace_authoring_host_reset(&host);
    assert(!map_forge_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 1));
    assert(!map_forge_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 1));
    assert(!map_forge_workspace_authoring_host_active(&host));
}

int main(void) {
    test_entry_chord_and_exit();
    test_reserved_capture_and_overlay_cycle();
    test_overlay_button_hit_testing();
    test_font_theme_button_hit_testing();
    test_font_theme_cancel_restores_entry_baseline();
    test_font_theme_apply_accepts_draft();
    test_text_entry_blocks_entry();
    puts("map_forge_workspace_authoring_host_test: success");
    return 0;
}
