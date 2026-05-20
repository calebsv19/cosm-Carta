#include "app/app_pins.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

int main(void) {
    test_load_and_lookup();
    test_upsert_save_and_remove();
    test_move_reorders_pins();
    test_runtime_dir_default_path();
    printf("app_pins_test: success\n");
    return 0;
}
