#include "core_config.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static void fill_string(char *dst, size_t len, char ch) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = ch;
    }
    dst[len] = '\0';
}

int main(void) {
    CoreConfigEntry backing[4];
    CoreConfigTable table;
    CoreConfigValue value;
    char max_key[CORE_CONFIG_MAX_KEY_LENGTH + 1];
    char too_long_key[CORE_CONFIG_MAX_KEY_LENGTH + 2];
    char max_string[CORE_CONFIG_MAX_STRING_LENGTH + 1];
    char too_long_string[CORE_CONFIG_MAX_STRING_LENGTH + 2];

    fill_string(max_key, CORE_CONFIG_MAX_KEY_LENGTH, 'k');
    fill_string(too_long_key, CORE_CONFIG_MAX_KEY_LENGTH + 1, 'x');
    fill_string(max_string, CORE_CONFIG_MAX_STRING_LENGTH, 's');
    fill_string(too_long_string, CORE_CONFIG_MAX_STRING_LENGTH + 1, 'z');

    value.type = CORE_CONFIG_TYPE_STRING;
    strcpy(value.data.as_string, "unchanged");

    assert(!core_config_table_init(NULL, backing, 4u));
    assert(!core_config_table_init(&table, NULL, 4u));
    assert(!core_config_table_init(&table, backing, 0u));

    assert(core_config_table_init(&table, backing, 4u));
    assert(table.entries == backing);
    assert(table.capacity == 4u);
    assert(table.count == 0u);

    assert(!core_config_set_bool(NULL, "feature.authoring", true));
    assert(!core_config_set_bool(&table, NULL, true));
    assert(!core_config_set_int(&table, "", 1));
    assert(!core_config_set_string(&table, too_long_key, "value"));
    assert(!core_config_set_double(&table, "nan.value", NAN));
    assert(!core_config_set_double(&table, "inf.value", INFINITY));
    assert(!core_config_set_string(&table, "null.string", NULL));
    assert(!core_config_set_string(&table, "too_long_string", too_long_string));

    assert(core_config_set_bool(&table, "feature.authoring", true));
    assert(core_config_set_int(&table, "pane.min_count", 3));
    assert(core_config_set_double(&table, "split.default_ratio", 0.5));
    assert(core_config_set_string(&table, "workspace.default", "ide_like"));
    assert(table.count == 4u);

    assert(core_config_get(&table, "feature.authoring", &value));
    assert(value.type == CORE_CONFIG_TYPE_BOOL);
    assert(value.data.as_bool);

    assert(core_config_get(&table, "pane.min_count", &value));
    assert(value.type == CORE_CONFIG_TYPE_INT);
    assert(value.data.as_int == 3);

    assert(core_config_get(&table, "split.default_ratio", &value));
    assert(value.type == CORE_CONFIG_TYPE_DOUBLE);
    assert(value.data.as_double == 0.5);

    assert(core_config_get(&table, "workspace.default", &value));
    assert(value.type == CORE_CONFIG_TYPE_STRING);
    assert(strcmp(value.data.as_string, "ide_like") == 0);

    assert(core_config_set_int(&table, "pane.min_count", 4));
    assert(table.count == 4u);
    assert(core_config_get(&table, "pane.min_count", &value));
    assert(value.type == CORE_CONFIG_TYPE_INT);
    assert(value.data.as_int == 4);

    assert(core_config_set_string(&table, "pane.min_count", "retyped"));
    assert(table.count == 4u);
    assert(core_config_get(&table, "pane.min_count", &value));
    assert(value.type == CORE_CONFIG_TYPE_STRING);
    assert(strcmp(value.data.as_string, "retyped") == 0);

    assert(core_config_table_init(&table, backing, 4u));
    assert(core_config_set_string(&table, max_key, max_string));
    assert(core_config_get(&table, max_key, &value));
    assert(value.type == CORE_CONFIG_TYPE_STRING);
    assert(strcmp(value.data.as_string, max_string) == 0);

    assert(core_config_set_string(&table, "empty.string", ""));
    assert(core_config_get(&table, "empty.string", &value));
    assert(value.type == CORE_CONFIG_TYPE_STRING);
    assert(strcmp(value.data.as_string, "") == 0);

    assert(core_config_set_bool(&table, "slot.two", false));
    assert(core_config_set_bool(&table, "slot.three", true));
    assert(!core_config_set_bool(&table, "overflow.slot", false));
    assert(table.count == 4u);

    value.type = CORE_CONFIG_TYPE_STRING;
    strcpy(value.data.as_string, "sentinel");
    assert(!core_config_get(&table, "missing", &value));
    assert(value.type == 0);
    assert(value.data.as_string[0] == '\0');

    value.type = CORE_CONFIG_TYPE_STRING;
    strcpy(value.data.as_string, "sentinel");
    assert(!core_config_get(&table, "", &value));
    assert(value.type == 0);
    assert(value.data.as_string[0] == '\0');

    value.type = CORE_CONFIG_TYPE_STRING;
    strcpy(value.data.as_string, "sentinel");
    assert(!core_config_get(NULL, "missing", &value));
    assert(value.type == 0);
    assert(value.data.as_string[0] == '\0');

    assert(!core_config_get(&table, "missing", NULL));
    assert(core_config_get(NULL, "missing", NULL) == false);

    {
        CoreConfigTable invalid_table = {0};
        CoreConfigValue invalid_value;
        invalid_table.entries = NULL;
        invalid_table.capacity = 2u;
        invalid_table.count = 0u;
        invalid_value.type = CORE_CONFIG_TYPE_INT;
        invalid_value.data.as_int = 99;
        assert(!core_config_set_bool(&invalid_table, "bad", true));
        assert(!core_config_get(&invalid_table, "bad", &invalid_value));
        assert(invalid_value.type == 0);
    }

    return 0;
}
