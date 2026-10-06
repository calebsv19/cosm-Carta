#include "app/app_map_viewport_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_place_tools_internal.h"
#include "app/app_ui_internal.h"
#include "app/app_utility_workspace.h"
#include "app/place_repository.h"
#include "app/place_runtime.h"
#include "map/mercator.h"
#include "ui/font.h"
#include "ui/control_surface.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

PlaceTools tools;
static void tools_backups_load(AppState *app);
const char *const service_keys[] = {"dog_walking", "pet_sitting", "drop_in", "custom"};
const char *const service_labels[] = {"Walk", "Pet sit", "Drop-in", "Custom"};

// Reads typed optional text for UI inspection.
const char *tools_text(struct json_object *v, const char *key) {
    struct json_object *x = NULL;
    return v && json_object_object_get_ex(v, key, &x) && json_object_is_type(x, json_type_string)
               ? json_object_get_string(x)
               : "";
}
// Reads an optional scalar without manufacturing available values.
int64_t tools_int(struct json_object *v, const char *key) {
    struct json_object *x = NULL;
    return v && json_object_object_get_ex(v, key, &x) ? json_object_get_int64(x) : 0;
}
// Clones private data for a cancelable native editor.
static struct json_object *tools_clone(struct json_object *v) {
    return json_tokener_parse(json_object_to_json_string_ext(v, JSON_C_TO_STRING_PLAIN));
}
// Tests a current logical UI rectangle.
static bool tools_hit(SDL_FRect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// Centers or fits geographic places without modifying saved coordinates or routes.
static void tools_focus(AppState *app, bool selected) {
    double minx = INFINITY, miny = INFINITY, maxx = -INFINITY, maxy = -INFINITY;
    unsigned count = 0;
    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        const MapForgePin *pin = &app->pins_file.pins[i];
        if (selected ? (int)i != app->ui_state_bridge.pin_selected_index : !app_utility_pin_visible(app, pin))
            continue;
        MercatorMeters m = mercator_from_latlon((LatLon){pin->lat, pin->lon});
        minx = fmin(minx, m.x);
        maxx = fmax(maxx, m.x);
        miny = fmin(miny, m.y);
        maxy = fmax(maxy, m.y);
        ++count;
    }
    if (!count) {
        app_pin_editor_set_status(app, "No matching saved places to show.");
        return;
    }
    Camera *c = &app->view_state_bridge.camera;
    SDL_FRect v = app_map_viewport_rect(app);
    double available = fmax(100, v.w - app->ui_state_bridge.left_pane_rect.w - 40);
    double ppm =
        fmin(available / fmax(60, (maxx - minx) * 1.2), fmax(100, v.h - 50) / fmax(60, (maxy - miny) * 1.2));
    float zoom = selected ? 17 : app_clampf((float)log2(ppm * mercator_world_size_meters() / 256), 10, 18);
    double scale = 256 * pow(2, zoom) / mercator_world_size_meters();
    c->x = c->x_target = (float)((minx + maxx) * .5 - app->ui_state_bridge.left_pane_rect.w * .5 / scale);
    c->y = c->y_target = (float)((miny + maxy) * .5);
    c->zoom = c->zoom_target = zoom;
    c->heading_rad = c->heading_target_rad = 0;
    app_pin_editor_set_status(app, selected ? "Centered selected home." : "Showing matching saved places.");
}
// Returns modal ownership for the native/agent mutation guard.
bool app_place_tools_active(void) { return tools.kind != 0; }
// Releases editor/preview snapshots without changing personal records.
void app_place_tools_close(void) {
    if (tools.draft)
        json_object_put(tools.draft);
    if (tools.routes)
        json_object_put(tools.routes);
    if (tools.restore)
        json_object_put(tools.restore);
    memset(&tools, 0, sizeof(tools));
    tools.field = -1;
    tools.selected = -1;
    SDL_StopTextInput();
}
// Executes one repository operation and retains its visible result message.
static struct json_object *tools_command(AppState *app, struct json_object *request) {
    struct json_object *reply = map_forge_places_command(app->pins_path, &app->pins_file, request),
                       *ok = NULL;
    json_object_object_get_ex(reply, "ok", &ok);
    snprintf(tools.status, sizeof(tools.status), "%s",
             tools_text(reply, json_object_get_boolean(ok) ? "status" : "error"));
    return reply;
}
// Builds the current bounded saved-route page without reading map graph bytes.
static void tools_routes_load(AppState *app) {
    struct json_object *q = json_object_new_object();
    json_object_object_add(q, "operation", json_object_new_string("routes.list"));
    json_object_object_add(q, "offset", json_object_new_int(tools.route_page * 100));
    json_object_object_add(q, "limit", json_object_new_int(100));
    if (tools.routes)
        json_object_put(tools.routes);
    tools.routes = tools_command(app, q);
    json_object_put(q);
    tools.selected = -1;
    tools.scroll = 0;
    tools.field = -1;
}
// Copies one selected home's pets and per-pet roles into the cancelable form.
static void tools_profile_load(AppState *app, struct json_object *place) {
    struct json_object *profile = json_object_new_object(), *items = json_object_new_array(),
                       *home = json_object_new_array(), *pets = NULL, *services = NULL;
    json_object_object_get_ex(place, "pets", &pets);
    json_object_object_get_ex(place, "services", &services);
    for (size_t i = 0; pets && i < json_object_array_length(pets); ++i) {
        struct json_object *v = tools_clone(json_object_array_get_idx(pets, i)),
                           *kinds = json_object_new_array();
        for (size_t j = 0; services && j < json_object_array_length(services); ++j) {
            struct json_object *s = json_object_array_get_idx(services, j);
            if (!strcmp(tools_text(s, "pet_id"), tools_text(v, "id")))
                json_object_array_add(kinds, json_object_new_string(tools_text(s, "kind")));
        }
        json_object_object_add(v, "services", kinds);
        json_object_array_add(items, v);
    }
    for (size_t j = 0; services && j < json_object_array_length(services); ++j) {
        struct json_object *s = json_object_array_get_idx(services, j);
        if (!tools_text(s, "pet_id")[0])
            json_object_array_add(home, json_object_new_string(tools_text(s, "kind")));
    }
    /* Legacy manual service collections become explicit home roles when the editor first saves. */
    if (!services || !json_object_array_length(services)) {
        int selected = app->ui_state_bridge.pin_selected_index;
        if (selected >= 0 && (size_t)selected < app->pins_file.pin_count) {
            uint32_t mask = app->pins_file.pins[selected].collection_mask;
            if (mask & 2u)
                json_object_array_add(home, json_object_new_string("dog_walking"));
            if (mask & 4u)
                json_object_array_add(home, json_object_new_string("pet_sitting"));
        }
    }
    json_object_object_add(profile, "pets", items);
    json_object_object_add(profile, "home_services", home);
    json_object_object_add(tools.draft, "profile", profile);
    (void)app;
}
// Opens one dialog only after pending pin edits are saved or canceled.
void app_place_tools_open(AppState *app, unsigned kind) {
    if (app_places_has_unsaved_draft(app)) {
        app_pin_editor_set_status(app, "Save or Cancel the current pin draft first.");
        return;
    }
    app_place_tools_close();
    tools.kind = kind;
    tools.field = -1;
    tools.selected = -1;
    app->ui_state_bridge.place_search_active = false;
    app->ui_state_bridge.route_plan_name_active = false;
    app_pin_panel_name_edit_deactivate(app);
    snprintf(tools.store_token, sizeof(tools.store_token), "%016llx",
             (unsigned long long)app->pins_file.source_signature);
    if (kind == 1) {
        int i = app->ui_state_bridge.pin_selected_index;
        if (i < 0 || (size_t)i >= app->pins_file.pin_count) {
            app_place_tools_close();
            app_pin_editor_set_status(app, "Select a saved place first.");
            return;
        }
        struct json_object *q = json_object_new_object();
        json_object_object_add(q, "operation", json_object_new_string("places.get"));
        json_object_object_add(q, "place_id", json_object_new_string(app->pins_file.pins[i].id));
        struct json_object *r = tools_command(app, q), *places = NULL;
        json_object_object_get_ex(r, "places", &places);
        tools.draft = tools_clone(json_object_array_get_idx(places, 0));
        tools_profile_load(app, tools.draft);
        json_object_put(r);
        json_object_put(q);
        snprintf(
            tools.status, sizeof(tools.status),
            "Edits stay local. Save applies this home's profile; pet names follow their stable identity.");
    } else if (kind == 2)
        tools_routes_load(app);
    else {
        tools_backups_load(app);
        snprintf(tools.status, sizeof(tools.status),
                 "Backup includes places, pets, services, routes and history. Map packs are excluded.");
    }
}
// Resolves a selected route row from the retained read-only page.
struct json_object *tools_route_row(int ordinal) {
    struct json_object *rows = NULL;
    json_object_object_get_ex(tools.routes, "routes", &rows);
    for (size_t i = 0; rows && i < json_object_array_length(rows); ++i) {
        struct json_object *row = json_object_array_get_idx(rows, i), *intent = NULL;
        json_object_object_get_ex(row, "saved_intent", &intent);
        if (!tools.archived && tools_int(intent, "archived"))
            continue;
        if (ordinal-- == 0)
            return row;
    }
    return NULL;
}
// Resolves a form field to its selected object/property without borrowing mutable store records.
static struct json_object *tools_field_object(int field, const char **key, size_t *cap) {
    *cap = 128;
    if (tools.kind == 3) {
        *key = NULL;
        *cap = sizeof(tools.source);
        return NULL;
    }
    if (tools.kind == 2) {
        *key = "name";
        return NULL;
    }
    if (field < 3) {
        *key = field == 0 ? "name" : field == 1 ? "address" : "notes";
        *cap = field == 2 ? MAPFORGE_PIN_TEXT_CAPACITY : 128;
        if (field == 1)
            *cap = MAPFORGE_PIN_TEXT_CAPACITY;
        return tools.draft;
    }
    struct json_object *p = NULL, *pets = NULL;
    json_object_object_get_ex(tools.draft, "profile", &p);
    json_object_object_get_ex(p, "pets", &pets);
    *key = (field - 3) % 2 ? "species" : "name";
    *cap = (field - 3) % 2 ? 64 : 128;
    return json_object_array_get_idx(pets, (size_t)(field - 3) / 2);
}
// Applies text locally when changing field focus.
static void tools_field_finish(void) {
    if (tools.field < 0)
        return;
    const char *key = NULL;
    size_t cap = 0;
    struct json_object *v = tools_field_object(tools.field, &key, &cap);
    if (tools.kind == 3)
        snprintf(tools.source, sizeof(tools.source), "%s", tools.edit);
    else if (v && key)
        json_object_object_add(v, key, json_object_new_string(tools.edit));
}
// Starts native text editing on a drawn form field.
static void tools_field_begin(int field, const char *value) {
    // Finishing the prior draft may replace the JSON string supplying this field.
    char snapshot[sizeof(tools.edit)];snprintf(snapshot,sizeof(snapshot),"%s",value);
    tools_field_finish();
    tools.field = field;
    snprintf(tools.edit, sizeof(tools.edit), "%s", snapshot);
    const char *key=NULL;size_t capacity=0;tools_field_object(field,&key,&capacity);
    carta_text_bind(&tools.text,tools.edit,capacity<sizeof(tools.edit)?capacity:sizeof(tools.edit),true);
}

// Toggles one supported role without changing identity or inventing scheduling data.
static void tools_service_toggle(struct json_object *kinds, unsigned index) {
    for (size_t j = 0; j < json_object_array_length(kinds); ++j)
        if (!strcmp(service_keys[index], json_object_get_string(json_object_array_get_idx(kinds, j)))) {
            json_object_array_del_idx(kinds, j, 1);
            return;
        }
    json_object_array_add(kinds, json_object_new_string(service_keys[index]));
}
// Produces a private action identity for receipt-bound native mutations.
static void tools_action(char out[96]) {
    snprintf(out, 96, "native-edit-%llu", (unsigned long long)SDL_GetPerformanceCounter());
}
// Saves the full home profile through preview/revision/store guards and readback.
static void tools_profile_save(AppState *app) {
    tools_field_finish();
    struct json_object *q = json_object_new_object(), *p = NULL;
    char action[96];
    tools_action(action);
    json_object_object_add(q, "operation", json_object_new_string("intake.preview"));
    json_object_object_add(q, "request_id", json_object_new_string(action));
    json_object_object_add(q, "place_id", json_object_new_string(tools_text(tools.draft, "id")));
    json_object_object_add(q, "expected_revision", json_object_new_int64(tools_int(tools.draft, "revision")));
    const char *keys[] = {"name", "address", "notes"};
    for (unsigned i = 0; i < 3; ++i)
        json_object_object_add(q, keys[i], json_object_new_string(tools_text(tools.draft, keys[i])));
    json_object_object_get_ex(tools.draft, "profile", &p);
    json_object_object_add(q, "profile", tools_clone(p));
    struct json_object *preview = tools_command(app, q), *ok = NULL;
    json_object_object_get_ex(preview, "ok", &ok);
    if (json_object_get_boolean(ok) && !strcmp(tools.store_token, tools_text(preview, "store_token"))) {
        json_object_object_add(q, "expected_store_token",
                               json_object_new_string(tools_text(preview, "store_token")));
        json_object_object_add(q, "candidate_token",
                               json_object_new_string(tools_text(preview, "candidate_token")));
        json_object_object_add(q, "operation", json_object_new_string("places.update"));
        struct json_object *reply = tools_command(app, q);
        json_object_object_get_ex(reply, "ok", &ok);
        if (json_object_get_boolean(ok)) {
            char id[128];
            snprintf(id, sizeof(id), "%s", tools_text(tools.draft, "id"));
            app_places_invalidate_route(app);
            const MapForgePin *pin = map_forge_pins_find_by_id_const(&app->pins_file, id);
            if (pin)
                app_pin_editor_select_saved_pin(app, (int)(pin - app->pins_file.pins));
            app_place_tools_close();
            app_pin_editor_set_status(app,
                                      "Home details, pet associations and services saved and read back.");
        }
        json_object_put(reply);
    } else if (json_object_get_boolean(ok))
        snprintf(tools.status, sizeof(tools.status),
                 "Personal records changed; Cancel and reopen before saving.");
    json_object_put(preview);
    json_object_put(q);
}
// Updates only selected route metadata through revision-guarded management commands.
static void tools_route_manage(AppState *app, bool rename) {
    struct json_object *row = tools_route_row(tools.selected), *intent = NULL;
    if (!row)
        return;
    json_object_object_get_ex(row, "saved_intent", &intent);
    char action[96];
    tools_action(action);
    struct json_object *q = json_object_new_object();
    json_object_object_add(q, "operation",
                           json_object_new_string(rename ? "routes.rename" : "routes.archive"));
    json_object_object_add(q, "request_id", json_object_new_string(action));
    json_object_object_add(q, "route_id", json_object_new_string(tools_text(intent, "id")));
    json_object_object_add(q, "expected_revision", json_object_new_int64(tools_int(intent, "revision")));
    if (rename)
        json_object_object_add(q, "name", json_object_new_string(tools.edit));
    else
        json_object_object_add(q, "archived", json_object_new_boolean(!tools_int(intent, "archived")));
    struct json_object *reply = tools_command(app, q), *ok = NULL;
    json_object_object_get_ex(reply, "ok", &ok);
    if (json_object_get_boolean(ok)) {
        tools_routes_load(app);
        snprintf(tools.status, sizeof(tools.status), "%s",
                 rename ? "Route renamed." : "Archive state saved. Show archived to restore it.");
    }
    json_object_put(reply);
    json_object_put(q);
}
// Creates a unique private recovery/export path beside the personal database.
static bool tools_backup_path(AppState *app, char *out, size_t cap, bool export) {
    char dir[MAPFORGE_PIN_PATH_CAPACITY];
    snprintf(dir, sizeof(dir), "%s", app->pins_path);
    char *slash = strrchr(dir, '/');
    if (!slash)
        return false;
    *slash = 0;
    size_t n = strlen(dir);
    if (n + 9 >= sizeof(dir))
        return false;
    strcat(dir, "/backups");
    if (mkdir(dir, 0700) && access(dir, F_OK))
        return false;
    int result = snprintf(out, cap, "%s/places-%llu.%s", dir, (unsigned long long)SDL_GetPerformanceCounter(),
                          export ? "json" : "sqlite");
    return result > 0 && (size_t)result < cap;
}
// Lists recent private snapshots without opening or accepting them as restore candidates.
static void tools_backups_load(AppState *app) {
    if (tools.draft)
        json_object_put(tools.draft);
    tools.draft = json_object_new_array();
    char base[MAPFORGE_PIN_PATH_CAPACITY];
    snprintf(base, sizeof(base), "%s", app->pins_path);
    char *slash = strrchr(base, '/');
    if (!slash)
        return;
    *slash = 0;
    for (unsigned location = 0; location < 2; ++location) {
        char directory[MAPFORGE_PIN_PATH_CAPACITY];
        int n = snprintf(directory, sizeof(directory), "%s%s", base, location ? "" : "/backups");
        if (n <= 0 || (size_t)n >= sizeof(directory))
            continue;
        DIR *dir = opendir(directory);
        if (!dir)
            continue;
        struct dirent *entry;
        while ((entry = readdir(dir)) && json_object_array_length(tools.draft) < 256) {
            if (location && !strstr(entry->d_name, ".before-restore-"))
                continue;
            size_t length = strlen(entry->d_name);
            if (length < 7 || strcmp(entry->d_name + length - 7, ".sqlite"))
                continue;
            char path[MAPFORGE_PIN_PATH_CAPACITY];
            n = snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
            struct stat st;
            if (n <= 0 || (size_t)n >= sizeof(path) || lstat(path, &st) || !S_ISREG(st.st_mode))
                continue;
            struct json_object *v = json_object_new_object();
            json_object_object_add(v, "path", json_object_new_string(path));
            json_object_object_add(v, "name", json_object_new_string(entry->d_name));
            json_object_object_add(v, "modified", json_object_new_int64(st.st_mtime));
            json_object_array_add(tools.draft, v);
        }
        closedir(dir);
    }
    for (size_t i = 0; i < json_object_array_length(tools.draft); ++i)
        for (size_t j = i + 1; j < json_object_array_length(tools.draft); ++j) {
            struct json_object *a = json_object_array_get_idx(tools.draft, i),
                               *b = json_object_array_get_idx(tools.draft, j);
            if (tools_int(b, "modified") > tools_int(a, "modified")) {
                json_object_get(a);
                json_object_get(b);
                json_object_array_put_idx(tools.draft, i, b);
                json_object_array_put_idx(tools.draft, j, a);
            }
        }
}
// Runs backup/export or the deliberate preview-then-restore workflow.
static void tools_recovery(AppState *app, unsigned action) {
    tools_field_finish();
    struct json_object *q = json_object_new_object();
    char dest[MAPFORGE_PIN_PATH_CAPACITY], rid[96];
    if (action < 2) {
        if (!tools_backup_path(app, dest, sizeof(dest), action == 1)) {
            snprintf(tools.status, sizeof(tools.status), "Cannot create private backup directory.");
            json_object_put(q);
            return;
        }
        json_object_object_add(
            q, "operation", json_object_new_string(action == 0 ? "repository.backup" : "repository.export"));
        json_object_object_add(q, "destination", json_object_new_string(dest));
    } else {
        json_object_object_add(
            q, "operation",
            json_object_new_string(action == 2 ? "repository.restore.preview" : "repository.restore"));
        json_object_object_add(q, "source", json_object_new_string(tools.source));
        if (action == 3) {
            if (!tools.restore) {
                json_object_put(q);
                return;
            }
            tools_action(rid);
            json_object_object_add(q, "request_id", json_object_new_string(rid));
            json_object_object_add(q, "expected_store_token",
                                   json_object_new_string(tools_text(tools.restore, "store_token")));
            json_object_object_add(q, "source_token",
                                   json_object_new_string(tools_text(tools.restore, "source_token")));
        }
    }
    struct json_object *reply = tools_command(app, q), *ok = NULL;
    json_object_object_get_ex(reply, "ok", &ok);
    if (json_object_get_boolean(ok)) {
        if (action == 0) {
            tools_backups_load(app);
            snprintf(tools.source, sizeof(tools.source), "%s", dest);
            tools.field = -1;
        }
        if (action == 2) {
            if (tools.restore)
                json_object_put(tools.restore);
            tools.restore = tools_clone(reply);
            snprintf(
                tools.status, sizeof(tools.status),
                "Preview ready: Restore replaces current personal records; a recovery backup is made first.");
        }
        if (action == 3) {
            app_places_invalidate_route(app);
            app->ui_state_bridge.pin_selected_index = -1;
            app_pin_editor_cancel_draft(app);
            if (tools.restore)
                json_object_put(tools.restore);
            tools.restore = NULL;
            tools_backups_load(app);
            snprintf(tools.status, sizeof(tools.status),
                     "Restored and read back. Previous records remain in the before-restore backup.");
        }
        if (action < 2)
            app_pin_editor_set_statusf(app, "%s: %s", action == 0 ? "Backup verified" : "JSON exported",
                                       dest);
    } else if (action == 2) {
        if (tools.restore)
            json_object_put(tools.restore);
        tools.restore = NULL;
    }
    json_object_put(reply);
    json_object_put(q);
}

// Runs the existing toolbar policy only after semantic eligibility is established.
void app_place_tools_activate_toolbar(AppState *app,unsigned i) {
    if(i>=9 || tools.kind)return;
                if (i < 3) {
                    app->ui_state_bridge.service_filter = i == 1 ? 2 : i == 2 ? 4 : 0;
                    app->ui_state_bridge.place_search[0] = 0;
                    app->ui_state_bridge.pin_pane_row_base = 0;
                    app->ui_state_bridge.utility_workspace =
                        i == 0 ? APP_UTILITY_MAP : APP_UTILITY_DOG_WALKING;
                    app_pin_editor_set_status(app, i == 0   ? "All saved places."
                                                   : i == 1 ? "Walking clients."
                                                            : "Sitting clients.");
                }
                if (i == 3 || i == 4)
                    tools_focus(app, i == 4);
                if (i == 5)
                    app_place_tools_open(app, 1);
                if (i == 7)
                    app_place_tools_open(app, 3);
                if (i == 6) {
                    app->ui_state_bridge.place_search_active = true;
                    SDL_StartTextInput();
                }
                if (i == 8) {
                    app->ui_state_bridge.place_search[0] = 0;
                    app->ui_state_bridge.place_search_active = false;
                    app->ui_state_bridge.pin_pane_row_base = 0;
                    SDL_StopTextInput();
                }
    carta_ui_controls_sync(app);
}
// Consumes modal text editing, bounded scrolling and explicit actions before map input.
bool app_place_tools_handle_event(AppState *app, const SDL_Event *event) {
    if (!tools.kind)return carta_ui_controls_event(app,event);
    if (tools.field>=0) {
        KitUiTextEventResult text=carta_text_event(&tools.text,event);
        if(text.changed && tools.kind==3 && tools.restore) {json_object_put(tools.restore);tools.restore=NULL;}
        if(text.cancel){app_place_tools_close();return true;}
        if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_TAB && !event->key.repeat) {
            int direction=(event->key.keysym.mod&KMOD_SHIFT)?-1:1;
            for(unsigned n=1;n<=tools.field_count;n++) {
                int next=(tools.field+direction*(int)n+(int)tools.field_count*2)%(int)tools.field_count;
                if(tools.fields[next].w<=0)continue;
                const char *property=NULL;size_t capacity=0;
                struct json_object *object=tools_field_object(next,&property,&capacity);
                if(tools.kind==2) {
                    struct json_object *row=tools_route_row(tools.selected),*intent=NULL;
                    json_object_object_get_ex(row,"saved_intent",&intent);
                    tools_field_begin(next,tools_text(intent,"name"));
                } else tools_field_begin(next,tools.kind==3?tools.source:tools_text(object,property));
                break;
            }
            return true;
        }
        if(text.submit) {
            tools_field_finish();tools.field=-1;carta_text_end(&tools.text);SDL_StopTextInput();return true;
        }
        if(text.consumed)return true;
    }
    if(event->type==SDL_KEYDOWN){if(event->key.keysym.sym==SDLK_ESCAPE && !event->key.repeat)app_place_tools_close();return true;}
    if (event->type == SDL_MOUSEWHEEL) {
        tools_field_finish();
        tools.field = -1;
        SDL_StopTextInput();
        int max = 0;
        if (tools.kind == 1) {
            struct json_object *p = NULL, *pets = NULL;
            json_object_object_get_ex(tools.draft, "profile", &p);
            json_object_object_get_ex(p, "pets", &pets);
            max = (int)json_object_array_length(pets) * 82 + 330 - (int)tools_rect(app).h + 140;
            if (tools.linking)
                max += 30;
        } else if (tools.kind == 3) {
            max = tools.draft ? (int)json_object_array_length(tools.draft) : 0;
            int visible = (int)(tools_rect(app).h - 410) / 28;
            if (visible < 1)
                visible = 1;
            max -= visible;
        } else if (tools.kind == 2) {
            while (tools_route_row(max))
                ++max;
            int visible = (int)(tools_rect(app).h - 186) / 36;
            if (visible < 1)
                visible = 1;
            max -= visible;
        }
        tools.scroll -= event->wheel.y * (tools.kind == 1 ? 36 : 1);
        if (tools.scroll < 0)
            tools.scroll = 0;
        if (tools.scroll > max)
            tools.scroll = max > 0 ? max : 0;
        return true;
    }
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        int mx = event->button.x, my = event->button.y;
        SDL_FRect r = tools_rect(app);
        float x = r.x + 14, w = r.w - 28;
        if (tools_hit((SDL_FRect){r.x + r.w - 74, r.y + 8, 64, 26}, mx, my)) {
            app_place_tools_close();
            return true;
        }
        for (unsigned i = 0; i < tools.field_count; ++i)
            if (tools.fields[i].w > 0 && tools_hit(tools.fields[i], mx, my)) {
                const char *key = NULL;
                size_t cap = 0;
                struct json_object *v = tools_field_object((int)i, &key, &cap);
                if (tools.kind == 2) {
                    struct json_object *row = tools_route_row(tools.selected), *intent = NULL;
                    json_object_object_get_ex(row, "saved_intent", &intent);
                    tools_field_begin((int)i, tools_text(intent, "name"));
                } else if(tools.field==(int)i) {
                    carta_text_click(&tools.text,mx,my,(SDL_GetModState()&KMOD_SHIFT)!=0);
                    return true;
                } else
                    tools_field_begin((int)i, tools.kind == 3 ? tools.source : tools_text(v, key));
                return true;
            }
        if (tools.kind == 1) {
            float fy = r.y + r.h - 62;
            if (tools_hit((SDL_FRect){x, fy, 140, 24}, mx, my)) {
                tools_profile_save(app);
                return true;
            }
            if (tools_hit((SDL_FRect){x + 148, fy, 120, 24}, mx, my)) {
                app_place_tools_close();
                return true;
            }
            float y = r.y + 54 - tools.scroll + 66 + 144;
            struct json_object *p = NULL, *pets = NULL, *home = NULL;
            json_object_object_get_ex(tools.draft, "profile", &p);
            json_object_object_get_ex(p, "pets", &pets);
            json_object_object_get_ex(p, "home_services", &home);
            if (my < r.y + 52 || my >= r.y + r.h - 72)
                return true;
            for (unsigned j = 0; j < 4; ++j)
                if (tools_hit((SDL_FRect){x + j * w / 4, y + 18, w / 4 - 4, 22}, mx, my)) {
                    tools_service_toggle(home, j);
                    return true;
                }
            y += 48;
            if (tools_hit((SDL_FRect){x, y, 110, 22}, mx, my)) {
                tools_field_finish();
                tools.field = -1;
                if (json_object_array_length(pets) >= 32) {
                    snprintf(tools.status, sizeof(tools.status), "Editor limit: 32 pets per home.");
                    return true;
                }
                struct json_object *pet = json_object_new_object();
                char id[96];
                snprintf(id, sizeof(id), "native-pet-%llu", (unsigned long long)SDL_GetPerformanceCounter());
                json_object_object_add(pet, "id", json_object_new_string(id));
                json_object_object_add(pet, "name", json_object_new_string(""));
                json_object_object_add(pet, "species", json_object_new_string(""));
                json_object_object_add(pet, "services", json_object_new_array());
                json_object_array_add(pets, pet);
                snprintf(tools.status, sizeof(tools.status),
                         "New pet added to draft; enter its name and service roles, then Save.");
                return true;
            }
            if (tools_hit((SDL_FRect){x + 118, y, 150, 22}, mx, my)) {
                tools.linking = !tools.linking;
                return true;
            }
            y += 30;
            if (tools.linking) {
                struct json_object *all = NULL;
                json_object_object_get_ex(app->pins_file.source_json, "pets", &all);
                size_t count = all ? json_object_array_length(all) : 0;
                if (tools_hit((SDL_FRect){x, y, w - 82, 22}, mx, my)) {
                    ++tools.link_index;
                    return true;
                }
                if (count && tools_hit((SDL_FRect){x + w - 76, y, 76, 22}, mx, my)) {
                    struct json_object *pet =
                        json_object_array_get_idx(all, (size_t)tools.link_index % count);
                    bool duplicate = false;
                    for (size_t i = 0; i < json_object_array_length(pets); ++i)
                        if (!strcmp(tools_text(json_object_array_get_idx(pets, i), "id"),
                                    tools_text(pet, "id")))
                            duplicate = true;
                    if (!duplicate && json_object_array_length(pets) < 32) {
                        struct json_object *copy = tools_clone(pet);
                        json_object_object_add(copy, "services", json_object_new_array());
                        json_object_array_add(pets, copy);
                        tools.linking = false;
                    } else
                        snprintf(tools.status, sizeof(tools.status),
                                 "Pet already linked or editor capacity reached.");
                    return true;
                }
                y += 30;
            }
            for (size_t i = 0; i < json_object_array_length(pets); ++i) {
                struct json_object *pet = json_object_array_get_idx(pets, i), *kinds = NULL;
                json_object_object_get_ex(pet, "services", &kinds);
                y += 48;
                if (tools_hit((SDL_FRect){x + w - 84, y, 84, 22}, mx, my)) {
                    tools_field_finish();
                    tools.field = -1;
                    json_object_array_del_idx(pets, i, 1);
                    snprintf(tools.status, sizeof(tools.status),
                             "Detached in draft; the pet record and other homes will be preserved.");
                    return true;
                }
                for (unsigned j = 0; j < 4; ++j)
                    if (tools_hit((SDL_FRect){x + j * (w - 90) / 4, y, (w - 90) / 4 - 4, 22}, mx, my)) {
                        tools_service_toggle(kinds, j);
                        return true;
                    }
                y += 34;
            }
        } else if (tools.kind == 2) {
            if (tools_hit((SDL_FRect){x, r.y + 54, 160, 24}, mx, my)) {
                tools.archived = !tools.archived;
                tools.selected = -1;
                tools.scroll = 0;
                tools.field = -1;
                return true;
            }
            int visible = (int)(r.h - 186) / 36;
            if (visible < 1)
                visible = 1;
            for (int i = 0; i < visible; ++i)
                if (tools_hit((SDL_FRect){x, r.y + 104 + i * 36, w * .46f, 30}, mx, my) &&
                    tools_route_row(tools.scroll + i)) {
                    tools.selected = tools.scroll + i;
                    struct json_object *row = tools_route_row(tools.selected), *intent = NULL;
                    json_object_object_get_ex(row, "saved_intent", &intent);
                    tools_field_begin(0, tools_text(intent, "name"));
                    return true;
                }
            for (unsigned i = 0; i < 5; ++i)
                if (tools_hit((SDL_FRect){x + i * w / 5, r.y + r.h - 62, w / 5 - 5, 24}, mx, my)) {
                    if (i == 0) {
                        struct json_object *row = tools_route_row(tools.selected), *intent = NULL;
                        if (row) {
                            json_object_object_get_ex(row, "saved_intent", &intent);
                            char id[128];
                            snprintf(id, sizeof(id), "%s", tools_text(intent, "id"));
                            app_place_tools_close();
                            app_places_open_route(app, id);
                        }
                    }
                    if (i == 1 || i == 2)
                        tools_route_manage(app, i == 1);
                    if (i == 3 && tools.route_page > 0) {
                        --tools.route_page;
                        tools_routes_load(app);
                    }
                    if (i == 4 && (tools.route_page + 1) * 100 < tools_int(tools.routes, "total")) {
                        ++tools.route_page;
                        tools_routes_load(app);
                    }
                    return true;
                }
        } else {
            for (unsigned i = 0; i < 4; ++i)
                if (tools_hit((SDL_FRect){x + i * w / 4, r.y + 180, w / 4 - 6, 26}, mx, my)) {
                    tools_recovery(app, i);
                    return true;
                }
            int visible = (int)(r.h - 410) / 28;
            if (visible < 1)
                visible = 1;
            for (int i = 0; i < visible && tools.draft &&
                            (size_t)(tools.scroll + i) < json_object_array_length(tools.draft);
                 ++i)
                if (tools_hit((SDL_FRect){x, r.y + 330 + i * 28, w, 24}, mx, my)) {
                    tools_field_finish();
                    tools.field = -1;
                    SDL_StopTextInput();
                    struct json_object *v =
                        json_object_array_get_idx(tools.draft, (size_t)(tools.scroll + i));
                    snprintf(tools.source, sizeof(tools.source), "%s", tools_text(v, "path"));
                    if (tools.restore)
                        json_object_put(tools.restore);
                    tools.restore = NULL;
                    return true;
                }
        }
        return true;
    }
    return event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
           event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEWHEEL || event->type == SDL_KEYUP ||
           event->type == SDL_TEXTINPUT;
}
