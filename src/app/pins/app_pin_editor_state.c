#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void app_pin_editor_set_status(AppState *app, const char *message) {
    if (!app) {
        return;
    }
    snprintf(app->ui_state_bridge.pin_editor_status,
             sizeof(app->ui_state_bridge.pin_editor_status),
             "%s",
             message ? message : "");
}

void app_pin_editor_set_statusf(AppState *app, const char *fmt, ...) {
    va_list args;
    if (!app || !fmt) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(app->ui_state_bridge.pin_editor_status,
              sizeof(app->ui_state_bridge.pin_editor_status),
              fmt,
              args);
    va_end(args);
}

static void app_pin_editor_sync_name_cursor_to_end(AppState *app) {
    size_t len = 0u;
    if (!app) {
        return;
    }
    len = strnlen(app->ui_state_bridge.pin_editor_name_edit,
                  sizeof(app->ui_state_bridge.pin_editor_name_edit));
    app->ui_state_bridge.pin_name_edit_active = false;
    app->ui_state_bridge.pin_name_cursor_index = (int)len;
}

static void app_pin_editor_sync_draft_name(AppState *app) {
    if (!app) {
        return;
    }
    snprintf(app->ui_state_bridge.pin_editor_name_edit,
             sizeof(app->ui_state_bridge.pin_editor_name_edit),
             "%s",
             app->ui_state_bridge.pin_editor_draft.name);
    app_pin_editor_sync_name_cursor_to_end(app);
}

static void app_pin_editor_clear_draft(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_editor_has_draft = false;
    app->ui_state_bridge.pin_editor_is_new = false;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
    app->ui_state_bridge.pin_add_mode_active = false;
    memset(&app->ui_state_bridge.pin_editor_draft, 0, sizeof(app->ui_state_bridge.pin_editor_draft));
    app->ui_state_bridge.pin_editor_name_edit[0] = '\0';
    app->ui_state_bridge.pin_name_edit_active = false;
    app->ui_state_bridge.pin_name_cursor_index = 0;
}

static int app_pin_editor_next_untitled_index(const AppState *app) {
    int max_index = -1;
    if (!app) {
        return 0;
    }
    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        const char *name = app->pins_file.pins[i].name;
        int index = -1;
        if (sscanf(name, "Untitled %d", &index) == 1 && index >= max_index) {
            max_index = index + 1;
        }
    }
    return max_index < 0 ? 0 : max_index;
}

static void app_pin_editor_stamp_now(char *dst, size_t dst_size) {
    time_t now = 0;
    struct tm local_tm;
    if (!dst || dst_size == 0u) {
        return;
    }
    now = time(NULL);
    memset(&local_tm, 0, sizeof(local_tm));
#if defined(_POSIX_VERSION)
    localtime_r(&now, &local_tm);
#else
    {
        struct tm *tmp = localtime(&now);
        if (tmp) {
            local_tm = *tmp;
        }
    }
#endif
    strftime(dst, dst_size, "%Y-%m-%dT%H:%M:%S", &local_tm);
}

static void app_pin_editor_slugify_name(const char *name, char *out_slug, size_t out_size) {
    size_t write_index = 0u;
    bool last_was_sep = false;
    if (!out_slug || out_size == 0u) {
        return;
    }
    out_slug[0] = '\0';
    if (!name) {
        return;
    }
    for (size_t i = 0; name[i] != '\0' && write_index + 1u < out_size; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (isalnum(ch)) {
            out_slug[write_index++] = (char)tolower(ch);
            last_was_sep = false;
            continue;
        }
        if (!last_was_sep && write_index > 0u) {
            out_slug[write_index++] = '_';
            last_was_sep = true;
        }
    }
    while (write_index > 0u && out_slug[write_index - 1u] == '_') {
        write_index -= 1u;
    }
    out_slug[write_index] = '\0';
}

void app_pin_editor_select_saved_pin(AppState *app, int index) {
    if (!app) {
        return;
    }
    if (index < 0 || index >= (int)app->pins_file.pin_count) {
        app->ui_state_bridge.pin_selected_index = -1;
        app_pin_editor_clear_draft(app);
        return;
    }
    app->ui_state_bridge.pin_selected_index = index;
    app->ui_state_bridge.pin_add_mode_active = false;
    app->ui_state_bridge.pin_editor_has_draft = true;
    app->ui_state_bridge.pin_editor_is_new = false;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
    app->ui_state_bridge.pin_editor_draft = app->pins_file.pins[index];
    app_pin_editor_sync_draft_name(app);
}

void app_pin_editor_begin_new_pin(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_selected_index = -1;
    app->ui_state_bridge.pin_add_mode_active = true;
    app->ui_state_bridge.pin_editor_has_draft = true;
    app->ui_state_bridge.pin_editor_is_new = true;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = true;
    memset(&app->ui_state_bridge.pin_editor_draft, 0, sizeof(app->ui_state_bridge.pin_editor_draft));
    snprintf(app->ui_state_bridge.pin_editor_draft.type,
             sizeof(app->ui_state_bridge.pin_editor_draft.type),
             "general");
    snprintf(app->ui_state_bridge.pin_editor_draft.color,
             sizeof(app->ui_state_bridge.pin_editor_draft.color),
             "blue");
    app->ui_state_bridge.pin_editor_draft.private_flag = true;
    snprintf(app->ui_state_bridge.pin_editor_name_edit,
             sizeof(app->ui_state_bridge.pin_editor_name_edit),
             "Untitled %d",
             app_pin_editor_next_untitled_index(app));
    app_pin_editor_sync_name_cursor_to_end(app);
    app_pin_editor_set_status(app, "Add mode active. Right-click the map to create pins.");
}

bool app_pin_editor_save_draft(AppState *app) {
    char error[256];
    char slug[MAPFORGE_PIN_ID_CAPACITY];
    char candidate[MAPFORGE_PIN_ID_CAPACITY];
    char stamp[64];
    int selected_index = -1;
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return false;
    }
    snprintf(app->ui_state_bridge.pin_editor_draft.name,
             sizeof(app->ui_state_bridge.pin_editor_draft.name),
             "%s",
             app->ui_state_bridge.pin_editor_name_edit);
    if (app->ui_state_bridge.pin_editor_draft.name[0] == '\0') {
        app_pin_editor_set_status(app, "Pin name is required before saving.");
        return false;
    }
    if (app->ui_state_bridge.pin_editor_waiting_for_map_click) {
        app_pin_editor_set_status(app, "Place the draft on the map before saving.");
        return false;
    }
    if (app->ui_state_bridge.pin_editor_is_new || app->ui_state_bridge.pin_editor_draft.id[0] == '\0') {
        app_pin_editor_slugify_name(app->ui_state_bridge.pin_editor_draft.name,
                                    slug,
                                    sizeof(slug));
        if (slug[0] == '\0') {
            snprintf(slug, sizeof(slug), "pin_%zu", app->pins_file.pin_count + 1u);
        }
        snprintf(candidate, sizeof(candidate), "%s", slug);
        for (size_t attempt = 2u;
             map_forge_pins_find_by_id_const(&app->pins_file, candidate) != NULL &&
             attempt < 1000u;
             ++attempt) {
            snprintf(candidate, sizeof(candidate), "%s_%zu", slug, attempt);
        }
        snprintf(app->ui_state_bridge.pin_editor_draft.id,
                 sizeof(app->ui_state_bridge.pin_editor_draft.id),
                 "%s",
                 candidate);
        app_pin_editor_stamp_now(stamp, sizeof(stamp));
        snprintf(app->ui_state_bridge.pin_editor_draft.created_at,
                 sizeof(app->ui_state_bridge.pin_editor_draft.created_at),
                 "%s",
                 stamp);
    }
    app_pin_editor_stamp_now(stamp, sizeof(stamp));
    snprintf(app->ui_state_bridge.pin_editor_draft.updated_at,
             sizeof(app->ui_state_bridge.pin_editor_draft.updated_at),
             "%s",
             stamp);
    if (!map_forge_pins_upsert(&app->pins_file,
                               &app->ui_state_bridge.pin_editor_draft,
                               error,
                               sizeof(error))) {
        app_pin_editor_set_status(app, error);
        return false;
    }
    snprintf(app->pins_file.map_region,
             sizeof(app->pins_file.map_region),
             "%s",
             app->region.name ? app->region.name : "");
    if (!map_forge_pins_save(app->pins_path, &app->pins_file, error, sizeof(error))) {
        app_pin_editor_set_status(app, error);
        return false;
    }
    app->pins_dirty = false;
    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        if (strcmp(app->pins_file.pins[i].id, app->ui_state_bridge.pin_editor_draft.id) == 0) {
            selected_index = (int)i;
            break;
        }
    }
    app_pin_editor_select_saved_pin(app, selected_index);
    app_pin_editor_set_status(app, "Pin saved.");
    return true;
}

bool app_pin_editor_delete_selected(AppState *app) {
    char error[256];
    if (!app) {
        return false;
    }
    if (app->ui_state_bridge.pin_editor_is_new) {
        app_pin_editor_clear_draft(app);
        app_pin_editor_set_status(app, "Draft pin discarded.");
        return true;
    }
    if (app->ui_state_bridge.pin_selected_index < 0 ||
        app->ui_state_bridge.pin_selected_index >= (int)app->pins_file.pin_count) {
        app_pin_editor_set_status(app, "Select a saved pin to delete.");
        return false;
    }
    if (!map_forge_pins_remove_by_id(&app->pins_file, app->ui_state_bridge.pin_editor_draft.id)) {
        app_pin_editor_set_status(app, "Failed to remove pin.");
        return false;
    }
    if (strcmp(app->ui_state_bridge.pin_route_start_id, app->ui_state_bridge.pin_editor_draft.id) == 0) {
        app_pin_panel_clear_route_start(app);
    }
    if (strcmp(app->ui_state_bridge.pin_route_goal_id, app->ui_state_bridge.pin_editor_draft.id) == 0) {
        app_pin_panel_clear_route_goal(app);
    }
    if (!map_forge_pins_save(app->pins_path, &app->pins_file, error, sizeof(error))) {
        app_pin_editor_set_status(app, error);
        return false;
    }
    app->pins_dirty = false;
    if (app->ui_state_bridge.pin_selected_index >= (int)app->pins_file.pin_count) {
        app->ui_state_bridge.pin_selected_index = (int)app->pins_file.pin_count - 1;
    }
    if (app->pins_file.pin_count > 0u) {
        app_pin_editor_select_saved_pin(app, app->ui_state_bridge.pin_selected_index);
    } else {
        app_pin_editor_select_saved_pin(app, -1);
    }
    app_pin_editor_set_status(app, "Pin deleted.");
    return true;
}

void app_pin_editor_cancel_draft(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_add_mode_active = false;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
    if (app->ui_state_bridge.pin_selected_index >= 0 &&
        app->ui_state_bridge.pin_selected_index < (int)app->pins_file.pin_count) {
        app_pin_editor_select_saved_pin(app, app->ui_state_bridge.pin_selected_index);
        app_pin_editor_set_status(app, "Edits reverted.");
    } else {
        app_pin_editor_select_saved_pin(app, -1);
        app_pin_editor_set_status(app, "Draft cleared.");
    }
}
