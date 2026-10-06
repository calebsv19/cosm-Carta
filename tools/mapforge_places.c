#include "app/place_repository.h"
#include "core_io.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Executes a structured request locally or submits it to the running native owner.
int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "--request")) {
        fprintf(stderr,
                "usage: MAPFORGE_RUNTIME_DIR=/absolute/runtime mapforge_places --request request.json\n");
        return 2;
    }
    const char *runtime = getenv("MAPFORGE_RUNTIME_DIR");
    if (!runtime || runtime[0] != '/') {
        fprintf(stderr, "An absolute MAPFORGE_RUNTIME_DIR is required.\n");
        return 2;
    }
    CoreBuffer bytes = {0};
    if (core_io_read_all(argv[2], &bytes).code != CORE_OK || bytes.size > 65536u || !bytes.size) {
        core_io_buffer_free(&bytes);
        fprintf(stderr, "Request unreadable or exceeds 64 KiB.\n");
        return 2;
    }
    struct json_tokener *tokener = json_tokener_new();
    json_tokener_set_flags(tokener, JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    struct json_object *request = json_tokener_parse_ex(tokener, (const char *)bytes.data, (int)bytes.size);
    bool valid = json_tokener_get_error(tokener) == json_tokener_success;
    for (size_t i = json_tokener_get_parse_end(tokener); valid && i < bytes.size; ++i)
        if (!strchr(" \t\r\n", bytes.data[i]) || bytes.data[i] == 0)
            valid = false;
    json_tokener_free(tokener);
    core_io_buffer_free(&bytes);
    if (!valid || !request || !json_object_is_type(request, json_type_object)) {
        if (request)
            json_object_put(request);
        fprintf(stderr, "Invalid request JSON.\n");
        return 2;
    }
    char path[MAPFORGE_PIN_PATH_CAPACITY], error[256] = {0};
    struct json_object *reply = NULL;
    if (!map_forge_pins_workspace_path(path, sizeof(path)))
        return 2;
    int lock = map_forge_places_lock(path);
    if (lock >= 0) {
        MapForgePinsFile file;
        map_forge_pins_file_init(&file);
        if (map_forge_pins_load_workspace(&file, path, sizeof(path), error, sizeof(error)))
            reply = map_forge_places_command(path, &file, request);
        else
            fprintf(stderr, "%s\n", error);
        map_forge_pins_file_free(&file);
        close(lock);
    } else {
        char inbox[MAPFORGE_PIN_PATH_CAPACITY + 32], submitted[MAPFORGE_PIN_PATH_CAPACITY + 96],
            response[MAPFORGE_PIN_PATH_CAPACITY + 96];
        snprintf(inbox, sizeof(inbox), "%s.commands", path);
        if (access(inbox, F_OK) != 0) {
            fprintf(stderr, "Owner unavailable; no command inbox.\n");
            json_object_put(request);
            return 1;
        }
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        snprintf(submitted, sizeof(submitted), "%s/%ld-%ld.request.json", inbox, (long)getpid(), now.tv_nsec);
        snprintf(response, sizeof(response), "%s/%ld-%ld.reply.json", inbox, (long)getpid(), now.tv_nsec);
        const char *text = json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN);
        if (core_io_write_all_atomic(submitted, text, strlen(text)).code == CORE_OK) {
            chmod(submitted, 0600);
            for (int attempt = 0; attempt < 200; ++attempt) {
                if (access(response, F_OK) == 0) {
                    reply = json_object_from_file(response);
                    unlink(response);
                    break;
                }
                struct timespec delay = {0, 50000000};
                nanosleep(&delay, NULL);
            }
        }
        if (!reply) {
            unlink(submitted);
            fprintf(stderr, "Owner did not reply; retry the same request_id to resolve uncertainty.\n");
        }
    }
    json_object_put(request);
    if (!reply)
        return 1;
    puts(json_object_to_json_string_ext(reply, JSON_C_TO_STRING_PRETTY));
    struct json_object *ok = NULL;
    bool succeeded = json_object_object_get_ex(reply, "ok", &ok) && json_object_get_boolean(ok);
    json_object_put(reply);
    return succeeded ? 0 : 1;
}
