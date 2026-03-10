#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORE_CONFIG_MAX_KEY_LENGTH 63
#define CORE_CONFIG_MAX_STRING_LENGTH 95

typedef enum CoreConfigValueType {
    CORE_CONFIG_TYPE_BOOL = 1,
    CORE_CONFIG_TYPE_INT = 2,
    CORE_CONFIG_TYPE_DOUBLE = 3,
    CORE_CONFIG_TYPE_STRING = 4
} CoreConfigValueType;

typedef struct CoreConfigValue {
    CoreConfigValueType type;
    union {
        bool as_bool;
        int64_t as_int;
        double as_double;
        char as_string[CORE_CONFIG_MAX_STRING_LENGTH + 1];
    } data;
} CoreConfigValue;

typedef struct CoreConfigEntry {
    char key[CORE_CONFIG_MAX_KEY_LENGTH + 1];
    CoreConfigValue value;
    bool active;
} CoreConfigEntry;

typedef struct CoreConfigTable {
    CoreConfigEntry *entries;
    size_t capacity;
    size_t count;
} CoreConfigTable;

bool core_config_table_init(CoreConfigTable *table, CoreConfigEntry *backing, size_t capacity);
bool core_config_set_bool(CoreConfigTable *table, const char *key, bool value);
bool core_config_set_int(CoreConfigTable *table, const char *key, int64_t value);
bool core_config_set_double(CoreConfigTable *table, const char *key, double value);
bool core_config_set_string(CoreConfigTable *table, const char *key, const char *value);
bool core_config_get(const CoreConfigTable *table, const char *key, CoreConfigValue *out_value);

#endif
