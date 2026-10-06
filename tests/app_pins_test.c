#include "app/app_pins.h"
#include "app/place_repository.h"
#include <sqlite3.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>
#include <sys/stat.h>

static void test_load_and_lookup(void) {
    MapForgePinsFile pins;
    char error[256];
    map_forge_pins_file_init(&pins);
    assert(map_forge_pins_load("data/pins/examples/demo.seattle.pins.json", &pins, error, sizeof(error)));
    assert(pins.pin_count >= 2u);
    assert(strcmp(pins.map_region, "seattle") == 0);
    assert(map_forge_pins_find_by_id_const(&pins, "demo_start") != NULL);
    assert(map_forge_pins_find_by_name_const(&pins, "Demo Goal") != NULL);
    map_forge_pins_file_free(&pins);
}

static void test_upsert_save_and_remove(void) {
    MapForgePinsFile pins;
    MapForgePinsFile reloaded;
    MapForgePin pin;
    RegionInfo region = {0};
    char save_path[MAPFORGE_PIN_PATH_CAPACITY];
    char default_path[MAPFORGE_PIN_PATH_CAPACITY];
    char error[256];

    map_forge_pins_file_init(&pins);
    map_forge_pins_file_init(&reloaded);
    snprintf(pins.map_region, sizeof(pins.map_region), "seattle");

    memset(&pin, 0, sizeof(pin));
    snprintf(pin.id, sizeof(pin.id), "test_pin");
    snprintf(pin.name, sizeof(pin.name), "Test Pin");
    snprintf(pin.type, sizeof(pin.type), "general");
    snprintf(pin.color, sizeof(pin.color), "green");
    pin.lat = 47.6205;
    pin.lon = -122.3493;
    pin.private_flag = true;

    assert(map_forge_pins_upsert(&pins, &pin, error, sizeof(error)));
    assert(pins.pin_count == 1u);

    snprintf(save_path, sizeof(save_path), "/private/tmp/map_forge_pins_test_%d.json", (int)getpid());
    assert(map_forge_pins_save(save_path, &pins, error, sizeof(error)));
    assert(map_forge_pins_load(save_path, &reloaded, error, sizeof(error)));
    assert(reloaded.pin_count == 1u);
    assert(map_forge_pins_find_by_id_const(&reloaded, "test_pin") != NULL);
    assert(map_forge_pins_remove_by_id(&reloaded, "test_pin"));
    assert(reloaded.pin_count == 0u);

    region.name = "seattle";
    assert(map_forge_pins_default_private_path(&region, default_path, sizeof(default_path)));
    assert(strcmp(default_path, "data/pins/private/seattle.pins.local.json") == 0);

    (void)remove(save_path);
    map_forge_pins_file_free(&pins);
    map_forge_pins_file_free(&reloaded);
}

static void test_move_reorders_pins(void) {
    MapForgePinsFile pins;
    MapForgePin a;
    MapForgePin b;
    MapForgePin c;
    char error[256];

    map_forge_pins_file_init(&pins);
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c));
    snprintf(a.id, sizeof(a.id), "a");
    snprintf(a.name, sizeof(a.name), "A");
    snprintf(b.id, sizeof(b.id), "b");
    snprintf(b.name, sizeof(b.name), "B");
    snprintf(c.id, sizeof(c.id), "c");
    snprintf(c.name, sizeof(c.name), "C");

    assert(map_forge_pins_upsert(&pins, &a, error, sizeof(error)));
    assert(map_forge_pins_upsert(&pins, &b, error, sizeof(error)));
    assert(map_forge_pins_upsert(&pins, &c, error, sizeof(error)));
    assert(map_forge_pins_move(&pins, 0u, 2u));
    assert(strcmp(pins.pins[0].id, "b") == 0);
    assert(strcmp(pins.pins[1].id, "c") == 0);
    assert(strcmp(pins.pins[2].id, "a") == 0);
    assert(map_forge_pins_move(&pins, 2u, 0u));
    assert(strcmp(pins.pins[0].id, "a") == 0);
    assert(strcmp(pins.pins[1].id, "b") == 0);
    assert(strcmp(pins.pins[2].id, "c") == 0);
    map_forge_pins_file_free(&pins);
}

static void test_runtime_dir_default_path(void) {
    RegionInfo region = {0};
    char default_path[MAPFORGE_PIN_PATH_CAPACITY];
    region.name = "seattle";
    assert(setenv("MAPFORGE_RUNTIME_DIR", "/tmp/mapforge-runtime", 1) == 0);
    assert(map_forge_pins_default_private_path(&region, default_path, sizeof(default_path)));
    assert(strcmp(default_path, "/tmp/mapforge-runtime/pins/seattle.pins.local.json") == 0);
    assert(unsetenv("MAPFORGE_RUNTIME_DIR") == 0);
}

static void test_load_preferred_region_file_legacy_fallback(void) {
    RegionInfo region = {0};
    MapForgePinsFile pins = {0};
    MapForgePinsFile seed = {0};
    MapForgePin pin = {0};
    char resolved_path[MAPFORGE_PIN_PATH_CAPACITY];
    char runtime_dir[MAPFORGE_PIN_PATH_CAPACITY];
    char legacy_path[MAPFORGE_PIN_PATH_CAPACITY];
    char error[256];
    bool loaded_from_file = false;
    bool loaded_from_legacy = false;

    region.name = "seattle";
    map_forge_pins_file_init(&pins);
    map_forge_pins_file_init(&seed);
    snprintf(runtime_dir, sizeof(runtime_dir), "/private/tmp/map_forge_runtime_%d", (int)getpid());
    assert(setenv("MAPFORGE_RUNTIME_DIR", runtime_dir, 1) == 0);
    assert(map_forge_pins_legacy_private_path(&region, legacy_path, sizeof(legacy_path)));
    snprintf(seed.map_region, sizeof(seed.map_region), "seattle");
    snprintf(pin.id, sizeof(pin.id), "legacy_seed");
    snprintf(pin.name, sizeof(pin.name), "Legacy Seed");
    pin.lat = 47.6205;
    pin.lon = -122.3493;
    assert(map_forge_pins_upsert(&seed, &pin, error, sizeof(error)));
    assert(map_forge_pins_save(legacy_path, &seed, error, sizeof(error)));

    assert(map_forge_pins_load_preferred_region_file(&region,
                                                     &pins,
                                                     resolved_path,
                                                     sizeof(resolved_path),
                                                     &loaded_from_file,
                                                     &loaded_from_legacy,
                                                     error,
                                                     sizeof(error)));
    assert(loaded_from_file);
    assert(loaded_from_legacy);
    assert(strcmp(resolved_path, legacy_path) == 0);
    assert(pins.pin_count == 1u);
    assert(map_forge_pins_find_by_id_const(&pins, "legacy_seed") != NULL);

    (void)remove(legacy_path);
    (void)unsetenv("MAPFORGE_RUNTIME_DIR");
    map_forge_pins_file_free(&pins);
    map_forge_pins_file_free(&seed);
}

// Verifies migration, extension preservation, recovery and canceled writes against real files.
static void test_durable_workspace(void) {
    char cwd[4096]; assert(getcwd(cwd, sizeof(cwd)));
    char temp[] = "/private/tmp/carta-places-XXXXXX"; assert(mkdtemp(temp)); assert(chdir(temp) == 0);
    assert(setenv("MAPFORGE_RUNTIME_DIR", temp, 1) == 0);
    MapForgePinsFile pins, recovered; map_forge_pins_file_init(&pins); map_forge_pins_file_init(&recovered);
    MapForgePin pin = {0}; strcpy(pin.id, "stable_personal_id"); strcpy(pin.name, "A place");
    pin.lat = 47.66; pin.lon = -122.35; pin.collection_mask = 2u | 8u;
    char error[256], legacy[512], global[512]; RegionInfo region = {.name = "obsolete_region"};
    assert(map_forge_pins_default_private_path(&region, legacy, sizeof(legacy)));
    assert(map_forge_pins_upsert(&pins, &pin, error, sizeof(error)));
    assert(map_forge_pins_save(legacy, &pins, error, sizeof(error)));
    map_forge_pins_file_free(&pins);
    assert(map_forge_pins_load_workspace(&pins, global, sizeof(global), error, sizeof(error)));
    assert(pins.pin_count == 1 && strcmp(pins.pins[0].id, "stable_personal_id") == 0 && pins.map_region[0] == 0);
    assert(pins.pins[0].collection_mask == (2u | 8u)); assert(access(legacy, F_OK) == 0);
    struct json_object *extension = json_object_new_object(); json_object_object_add(extension, "visits", json_object_new_int(3));
    struct json_object *records = NULL; json_object_object_get_ex(pins.source_json, "pins", &records);
    json_object_object_add(json_object_array_get_idx(records, 0), "future_metadata", extension);
    assert(map_forge_pins_save(global, &pins, error, sizeof(error)));
    strcpy(pins.pins[0].name, "Renamed place"); assert(map_forge_pins_save(global, &pins, error, sizeof(error)));
    /* Native edits commit histories, and refused writes leave both caller and SQLite unchanged. */
    MapForgePin edit = pins.pins[0]; strcpy(edit.name, "Native rename");
    assert(map_forge_places_commit_pin(global, &pins, &edit, false, error, sizeof(error)));
    struct json_object *history = NULL;
    assert(json_object_object_get_ex(pins.source_json, "place_history", &history) && json_object_array_length(history) == 1u);
    assert(map_forge_pins_load(global, &recovered, error, sizeof(error)));
    edit.lat += 0.0001;
    assert(map_forge_places_commit_pin(global, &pins, &edit, false, error, sizeof(error)));
    assert(!strcmp(pins.pins[0].location_quality, "manual"));
    assert(!map_forge_places_commit_pin(global, &recovered, &edit, false, error, sizeof(error)));
    assert(recovered.pins[0].lat != edit.lat && !strcmp(recovered.pins[0].name, "Native rename"));
    sqlite3 *db = NULL; assert(sqlite3_open(global, &db) == SQLITE_OK);
    assert(sqlite3_exec(db, "CREATE TRIGGER refuse_native BEFORE INSERT ON document BEGIN SELECT RAISE(ABORT,'synthetic native failure'); END", NULL, NULL, NULL) == SQLITE_OK);
    strcpy(edit.name, "Must not appear");
    assert(!map_forge_places_commit_pin(global, &pins, &edit, false, error, sizeof(error)));
    assert(!strcmp(pins.pins[0].name, "Native rename"));
    assert(sqlite3_exec(db, "DROP TRIGGER refuse_native", NULL, NULL, NULL) == SQLITE_OK); sqlite3_close(db);
    json_object_object_add(pins.source_json, "services", json_tokener_parse("[{\"id\":\"s1\",\"place_id\":\"stable_personal_id\",\"kind\":\"drop_in\"}]"));
    assert(map_forge_pins_save(global, &pins, error, sizeof(error)));
    assert(!map_forge_places_commit_pin(global, &pins, &edit, true, error, sizeof(error)));
    assert(pins.pin_count == 1u && !strcmp(pins.pins[0].name, "Native rename"));
    json_object_object_del(pins.source_json, "services"); assert(map_forge_pins_save(global, &pins, error, sizeof(error)));
    strcpy(edit.name, "Renamed place"); assert(map_forge_places_commit_pin(global, &pins, &edit, false, error, sizeof(error)));
    /* Preserve legacy JSON recovery coverage independently of the SQLite owner. */
    assert(map_forge_pins_load(global, &recovered, error, sizeof(error)));
    assert(strcmp(recovered.pins[0].name, "Renamed place") == 0);
    snprintf(global, sizeof(global), "%s/pins/recovery.json", temp);
    pins.source_signature = 0u;
    assert(map_forge_pins_save(global, &pins, error, sizeof(error)));
    assert(map_forge_pins_save(global, &pins, error, sizeof(error)));
    /* A blocked write leaves the previous primary bytes intact. */
    FILE *f = fopen(global, "rb"); assert(f); char before[4096]; size_t size = fread(before, 1, sizeof(before), f); fclose(f);
    char dir[512]; snprintf(dir, sizeof(dir), "%s/pins", temp); assert(chmod(dir, 0500) == 0);
    strcpy(pins.pins[0].name, "Must not persist"); assert(!map_forge_pins_save(global, &pins, error, sizeof(error)));
    assert(chmod(dir, 0700) == 0); f = fopen(global, "rb"); assert(f); char after[4096]; size_t count = fread(after, 1, sizeof(after), f); fclose(f);
    assert(size == count && memcmp(before, after, size) == 0);
    f = fopen(global, "wb"); assert(f); fputs("{broken", f); fclose(f);
    assert(map_forge_pins_load(global, &recovered, error, sizeof(error)) && recovered.recovered_from_backup);
    assert(strcmp(recovered.pins[0].id, "stable_personal_id") == 0);
    assert(map_forge_pins_save(global, &recovered, error, sizeof(error)));
    struct json_object *json = json_object_from_file(global), *array = NULL, *value = NULL;
    assert(json_object_object_get_ex(json, "pins", &array));
    assert(json_object_object_get_ex(json_object_array_get_idx(array, 0), "future_metadata", &value)); json_object_put(json);
    json = json_object_from_file(global); json_object_object_add(json, "operator_note", json_object_new_string("external edit"));
    assert(json_object_to_file(global, json) == 0); json_object_put(json);
    assert(!map_forge_pins_save(global, &recovered, error, sizeof(error)));
    assert(map_forge_pins_load(global, &recovered, error, sizeof(error)));
    json = json_object_from_file(global); json_object_object_add(json, "version", json_object_new_int(3));
    assert(json_object_to_file(global, json) == 0); json_object_put(json);
    assert(!map_forge_pins_load(global, &recovered, error, sizeof(error)));
    /* Invalid coordinates, duplicate IDs and oversized text cannot replace loaded records. */
    char bad[] = "invalid.json"; f = fopen(bad, "wb"); assert(f);
    fputs("{\"version\":1,\"pins\":[{\"id\":\"bad\",\"name\":\"Bad\",\"lat\":999,\"lon\":0}]}", f); fclose(f);
    assert(!map_forge_pins_load(bad, &recovered, error, sizeof(error))); assert(recovered.pin_count == 1);
    f = fopen(bad, "wb"); assert(f); fputs("{\"version\":1,\"pins\":[{\"id\":\"x\",\"name\":\"X\",\"lat\":0,\"lon\":0},{\"id\":\"x\",\"name\":\"Y\",\"lat\":0,\"lon\":0}]}", f); fclose(f);
    assert(!map_forge_pins_load(bad, &recovered, error, sizeof(error)));
    f = fopen(bad, "wb"); assert(f); fputs("{\"version\":1,\"pins\":[{\"id\":\"x\",\"name\":\"X\",\"lat\":0,\"lon\":0,\"notes\":\"", f);
    for (int i = 0; i < 300; ++i) fputc('a', f); fputs("\"}]}", f); fclose(f);
    assert(!map_forge_pins_load(bad, &recovered, error, sizeof(error)));
    map_forge_pins_file_free(&pins); map_forge_pins_file_free(&recovered);
    unlink(bad); unlink(legacy); unlink(global); char backup[600]; snprintf(backup, sizeof(backup), "%s.bak", global); unlink(backup);
    assert(chdir(cwd) == 0); unsetenv("MAPFORGE_RUNTIME_DIR"); rmdir(dir); rmdir(temp);
}

int main(void) {
    test_durable_workspace();
    test_load_and_lookup();
    test_upsert_save_and_remove();
    test_move_reorders_pins();
    test_runtime_dir_default_path();
    test_load_preferred_region_file_legacy_fallback();
    printf("app_pins_test: success\n");
    return 0;
}
