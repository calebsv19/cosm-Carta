#include "core_config.h"

#include <assert.h>
#include <string.h>

int main(void) {
    CoreConfigEntry backing[4];
    CoreConfigTable table;
    CoreConfigValue value;

    assert(core_config_table_init(&table, backing, 4u));
    assert(table.count == 0u);

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
    assert(value.data.as_int == 4);

    assert(!core_config_set_int(&table, "", 1));
    assert(!core_config_set_string(&table, "too_long", "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnop"));
    assert(!core_config_set_bool(&table, "overflow.slot", false));
    assert(!core_config_get(&table, "missing", &value));

    return 0;
}
