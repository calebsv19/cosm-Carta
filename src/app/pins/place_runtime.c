#include "app/place_runtime.h"
#include "app/history_ui.h"
#include "app/app_place_tools.h"
#include "app/app_pin_panel_internal.h"
#include "app/place_repository.h"
#include "core_io.h"
#include "map/mercator.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int owner_fd = -1;
static void app_places_activate_route(AppState *app, struct json_object *reply);
static bool app_places_route_action(AppState *app, bool save);
// Claims one namespace while allowing repeated region reloads in the same owner.
bool app_places_owner_acquire(void) {
    if (owner_fd >= 0)
        return true;
    char path[MAPFORGE_PIN_PATH_CAPACITY], inbox[MAPFORGE_PIN_PATH_CAPACITY + 32];
    if (!map_forge_pins_workspace_path(path, sizeof(path)))
        return false;
    owner_fd = map_forge_places_lock(path);
    if (owner_fd < 0)
        return false;
    snprintf(inbox, sizeof(inbox), "%s.commands", path);
    if (mkdir(inbox, 0700) != 0 && access(inbox, F_OK) != 0) {
        app_places_owner_release();
        return false;
    }
    return true;
}
// Releases the process-local owner descriptor after shutdown.
void app_places_owner_release(void) {
    if (owner_fd >= 0)
        close(owner_fd);
    owner_fd = -1;
}
// Processes at most one command per frame using the exact repository service used by CLI.
void app_places_poll_commands(AppState *app) {
    static uint32_t next_poll = 0u;
    uint32_t now = SDL_GetTicks();
    if (owner_fd < 0 || (int32_t)(now - next_poll) < 0)
        return;
    next_poll = now + 250u;
    char inbox[MAPFORGE_PIN_PATH_CAPACITY + 32];
    snprintf(inbox, sizeof(inbox), "%s.commands", app->pins_path);
    DIR *dir = opendir(inbox);
    if (!dir)
        return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        size_t n = strlen(entry->d_name);
        const char *suffix = ".request.json";
        if (n <= strlen(suffix) || strcmp(entry->d_name + n - strlen(suffix), suffix))
            continue;
        char path[MAPFORGE_PIN_PATH_CAPACITY + 320], reply_path[MAPFORGE_PIN_PATH_CAPACITY + 320];
        snprintf(path, sizeof(path), "%s/%s", inbox, entry->d_name);
        snprintf(reply_path, sizeof(reply_path), "%s/%.*s.reply.json", inbox, (int)(n - strlen(suffix)),
                 entry->d_name);
        struct stat statbuf;
        struct json_object *request = NULL, *reply = NULL;
        if (lstat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode) && statbuf.st_size > 0 &&
            statbuf.st_size <= 65536)
            request = json_object_from_file(path);
        struct json_object *op = NULL;
        const char *operation =
            request && json_object_object_get_ex(request, "operation", &op) ? json_object_get_string(op) : "";
        bool read =
            operation && (!strcmp(operation, "places.get") || !strcmp(operation, "places.search") ||
                          !strcmp(operation, "intake.preview") || !strcmp(operation, "intake.receipt") ||
                          !strcmp(operation, "routes.preview") || !strcmp(operation, "routes.get") ||
                          !strcmp(operation, "routes.list") || !strcmp(operation, "repository.backup") ||
                          !strcmp(operation, "repository.export") || !strcmp(operation, "repository.restore.preview") ||
                          !strcmp(operation,"history.nearby") || !strcmp(operation,"history.list") || !strcmp(operation,"history.get") ||
                          !strcmp(operation,"history.point") || !strcmp(operation,"history.export") ||
                          !strcmp(operation,"history.preview"));
        if ((app_places_has_unsaved_draft(app) || app_place_tools_active() || history_ui_modal()) && !read)
            reply = json_tokener_parse("{\"ok\":false,\"error\":\"Finish Save or Cancel on the selected "
                                       "draft before an agent mutation\"}");
        else
            reply = map_forge_places_command(app->pins_path, &app->pins_file, request);
        if (!app_place_tools_active() && operation && (!strcmp(operation, "routes.get") || !strcmp(operation, "routes.preview")))
            app_places_activate_route(app, reply);
        struct json_object *ok = NULL, *id = NULL;
        bool place_mutation =
            operation && (!strcmp(operation, "intake.commit") || !strcmp(operation, "places.update"));
        if (place_mutation && json_object_object_get_ex(reply, "ok", &ok) && json_object_get_boolean(ok)) {
            app_places_invalidate_route(app);
            if (json_object_object_get_ex(reply, "place_id", &id)) {
                const MapForgePin *pin =
                    map_forge_pins_find_by_id_const(&app->pins_file, json_object_get_string(id));
                if (pin) {
                    app_pin_editor_select_saved_pin(app, (int)(pin - app->pins_file.pins));
                    MercatorMeters point = mercator_from_latlon((LatLon){pin->lat, pin->lon});
                    app->view_state_bridge.camera.x = app->view_state_bridge.camera.x_target = (float)point.x;
                    app->view_state_bridge.camera.y = app->view_state_bridge.camera.y_target = (float)point.y;
                    if (app->view_state_bridge.camera.zoom_target < 15.0f)
                        app->view_state_bridge.camera.zoom = app->view_state_bridge.camera.zoom_target =
                            16.0f;
                }
            }
            app_pin_editor_set_status(app, "Agent update saved and read back.");
        }
        /* Commands can change selection, camera, status or repository membership. */
        app->runtime_redraw_requested = true;
        const char *text = json_object_to_json_string_ext(reply, JSON_C_TO_STRING_PRETTY);
        if (core_io_write_all_atomic(reply_path, text, strlen(text)).code == CORE_OK) {
            chmod(reply_path, 0600);
            unlink(path);
        }
        if (request)
            json_object_put(request);
        json_object_put(reply);
        break;
    }
    closedir(dir);
}

// Invalidates active endpoints and any queued or late results after a saved-place change.
void app_places_invalidate_route(AppState *app) {
    app_pin_panel_clear_route_bindings(app);
    app->worker_state_bridge.route_latest_submitted_id = 0u;
    app->route_state_bridge.route_recompute_scheduled = false;
    if (app->lifetime.route_worker_initialized) {
        pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
        app->worker_state_bridge.route_job_pending = false;
        pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
    }
    route_state_clear(&app->route_state_bridge.route);
}
// Activates freshly computed place-ID endpoints without altering home markers.
static void app_places_activate_route(AppState *app, struct json_object *reply) {
    struct json_object *from = NULL, *to = NULL, *ok = NULL, *mode = NULL;
    if (!json_object_object_get_ex(reply, "ok", &ok) || !json_object_get_boolean(ok) ||
        !json_object_object_get_ex(reply, "from_place_id", &from) ||
        !json_object_object_get_ex(reply, "to_place_id", &to))
        return;
    struct json_object *region = NULL;
    if (!json_object_object_get_ex(reply, "region", &region) || !app->region.name ||
        strcmp(json_object_get_string(region), app->region.name)) {
        app_pin_editor_set_status(app, "Route computed; open its region to display it.");
        return;
    }
    const MapForgePin *a = map_forge_pins_find_by_id_const(&app->pins_file, json_object_get_string(from)),
                      *b = map_forge_pins_find_by_id_const(&app->pins_file, json_object_get_string(to));
    if (!a || !b)
        return;
    if (json_object_object_get_ex(reply, "mode", &mode))
        app->route_state_bridge.route.mode =
            !strcmp(json_object_get_string(mode), "walk") ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
    struct json_object *objective = NULL;
    if (json_object_object_get_ex(reply, "objective", &objective))
        app->route_state_bridge.route.objective = !strcmp(json_object_get_string(objective), "time")
                                                      ? ROUTE_OBJECTIVE_LOWEST_TIME
                                                      : ROUTE_OBJECTIVE_SHORTEST_DISTANCE;
    MercatorMeters pa = mercator_from_latlon((LatLon){a->lat, a->lon}),
                   pb = mercator_from_latlon((LatLon){b->lat, b->lon});
    RouteEndpointAnchor aa, bb;
    if (!app_pick_route_anchor_unbounded(app, (float)pa.x, (float)pa.y, &aa) ||
        !app_pick_route_anchor_unbounded(app, (float)pb.x, (float)pb.y, &bb))
        return;
    app_route_service_set_endpoint_anchor(app, true, &aa, 0);
    app_route_service_set_endpoint_anchor(app, false, &bb, 0);
    snprintf(app->ui_state_bridge.pin_route_start_id, sizeof(app->ui_state_bridge.pin_route_start_id), "%s",
             a->id);
    snprintf(app->ui_state_bridge.pin_route_goal_id, sizeof(app->ui_state_bridge.pin_route_goal_id), "%s",
             b->id);
    struct json_object *stale = NULL;
    bool changed = json_object_object_get_ex(reply, "stale", &stale) && json_object_get_boolean(stale);
    struct json_object *saved = NULL, *name = NULL;
    char status[256];
    if (json_object_object_get_ex(reply, "saved_intent", &saved) &&
        json_object_object_get_ex(saved, "name", &name))
        snprintf(status, sizeof(status), "%s%s",
                 changed ? "Recomputed changed route: " : "Opened route: ", json_object_get_string(name));
    else
        snprintf(status, sizeof(status), "Place route computed from current locations.");
    app_pin_editor_set_status(app, status);
}
// Opens one selected saved route and exposes any query refusal without changing the stored intent.
void app_places_open_route(AppState *app, const char *route_id) {
    struct json_object *q=json_object_new_object();
    json_object_object_add(q,"operation",json_object_new_string("routes.get"));
    json_object_object_add(q,"route_id",json_object_new_string(route_id));
    struct json_object *reply=map_forge_places_command(app->pins_path,&app->pins_file,q),*error=NULL;
    app_places_activate_route(app,reply);
    if (json_object_object_get_ex(reply,"error",&error)) app_pin_editor_set_status(app,json_object_get_string(error));
    json_object_put(reply);json_object_put(q);
}
// Reads validated string fields from a private document without coercion.
static const char *app_places_text(struct json_object *record, const char *key) {
    struct json_object *value = NULL;
    return record && json_object_object_get_ex(record, key, &value) &&
                   json_object_is_type(value, json_type_string)
               ? json_object_get_string(value)
               : "";
}
// Formats linked names and deduplicated service roles within the sidebar text capacity.
void app_places_detail_text(const AppState *app, const char *place_id, char *text, size_t capacity) {
    if (!capacity)
        return;
    snprintf(text, capacity, "Pets: ");
    struct json_object *links = NULL, *pets = NULL, *services = NULL;
    json_object_object_get_ex(app->pins_file.source_json, "pet_places", &links);
    json_object_object_get_ex(app->pins_file.source_json, "pets", &pets);
    json_object_object_get_ex(app->pins_file.source_json, "services", &services);
    unsigned names = 0;
    for (size_t i = 0; links && i < json_object_array_length(links); ++i) {
        struct json_object *link = json_object_array_get_idx(links, i);
        if (strcmp(app_places_text(link, "place_id"), place_id))
            continue;
        for (size_t j = 0; pets && j < json_object_array_length(pets); ++j) {
            struct json_object *pet = json_object_array_get_idx(pets, j);
            if (!strcmp(app_places_text(pet, "id"), app_places_text(link, "pet_id"))) {
                size_t n = strlen(text);
                snprintf(text + n, capacity - n, "%s%s", names++ ? ", " : "", app_places_text(pet, "name"));
            }
        }
    }
    size_t n = strlen(text);
    snprintf(text + n, capacity - n, "%s | Services: ", names ? "" : "none");
    unsigned mask = 0;
    const char *kinds[] = {"dog_walking", "pet_sitting", "drop_in", "custom"};
    const char *labels[] = {"Walk", "Pet sit", "Drop-in", "Custom"};
    for (size_t i = 0; services && i < json_object_array_length(services); ++i) {
        struct json_object *service = json_object_array_get_idx(services, i);
        if (strcmp(app_places_text(service, "place_id"), place_id))
            continue;
        for (unsigned j = 0; j < 4u; ++j) {
            if (!(mask & (1u << j)) && !strcmp(app_places_text(service, "kind"), kinds[j])) {
                n = strlen(text);
                snprintf(text + n, capacity - n, "%s%s", mask ? ", " : "", labels[j]);
                mask |= 1u << j;
            }
        }
    }
    if (!mask) {
        n = strlen(text);
        snprintf(text + n, capacity - n, "none");
    }
}
// Returns a visible control row above the saved-place list.
SDL_FRect app_places_route_button_rect(const AppState *app, uint32_t index) {
    SDL_FRect status = app->ui_state_bridge.pin_pane_status_rect;
    float width = (status.w - 8.0f) * 0.5f;
    return (SDL_FRect){status.x + index * (width + 8.0f), status.y + status.h + 8.0f, width, 22.0f};
}
// Handles local search, route naming and visible saved-route actions before map input.
bool app_places_handle_event(AppState *app, const SDL_Event *event) {
    if (app->ui_state_bridge.route_plan_name_active) {
        char *name = app->ui_state_bridge.route_plan_name;
        if (event->type == SDL_TEXTINPUT) {
            size_t n = strlen(name), m = strlen(event->text.text);
            if (n + m < sizeof(app->ui_state_bridge.route_plan_name))
                memcpy(name + n, event->text.text, m + 1u);
            return true;
        }
        if (event->type == SDL_KEYDOWN) {
            if ((event->key.keysym.mod & KMOD_CTRL) && event->key.keysym.sym == SDLK_a)
                name[0] = 0;
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                size_t n = strlen(name);
                if (n) {
                    --n;
                    while (n && ((unsigned char)name[n] & 0xc0) == 0x80)
                        --n;
                    name[n] = 0;
                }
            }
            if (event->key.keysym.sym == SDLK_ESCAPE || event->key.keysym.sym == SDLK_RETURN) {
                app->ui_state_bridge.route_plan_name_active = false;
                SDL_StopTextInput();
                if (event->key.keysym.sym == SDLK_RETURN)
                    return app_places_route_action(app, true);
            }
            return true;
        }
    }
    if (app->ui_state_bridge.route_plan_name_active &&
        (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEWHEEL || event->type == SDL_KEYUP))
        return true;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT &&
        app->ui_state_bridge.left_pane_open &&
        app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS) {
        for (uint32_t i = 0; i < 2u; ++i) {
            SDL_FRect rect = app_places_route_button_rect(app, i);
            if (event->button.x >= rect.x && event->button.x <= rect.x + rect.w &&
                event->button.y >= rect.y && event->button.y <= rect.y + rect.h) {
                SDL_Event key = {0};
                key.type = SDL_KEYDOWN;
                key.key.keysym.mod = KMOD_CTRL | KMOD_SHIFT;
                key.key.keysym.sym = i == 0 ? SDLK_s : SDLK_o;
                return app_places_handle_event(app, &key);
            }
        }
    }
    if (event->type == SDL_KEYDOWN && (event->key.keysym.mod & KMOD_CTRL) &&
        event->key.keysym.sym == SDLK_f) {
        app->ui_state_bridge.place_search_active = true;
        app->ui_state_bridge.left_pane_open = true;
        app->ui_state_bridge.left_pane_section = APP_LEFT_PANE_SECTION_PINS;
        SDL_StartTextInput();
        return true;
    }
    if (app->ui_state_bridge.place_search_active) {
        if (event->type == SDL_TEXTINPUT) {
            size_t n = strlen(app->ui_state_bridge.place_search), m = strlen(event->text.text);
            if (n + m < sizeof(app->ui_state_bridge.place_search))
                memcpy(app->ui_state_bridge.place_search + n, event->text.text, m + 1u);
            app->ui_state_bridge.pin_pane_row_base = 0;
            return true;
        }
        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym == SDLK_BACKSPACE) {
                size_t n = strlen(app->ui_state_bridge.place_search);
                if (n) {
                    --n;
                    while (n && ((unsigned char)app->ui_state_bridge.place_search[n] & 0xc0) == 0x80)
                        --n;
                    app->ui_state_bridge.place_search[n] = 0;
                }
                app->ui_state_bridge.pin_pane_row_base = 0;
            }
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                app->ui_state_bridge.place_search[0] = 0;
                app->ui_state_bridge.place_search_active = false;
                SDL_StopTextInput();
            }
            if (event->key.keysym.sym == SDLK_RETURN) {
                app->ui_state_bridge.place_search_active = false;
                SDL_StopTextInput();
            }
            return true;
        }
    }
    if (event->type != SDL_KEYDOWN || !(event->key.keysym.mod & KMOD_CTRL) ||
        !(event->key.keysym.mod & KMOD_SHIFT))
        return false;
    bool save = event->key.keysym.sym == SDLK_s, open = event->key.keysym.sym == SDLK_o;
    if (!save && !open)
        return false;
    if (open) {
        app_place_tools_open(app, 2);
        return true;
    }
    if (!app->ui_state_bridge.pin_route_start_id[0] || !app->ui_state_bridge.pin_route_goal_id[0]) {
        app_pin_editor_set_status(app, "Select saved start and goal places before saving a route.");
        return true;
    }
    app->ui_state_bridge.route_plan_name_active = true;
    app->ui_state_bridge.place_search_active = false;
    if (!app->ui_state_bridge.route_plan_name[0])
        snprintf(app->ui_state_bridge.route_plan_name, sizeof(app->ui_state_bridge.route_plan_name),
                 "Saved route");
    app_pin_editor_set_status(app, "Enter a route name; Ctrl+A replaces it, Enter saves, Esc cancels.");
    SDL_StartTextInput();
    return true;
}
// Saves or reopens place-ID route intent through the same command service.
static bool app_places_route_action(AppState *app, bool save) {
    bool open = !save;
    struct json_object *request = json_object_new_object(), *reply = NULL;
    if (open) {
        struct json_object *plans = NULL;
        json_object_object_get_ex(app->pins_file.source_json, "route_plans", &plans);
        if (!plans || !json_object_array_length(plans)) {
            app_pin_editor_set_status(app, "No saved routes yet.");
            json_object_put(request);
            return true;
        }
        struct json_object *plan =
                               json_object_array_get_idx(plans, app->ui_state_bridge.saved_route_cursor++ %
                                                                    json_object_array_length(plans)),
                           *id = NULL;
        json_object_object_get_ex(plan, "id", &id);
        json_object_object_add(request, "operation", json_object_new_string("routes.get"));
        json_object_object_add(request, "route_id", json_object_get(id));
        reply = map_forge_places_command(app->pins_path, &app->pins_file, request);
        app_places_activate_route(app, reply);
    } else {
        char id[96];
        snprintf(id, sizeof(id), "native-route-%llu", (unsigned long long)SDL_GetPerformanceCounter());
        json_object_object_add(request, "operation", json_object_new_string("routes.preview"));
        json_object_object_add(request, "request_id", json_object_new_string(id));
        json_object_object_add(request, "name", json_object_new_string(app->ui_state_bridge.route_plan_name));
        json_object_object_add(request, "from_place_id",
                               json_object_new_string(app->ui_state_bridge.pin_route_start_id));
        json_object_object_add(request, "to_place_id",
                               json_object_new_string(app->ui_state_bridge.pin_route_goal_id));
        json_object_object_add(request, "region",
                               json_object_new_string(app->region.name ? app->region.name : ""));
        json_object_object_add(
            request, "mode",
            json_object_new_string(app->route_state_bridge.route.mode == ROUTE_MODE_WALK ? "walk" : "car"));
        json_object_object_add(
            request, "objective",
            json_object_new_string(app->route_state_bridge.route.objective == ROUTE_OBJECTIVE_LOWEST_TIME
                                       ? "time"
                                       : "distance"));
        reply = map_forge_places_command(app->pins_path, &app->pins_file, request);
        struct json_object *token = NULL;
        if (json_object_object_get_ex(reply, "preview_token", &token)) {
            json_object_object_add(request, "preview_token", json_object_get(token));
            json_object_object_add(request, "operation", json_object_new_string("routes.save"));
            json_object_put(reply);
            reply = map_forge_places_command(app->pins_path, &app->pins_file, request);
            app_pin_editor_set_status(app, "Route saved. Ctrl+Shift+O reopens saved routes.");
        }
    }
    struct json_object *error = NULL;
    if (json_object_object_get_ex(reply, "error", &error))
        app_pin_editor_set_status(app, json_object_get_string(error));
    json_object_put(reply);
    json_object_put(request);
    return true;
}
