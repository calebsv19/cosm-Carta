#include "app/place_maintenance.h"
#include <stdio.h>
#include <string.h>

// Reads a typed profile field without coercion.
static const char *profile_text(struct json_object *v, const char *key) {
    struct json_object *x = NULL;
    return v && json_object_object_get_ex(v, key, &x) && json_object_is_type(x, json_type_string)
               ? json_object_get_string(x)
               : "";
}
// Copies one preserved record into a private mutation/history document.
static struct json_object *profile_clone(struct json_object *v) {
    return json_tokener_parse(json_object_to_json_string_ext(v, JSON_C_TO_STRING_PLAIN));
}
// Finds a globally stable pet identity.
static struct json_object *profile_find(struct json_object *pets, const char *id) {
    for (size_t i = 0; pets && i < json_object_array_length(pets); ++i) {
        struct json_object *v = json_object_array_get_idx(pets, i);
        if (!strcmp(profile_text(v, "id"), id))
            return v;
    }
    return NULL;
}
// Rejects invalid input without changing the caller's authoritative snapshot.
static bool profile_error(char *error, size_t cap, const char *message) {
    snprintf(error, cap, "%s", message);
    return false;
}
// Validates bounded text fields, including embedded NUL.
static bool profile_string(struct json_object *v, const char *key, size_t cap, bool required) {
    struct json_object *x = NULL;
    if (!json_object_object_get_ex(v, key, &x))
        return !required;
    return json_object_is_type(x, json_type_string) && (size_t)json_object_get_string_len(x) < cap &&
           strlen(json_object_get_string(x)) == (size_t)json_object_get_string_len(x) &&
           (!required || json_object_get_string_len(x) > 0);
}
// Recognizes only supported service kinds in an explicit replacement list.
static bool profile_kinds(struct json_object *kinds) {
    if (!json_object_is_type(kinds, json_type_array) || json_object_array_length(kinds) > 4)
        return false;
    const char *valid[] = {"dog_walking", "pet_sitting", "drop_in", "custom"};
    for (size_t i = 0; i < json_object_array_length(kinds); ++i) {
        struct json_object *v = json_object_array_get_idx(kinds, i);
        if (!json_object_is_type(v, json_type_string))
            return false;
        const char *s = json_object_get_string(v);
        bool found = false;
        if (strlen(s) != (size_t)json_object_get_string_len(v))
            return false;
        for (size_t j = 0; j < 4; ++j)
            if (!strcmp(s, valid[j]))
                found = true;
        if (!found)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(s, json_object_get_string(json_object_array_get_idx(kinds, j))))
                return false;
    }
    return true;
}
// Reuses matching service identities and metadata while replacing only this home's roles.
static void profile_services(struct json_object *out, struct json_object *old, const char *place,
                             const char *pet, struct json_object *kinds, const char *action, unsigned ordinal,
                             MapForgePin *pin) {
    for (size_t i = 0; i < json_object_array_length(kinds); ++i) {
        const char *kind = json_object_get_string(json_object_array_get_idx(kinds, i));
        struct json_object *s = NULL;
        for (size_t j = 0; old && j < json_object_array_length(old); ++j) {
            struct json_object *v = json_object_array_get_idx(old, j);
            if (!strcmp(profile_text(v, "place_id"), place) && !strcmp(profile_text(v, "pet_id"), pet) &&
                !strcmp(profile_text(v, "kind"), kind)) {
                s = profile_clone(v);
                break;
            }
        }
        if (!s) {
            s = json_object_new_object();
            char id[256];
            snprintf(id, sizeof(id), "profile-%s-%u-%zu", action, ordinal, i);
            json_object_object_add(s, "id", json_object_new_string(id));
            json_object_object_add(s, "place_id", json_object_new_string(place));
            json_object_object_add(s, "pet_id", json_object_new_string(pet));
            json_object_object_add(s, "kind", json_object_new_string(kind));
        }
        json_object_array_add(out, s);
        if (!strcmp(kind, "dog_walking"))
            pin->collection_mask |= 2u;
        if (!strcmp(kind, "pet_sitting") || !strcmp(kind, "drop_in"))
            pin->collection_mask |= 4u;
    }
}
// Replaces this home's associations/roles while preserving global pet identities and other homes.
bool map_forge_place_profile_apply(MapForgePinsFile *file, MapForgePin *pin, struct json_object *profile,
                                   struct json_object *request, char *error, size_t capacity) {
    struct json_object *input = NULL, *home = NULL, *pets = NULL, *links = NULL, *services = NULL;
    if (!json_object_is_type(profile, json_type_object) ||
        !json_object_object_get_ex(profile, "pets", &input) || !json_object_is_type(input, json_type_array) ||
        json_object_array_length(input) > 32u ||
        !json_object_object_get_ex(profile, "home_services", &home) || !profile_kinds(home))
        return profile_error(error, capacity, "Profile requires up to 32 pets and typed home services");
    json_object_object_get_ex(file->source_json, "pets", &pets);
    json_object_object_get_ex(file->source_json, "pet_places", &links);
    json_object_object_get_ex(file->source_json, "services", &services);
    for (size_t i = 0; i < json_object_array_length(input); ++i) {
        struct json_object *v = json_object_array_get_idx(input, i), *kinds = NULL;
        if (!json_object_is_type(v, json_type_object) || !profile_string(v, "id", 128, true) ||
            !profile_string(v, "name", 128, true) || !profile_string(v, "species", 64, false) ||
            !json_object_object_get_ex(v, "services", &kinds) || !profile_kinds(kinds))
            return profile_error(error, capacity, "Invalid pet identity, name, species or services");
        const char *id = profile_text(v, "id");
        for (size_t j = 0; j < i; ++j)
            if (!strcmp(id, profile_text(json_object_array_get_idx(input, j), "id")))
                return profile_error(error, capacity, "Duplicate pet identity in profile");
        if (!profile_find(pets, id) && strncmp(id, "native-pet-", 11))
            return profile_error(error, capacity,
                                 "Unknown pet identity; new pets require native-pet- identity");
    }
    struct json_object *history = NULL;
    if (json_object_object_get_ex(file->source_json, "place_history", &history) &&
        json_object_array_length(history)) {
        struct json_object *event = json_object_array_get_idx(history, json_object_array_length(history) - 1);
        struct json_object *prior_pets = json_object_new_array(), *prior_links = json_object_new_array(),
                           *prior_services = json_object_new_array();
        for (size_t i = 0; links && i < json_object_array_length(links); ++i) {
            struct json_object *v = json_object_array_get_idx(links, i);
            if (!strcmp(profile_text(v, "place_id"), pin->id)) {
                json_object_array_add(prior_links, profile_clone(v));
                struct json_object *pet = profile_find(pets, profile_text(v, "pet_id"));
                if (pet)
                    json_object_array_add(prior_pets, profile_clone(pet));
            }
        }
        for (size_t i = 0; services && i < json_object_array_length(services); ++i) {
            struct json_object *v = json_object_array_get_idx(services, i);
            if (!strcmp(profile_text(v, "place_id"), pin->id))
                json_object_array_add(prior_services, profile_clone(v));
        }
        json_object_object_add(event, "previous_pets", prior_pets);
        json_object_object_add(event, "previous_links", prior_links);
        json_object_object_add(event, "previous_services", prior_services);
    }
    struct json_object *next_links = json_object_new_array(), *next_services = json_object_new_array();
    for (size_t i = 0; links && i < json_object_array_length(links); ++i) {
        struct json_object *v = json_object_array_get_idx(links, i);
        if (strcmp(profile_text(v, "place_id"), pin->id))
            json_object_array_add(next_links, profile_clone(v));
    }
    for (size_t i = 0; services && i < json_object_array_length(services); ++i) {
        struct json_object *v = json_object_array_get_idx(services, i);
        if (strcmp(profile_text(v, "place_id"), pin->id))
            json_object_array_add(next_services, profile_clone(v));
    }
    pin->collection_mask &= ~6u;
    const char *action = profile_text(request, "request_id");
    profile_services(next_services, services, pin->id, "", home, action, 0, pin);
    for (size_t i = 0; i < json_object_array_length(input); ++i) {
        struct json_object *v = json_object_array_get_idx(input, i), *kinds = NULL;
        const char *id = profile_text(v, "id");
        struct json_object *pet = profile_find(pets, id);
        if (!pet) {
            pet = json_object_new_object();
            json_object_object_add(pet, "id", json_object_new_string(id));
            json_object_array_add(pets, pet);
        }
        json_object_object_add(pet, "name", json_object_new_string(profile_text(v, "name")));
        json_object_object_add(pet, "species", json_object_new_string(profile_text(v, "species")));
        struct json_object *link = json_object_new_object();
        json_object_object_add(link, "pet_id", json_object_new_string(id));
        json_object_object_add(link, "place_id", json_object_new_string(pin->id));
        json_object_object_add(link, "role", json_object_new_string("home"));
        json_object_array_add(next_links, link);
        json_object_object_get_ex(v, "services", &kinds);
        profile_services(next_services, services, pin->id, id, kinds, action, (unsigned)i + 1, pin);
    }
    json_object_object_add(file->source_json, "pet_places", next_links);
    json_object_object_add(file->source_json, "services", next_services);
    return true;
}
