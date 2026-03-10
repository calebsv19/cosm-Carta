#include "core_config.h"

#include <string.h>

static size_t bounded_strlen(const char *s, size_t max_len) {
    size_t i = 0u;

    if (!s) {
        return 0u;
    }
    while (i < max_len && s[i] != '\0') {
        i += 1u;
    }
    return i;
}

static int key_is_valid(const char *key) {
    size_t len;

    if (!key) {
        return 0;
    }
    len = bounded_strlen(key, CORE_CONFIG_MAX_KEY_LENGTH + 1u);
    return len > 0u && len <= CORE_CONFIG_MAX_KEY_LENGTH;
}

static CoreConfigEntry *find_entry(CoreConfigTable *table, const char *key) {
    size_t i;

    if (!table || !key) {
        return 0;
    }
    for (i = 0u; i < table->capacity; ++i) {
        if (table->entries[i].active && strncmp(table->entries[i].key, key, CORE_CONFIG_MAX_KEY_LENGTH + 1u) == 0) {
            return &table->entries[i];
        }
    }
    return 0;
}

static CoreConfigEntry *find_free_entry(CoreConfigTable *table) {
    size_t i;

    if (!table) {
        return 0;
    }
    for (i = 0u; i < table->capacity; ++i) {
        if (!table->entries[i].active) {
            return &table->entries[i];
        }
    }
    return 0;
}

static bool set_value(CoreConfigTable *table, const char *key, CoreConfigValue value) {
    CoreConfigEntry *entry;

    if (!table || !key_is_valid(key)) {
        return false;
    }

    entry = find_entry(table, key);
    if (!entry) {
        entry = find_free_entry(table);
        if (!entry) {
            return false;
        }
        memset(entry, 0, sizeof(*entry));
        memcpy(entry->key, key, bounded_strlen(key, CORE_CONFIG_MAX_KEY_LENGTH));
        entry->key[bounded_strlen(key, CORE_CONFIG_MAX_KEY_LENGTH)] = '\0';
        entry->active = true;
        table->count += 1u;
    }

    entry->value = value;
    return true;
}

bool core_config_table_init(CoreConfigTable *table, CoreConfigEntry *backing, size_t capacity) {
    if (!table || !backing || capacity == 0u) {
        return false;
    }

    memset(backing, 0, capacity * sizeof(*backing));
    table->entries = backing;
    table->capacity = capacity;
    table->count = 0u;
    return true;
}

bool core_config_set_bool(CoreConfigTable *table, const char *key, bool value) {
    CoreConfigValue v;

    v.type = CORE_CONFIG_TYPE_BOOL;
    v.data.as_bool = value;
    return set_value(table, key, v);
}

bool core_config_set_int(CoreConfigTable *table, const char *key, int64_t value) {
    CoreConfigValue v;

    v.type = CORE_CONFIG_TYPE_INT;
    v.data.as_int = value;
    return set_value(table, key, v);
}

bool core_config_set_double(CoreConfigTable *table, const char *key, double value) {
    CoreConfigValue v;

    v.type = CORE_CONFIG_TYPE_DOUBLE;
    v.data.as_double = value;
    return set_value(table, key, v);
}

bool core_config_set_string(CoreConfigTable *table, const char *key, const char *value) {
    CoreConfigValue v;
    size_t len;

    if (!value) {
        return false;
    }

    len = bounded_strlen(value, CORE_CONFIG_MAX_STRING_LENGTH + 1u);
    if (len > CORE_CONFIG_MAX_STRING_LENGTH) {
        return false;
    }

    v.type = CORE_CONFIG_TYPE_STRING;
    memset(v.data.as_string, 0, sizeof(v.data.as_string));
    memcpy(v.data.as_string, value, len);
    return set_value(table, key, v);
}

bool core_config_get(const CoreConfigTable *table, const char *key, CoreConfigValue *out_value) {
    size_t i;

    if (!table || !out_value || !key_is_valid(key)) {
        return false;
    }

    for (i = 0u; i < table->capacity; ++i) {
        if (table->entries[i].active && strncmp(table->entries[i].key, key, CORE_CONFIG_MAX_KEY_LENGTH + 1u) == 0) {
            *out_value = table->entries[i].value;
            return true;
        }
    }
    return false;
}
