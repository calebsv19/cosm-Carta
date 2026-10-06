#include "app/place_repository.h"
#include "app/history.h"
#include "app/place_routes.h"
#include "app/place_maintenance.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

// Reads a typed text field, returning an empty value for absent optional text.
static const char *command_text(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    return object && json_object_object_get_ex(object, key, &value) &&
                   json_object_is_type(value, json_type_string)
               ? json_object_get_string(value)
               : "";
}
// Clones retained metadata without aliasing the caller's current document.
static struct json_object *command_clone(struct json_object *object) {
    return object ? json_tokener_parse(json_object_to_json_string_ext(object, JSON_C_TO_STRING_PLAIN))
                  : json_object_new_object();
}
// Returns a named array, creating it only on the private mutation document.
static struct json_object *command_array(struct json_object *root, const char *key) {
    struct json_object *value = NULL;
    if (!json_object_object_get_ex(root, key, &value)) {
        value = json_object_new_array();
        json_object_object_add(root, key, value);
    }
    return json_object_is_type(value, json_type_array) ? value : NULL;
}
// Finds stable identity in an array without assuming names are unique.
static struct json_object *command_find(struct json_object *array, const char *id) {
    for (size_t i = 0u; array && i < json_object_array_length(array); ++i) {
        struct json_object *value = json_object_array_get_idx(array, i);
        if (strcmp(command_text(value, "id"), id) == 0)
            return value;
    }
    return NULL;
}
// Computes deterministic request-local identities and retry fingerprints.
static uint64_t command_hash(const char *text) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
// Formats a request hash as opaque text rather than a lossy JSON number.
static void command_token(uint64_t value, char token[32]) {
    snprintf(token, 32, "%016llx", (unsigned long long)value);
}
// Produces explicit success/error replies for both native and CLI callers.
static struct json_object *command_reply(bool ok, const char *message) {
    struct json_object *reply = json_object_new_object();
    json_object_object_add(reply, "ok", json_object_new_boolean(ok));
    if (message)
        json_object_object_add(reply, ok ? "status" : "error", json_object_new_string(message));
    return reply;
}
// Performs case-insensitive local search without network access.
static bool command_contains(const char *haystack, const char *needle) {
    if (!needle[0])
        return true;
    size_t length = strlen(needle);
    for (const char *p = haystack; *p; ++p) {
        size_t i = 0u;
        while (i < length && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
            ++i;
        if (i == length)
            return true;
    }
    return false;
}
// Extracts a finite numeric coordinate and rejects strings/booleans.
static bool command_number(struct json_object *object, const char *key, double *out) {
    struct json_object *value = NULL;
    if (!json_object_object_get_ex(object, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int)))
        return false;
    *out = json_object_get_double(value);
    return isfinite(*out);
}
// Writes bounded text into typed pin fields without truncating or accepting embedded NUL.
static bool command_copy(char *out, size_t capacity, struct json_object *request, const char *key) {
    struct json_object *value = NULL;
    if (!json_object_object_get_ex(request, key, &value))
        return true;
    if (!json_object_is_type(value, json_type_string))
        return false;
    const char *text = json_object_get_string(value);
    size_t n = (size_t)json_object_get_string_len(value);
    if (n >= capacity || strlen(text) != n)
        return false;
    memcpy(out, text, n + 1u);
    return true;
}
// Supplies UTC creation/update times for durable place records.
static void command_stamp(char out[64]) {
    time_t now = time(NULL);
    struct tm t;
    gmtime_r(&now, &t);
    strftime(out, 64, "%Y-%m-%dT%H:%M:%SZ", &t);
}
// Searches place fields plus linked pet names and service kinds.
static struct json_object *command_search(MapForgePinsFile *file, struct json_object *request) {
    const char *query = command_text(request, "query"), *id = command_text(request, "place_id");
    struct json_object *reply = command_reply(true, "read"), *results = json_object_new_array();
    struct json_object *records = NULL, *pets = NULL, *links = NULL, *services = NULL;
    json_object_object_get_ex(file->source_json, "pins", &records);
    json_object_object_get_ex(file->source_json, "pets", &pets);
    json_object_object_get_ex(file->source_json, "pet_places", &links);
    json_object_object_get_ex(file->source_json, "services", &services);
    for (size_t i = 0u; i < file->pin_count; ++i) {
        const MapForgePin *pin = &file->pins[i];
        if (id[0] && strcmp(id, pin->id))
            continue;
        bool match = command_contains(pin->name, query) || command_contains(pin->address, query);
        struct json_object *stored = command_find(records, pin->id), *evidence = NULL;
        json_object_object_get_ex(stored, "location_evidence", &evidence);
        match |= command_contains(command_text(evidence, "input_address"), query) ||
                 command_contains(command_text(evidence, "matched_address"), query);
        struct json_object *place_pets = json_object_new_array(), *place_services = json_object_new_array();
        for (size_t j = 0u; links && j < json_object_array_length(links); ++j) {
            struct json_object *link = json_object_array_get_idx(links, j);
            if (strcmp(command_text(link, "place_id"), pin->id))
                continue;
            struct json_object *pet = command_find(pets, command_text(link, "pet_id"));
            if (pet) {
                match |= command_contains(command_text(pet, "name"), query);
                json_object_array_add(place_pets, command_clone(pet));
            }
        }
        for (size_t j = 0u; services && j < json_object_array_length(services); ++j) {
            struct json_object *service = json_object_array_get_idx(services, j);
            if (strcmp(command_text(service, "place_id"), pin->id))
                continue;
            match |= command_contains(command_text(service, "kind"), query);
            json_object_array_add(place_services, command_clone(service));
        }
        if (match && json_object_array_length(results) < 100u) {
            struct json_object *record = command_clone(command_find(records, pin->id));
            json_object_object_add(record, "pets", place_pets);
            json_object_object_add(record, "services", place_services);
            json_object_array_add(results, record);
        } else {
            json_object_put(place_pets);
            json_object_put(place_services);
        }
    }
    json_object_object_add(reply, "places", results);
    return reply;
}
// Applies one validated intake/correction to a private document and typed place view.
static bool command_intake(MapForgePinsFile *next, struct json_object *request, struct json_object *reply,
                           char *error, size_t capacity) {
    struct json_object *location = NULL;
    json_object_object_get_ex(request, "location", &location);
    if (location) {
        double lat = 0.0, lon = 0.0;
        if (!command_number(location, "lat", &lat) || !command_number(location, "lon", &lon) ||
            fabs(lat) > 85.05112878 || fabs(lon) > 180.0) {
            snprintf(error, capacity, "Invalid resolved coordinates");
            return false;
        }
    }
    const char *operation = command_text(request, "operation"), *place_id = command_text(request, "place_id");
    struct json_object *expected_revision = NULL;
    bool update = strcmp(operation, "places.update") == 0 ||
                  json_object_object_get_ex(request, "expected_revision", &expected_revision);
    MapForgePin *existing = place_id[0] ? map_forge_pins_find_by_id(next, place_id) : NULL;
    if (place_id[0] && !existing) {
        snprintf(error, capacity, "Unknown place_id");
        return false;
    }
    if (!existing && command_text(request, "address")[0]) {
        for (size_t i = 0u; i < next->pin_count; ++i) {
            if (next->pins[i].address[0] &&
                strcasecmp(next->pins[i].address, command_text(request, "address")) == 0) {
                if (existing) {
                    snprintf(error, capacity, "Ambiguous address; supply place_id");
                    return false;
                }
                existing = &next->pins[i];
            }
        }
    }
    if (update && !existing) {
        snprintf(error, capacity, "Corrections require place_id");
        return false;
    }
    MapForgePin pin = {0};
    if (existing)
        pin = *existing;
    else {
        char token[32];
        command_token(command_hash(command_text(request, "request_id")), token);
        snprintf(pin.id, sizeof(pin.id), "place-%s", token);
        pin.private_flag = true;
        pin.collection_mask = 1u;
        command_stamp(pin.created_at);
        if (map_forge_pins_find_by_id(next, pin.id)) {
            snprintf(error, capacity, "Generated identity collision");
            return false;
        }
    }
    if (!existing || update) {
        if (!command_copy(pin.name, sizeof(pin.name), request, "name") ||
            !command_copy(pin.address, sizeof(pin.address), request, "address") ||
            !command_copy(pin.notes, sizeof(pin.notes), request, "notes"))
            return false;
        if (location) {
            if (!command_number(location, "lat", &pin.lat) || !command_number(location, "lon", &pin.lon) ||
                !command_copy(pin.location_source, sizeof(pin.location_source), location, "source") ||
                !command_copy(pin.location_quality, sizeof(pin.location_quality), location, "quality"))
                return false;
        } else if (!existing) {
            snprintf(error, capacity, "A resolved location is required");
            return false;
        }
        if (!pin.name[0] || fabs(pin.lat) > 85.05112878 || fabs(pin.lon) > 180.0 ||
            !pin.location_quality[0]) {
            snprintf(error, capacity, "Name, supported coordinates and location quality are required");
            return false;
        }
        if (strcmp(pin.location_quality, "manual") && strcmp(pin.location_quality, "address_point") &&
            strcmp(pin.location_quality, "building_centroid") &&
            strcmp(pin.location_quality, "approximate")) {
            snprintf(error, capacity, "Unsupported location quality");
            return false;
        }
        struct json_object *allow = NULL;
        if (!strcmp(pin.location_quality, "approximate") &&
            (!json_object_object_get_ex(request, "accept_approximate", &allow) ||
             !json_object_get_boolean(allow))) {
            snprintf(error, capacity, "Approximate match requires explicit acceptance");
            return false;
        }
    }
    struct json_object *records = command_array(next->source_json, "pins");
    struct json_object *record = command_find(records, pin.id);
    if (!record) {
        record = json_object_new_object();
        json_object_object_add(record, "id", json_object_new_string(pin.id));
        json_object_array_add(records, record);
    }
    struct json_object *revision = NULL, *expected = NULL;
    json_object_object_get_ex(record, "revision", &revision);
    int64_t rev = revision ? json_object_get_int64(revision) : 0;
    if (update && (!json_object_object_get_ex(request, "expected_revision", &expected) ||
                   !json_object_is_type(expected, json_type_int) || json_object_get_int64(expected) != rev)) {
        snprintf(error, capacity, "Stale or missing expected_revision");
        return false;
    }
    if (update) {
        struct json_object *history = command_array(next->source_json, "place_history");
        if (!history || json_object_array_length(history) >= 2000u) {
            snprintf(error, capacity, "Place revision history capacity reached");
            return false;
        }
        struct json_object *event = json_object_new_object();
        json_object_object_add(event, "place_id", json_object_new_string(pin.id));
        json_object_object_add(event, "previous", command_clone(record));
        json_object_object_add(event, "request_id",
                               json_object_new_string(command_text(request, "request_id")));
        json_object_array_add(history, event);
    }
    json_object_object_add(record, "revision", json_object_new_int64(rev + 1));
    if (location && (!existing || update))
        json_object_object_add(record, "location_evidence", command_clone(location));
    struct json_object *pets = command_array(next->source_json, "pets"),
                       *links = command_array(next->source_json, "pet_places"),
                       *services = command_array(next->source_json, "services");
    if (!pets || !links || !services)
        return false;
    struct json_object *pet_input = NULL, *pet = NULL;
    json_object_object_get_ex(request, "pet", &pet_input);
    if (pet_input) {
        if (!json_object_is_type(pet_input, json_type_object) || !command_text(pet_input, "name")[0])
            return false;
        const char *supplied = command_text(pet_input, "id");
        if (supplied[0]) {
            pet = command_find(pets, supplied);
            if (!pet) {
                snprintf(error, capacity, "Unknown pet identity");
                return false;
            }
        } else {
            if (existing) {
                for (size_t i = 0u; i < json_object_array_length(links); ++i) {
                    struct json_object *link = json_object_array_get_idx(links, i);
                    if (strcmp(command_text(link, "place_id"), pin.id))
                        continue;
                    struct json_object *candidate = command_find(pets, command_text(link, "pet_id"));
                    if (candidate &&
                        !strcasecmp(command_text(candidate, "name"), command_text(pet_input, "name"))) {
                        if (pet || (command_text(pet_input, "species")[0] &&
                                    strcmp(command_text(pet_input, "species"),
                                           command_text(candidate, "species")))) {
                            snprintf(error, capacity, "Ambiguous existing pet; supply pet.id");
                            return false;
                        }
                        pet = candidate;
                    }
                }
            }
            if (!pet) {
                char token[32], id[64];
                command_token(command_hash(command_text(request, "request_id")), token);
                snprintf(id, sizeof(id), "pet-%s", token);
                if (command_find(pets, id))
                    return false;
                pet = command_clone(pet_input);
                json_object_object_add(pet, "id", json_object_new_string(id));
                json_object_array_add(pets, pet);
            }
        }
        bool linked = false;
        for (size_t i = 0u; i < json_object_array_length(links); ++i) {
            struct json_object *v = json_object_array_get_idx(links, i);
            if (!strcmp(command_text(v, "pet_id"), command_text(pet, "id")) &&
                !strcmp(command_text(v, "place_id"), pin.id))
                linked = true;
        }
        if (!linked) {
            struct json_object *v = json_object_new_object();
            json_object_object_add(v, "pet_id", json_object_new_string(command_text(pet, "id")));
            json_object_object_add(v, "place_id", json_object_new_string(pin.id));
            json_object_object_add(v, "role", json_object_new_string("home"));
            json_object_array_add(links, v);
        }
    }
    struct json_object *kinds = NULL;
    json_object_object_get_ex(request, "services", &kinds);
    if (kinds && (!json_object_is_type(kinds, json_type_array) || json_object_array_length(kinds) > 16u))
        return false;
    for (size_t i = 0u; kinds && i < json_object_array_length(kinds); ++i) {
        struct json_object *v = json_object_array_get_idx(kinds, i);
        if (!json_object_is_type(v, json_type_string))
            return false;
        const char *kind = json_object_get_string(v), *pet_id = pet ? command_text(pet, "id") : "";
        if (strcmp(kind, "dog_walking") && strcmp(kind, "pet_sitting") && strcmp(kind, "drop_in") &&
            strcmp(kind, "custom")) {
            snprintf(error, capacity, "Unsupported service kind");
            return false;
        }
        bool found = false;
        for (size_t j = 0u; j < json_object_array_length(services); ++j) {
            struct json_object *s = json_object_array_get_idx(services, j);
            if (!strcmp(command_text(s, "place_id"), pin.id) && !strcmp(command_text(s, "pet_id"), pet_id) &&
                !strcmp(command_text(s, "kind"), kind))
                found = true;
        }
        if (!found) {
            char id[160];
            snprintf(id, sizeof(id), "service-%016llx-%zu",
                     (unsigned long long)command_hash(command_text(request, "request_id")), i);
            struct json_object *s = json_object_new_object();
            json_object_object_add(s, "id", json_object_new_string(id));
            json_object_object_add(s, "place_id", json_object_new_string(pin.id));
            json_object_object_add(s, "pet_id", json_object_new_string(pet_id));
            json_object_object_add(s, "kind", json_object_new_string(kind));
            json_object_array_add(services, s);
        }
        if (!strcmp(kind, "dog_walking"))
            pin.collection_mask |= 2u;
        if (!strcmp(kind, "pet_sitting") || !strcmp(kind, "drop_in"))
            pin.collection_mask |= 4u;
    }
    struct json_object *profile = NULL;
    if (json_object_object_get_ex(request, "profile", &profile)) {
        if (!update || !map_forge_place_profile_apply(next, &pin, profile, request, error, capacity))
            return false;
    }
    command_stamp(pin.updated_at);
    if (!map_forge_pins_upsert(next, &pin, error, capacity))
        return false;
    json_object_object_add(reply, "place_id", json_object_new_string(pin.id));
    json_object_object_add(reply, "revision", json_object_new_int64(rev + 1));
    if (pet)
        json_object_object_add(reply, "pet_id", json_object_new_string(command_text(pet, "id")));
    return true;
}
// Executes preview/read commands or a receipt-bound atomic local mutation.
struct json_object *map_forge_places_command(const char *path, MapForgePinsFile *file,
                                             struct json_object *request) {
    if (!request || !json_object_is_type(request, json_type_object))
        return command_reply(false, "Request must be an object");
    const char *operation = command_text(request, "operation");
    if (!strncmp(operation, "history.", 8)) return history_command(path,file,request);
    if (!strncmp(operation, "repository.", 11))
        return map_forge_place_maintenance_command(path, file, request);
    if (!strcmp(operation, "intake.receipt")) {
        struct json_object *receipts = NULL;
        json_object_object_get_ex(file->source_json, "receipts", &receipts);
        struct json_object *record = command_find(receipts, command_text(request, "request_id")),
                           *receipt = NULL;
        struct json_object *reply = command_reply(true, record ? "replayed" : "not_found");
        if (record && json_object_object_get_ex(record, "receipt", &receipt))
            json_object_object_add(reply, "receipt", command_clone(receipt));
        return reply;
    }
    if (!strcmp(operation, "routes.preview") || !strcmp(operation, "routes.save") ||
        !strcmp(operation, "routes.get") || !strcmp(operation, "routes.list") ||
        !strcmp(operation, "routes.rename") || !strcmp(operation, "routes.archive"))
        return map_forge_place_route_command(path, file, request);
    if (!strcmp(operation, "places.search") || !strcmp(operation, "places.get"))
        return command_search(file, request);
    bool preview = !strcmp(operation, "intake.preview");
    if (!preview && strcmp(operation, "intake.commit") && strcmp(operation, "places.update"))
        return command_reply(false, "Unknown operation");
    const char *request_id = command_text(request, "request_id");
    if (!request_id[0] || strlen(request_id) > 128u)
        return command_reply(false, "Bounded request_id is required");
    struct json_object *candidate_input = command_clone(request);
    json_object_object_del(candidate_input, "operation");
    json_object_object_del(candidate_input, "expected_store_token");
    json_object_object_del(candidate_input, "candidate_token");
    char candidate_token[32];
    command_token(command_hash(json_object_to_json_string_ext(candidate_input, JSON_C_TO_STRING_PLAIN)),
                  candidate_token);
    json_object_put(candidate_input);
    if (!preview && strcmp(command_text(request, "candidate_token"), candidate_token))
        return command_reply(false, "Resolved candidate changed since preview");
    char fingerprint[32];
    command_token(command_hash(json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN)), fingerprint);
    struct json_object *receipts = NULL;
    json_object_object_get_ex(file->source_json, "receipts", &receipts);
    struct json_object *old = command_find(receipts, request_id);
    if (!preview && old) {
        if (strcmp(command_text(old, "fingerprint"), fingerprint))
            return command_reply(false, "request_id already used for different input");
        struct json_object *receipt = NULL;
        json_object_object_get_ex(old, "receipt", &receipt);
        return command_clone(receipt);
    }
    MapForgePinsFile next;
    map_forge_pins_file_init(&next);
    const char *text =
        json_object_to_json_string_ext(file->source_json, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    char error[256] = {0};
    if (!map_forge_pins_decode_json(text, strlen(text), &next, error, sizeof(error)))
        return command_reply(false, error);
    struct json_object *reply = command_reply(true, preview ? "preview" : "saved");
    if (!command_intake(&next, request, reply, error, sizeof(error))) {
        json_object_put(reply);
        map_forge_pins_file_free(&next);
        return command_reply(false, error[0] ? error : "Invalid intake fields");
    }
    char token[32];
    command_token(file->source_signature, token);
    json_object_object_add(reply, "store_token", json_object_new_string(token));
    json_object_object_add(reply, "candidate_token", json_object_new_string(candidate_token));
    json_object_object_add(reply, "request_id", json_object_new_string(request_id));
    json_object_object_add(reply, "client_input_token",
                           json_object_new_string(command_text(request, "client_input_token")));
    if (preview) {
        map_forge_pins_file_free(&next);
        return reply;
    }
    if (strcmp(command_text(request, "expected_store_token"), token)) {
        json_object_put(reply);
        map_forge_pins_file_free(&next);
        return command_reply(false, "Preview is stale or missing expected_store_token");
    }
    receipts = command_array(next.source_json, "receipts");
    if (!receipts || json_object_array_length(receipts) >= 10000u) {
        json_object_put(reply);
        map_forge_pins_file_free(&next);
        return command_reply(false,
                             "Receipt capacity reached; archive through a supported maintenance operation");
    }
    struct json_object *record = json_object_new_object();
    json_object_object_add(record, "id", json_object_new_string(request_id));
    json_object_object_add(record, "fingerprint", json_object_new_string(fingerprint));
    json_object_object_add(record, "receipt", command_clone(reply));
    json_object_array_add(receipts, record);
    if (!map_forge_pins_save(path, &next, error, sizeof(error))) {
        json_object_put(reply);
        map_forge_pins_file_free(&next);
        return command_reply(false, error);
    }
    MapForgePinsFile readback;
    map_forge_pins_file_init(&readback);
    if (!map_forge_pins_load(path, &readback, error, sizeof(error))) {
        json_object_put(reply);
        map_forge_pins_file_free(&next);
        return command_reply(false, "Commit persisted but readback failed; retry the same request_id");
    }
    map_forge_pins_file_free(&next);
    map_forge_pins_file_free(file);
    *file = readback;
    return reply;
}
