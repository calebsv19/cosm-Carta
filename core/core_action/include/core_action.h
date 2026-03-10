#ifndef CORE_ACTION_H
#define CORE_ACTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORE_ACTION_MAX_ID_LENGTH 47
#define CORE_ACTION_MAX_LABEL_LENGTH 63
#define CORE_ACTION_MAX_TRIGGER_LENGTH 31

typedef struct CoreActionDef {
    char action_id[CORE_ACTION_MAX_ID_LENGTH + 1];
    char label[CORE_ACTION_MAX_LABEL_LENGTH + 1];
    bool active;
} CoreActionDef;

typedef struct CoreActionBinding {
    char trigger[CORE_ACTION_MAX_TRIGGER_LENGTH + 1];
    uint32_t action_index;
    bool active;
} CoreActionBinding;

typedef struct CoreActionRegistry {
    CoreActionDef *actions;
    size_t action_capacity;
    size_t action_count;
    CoreActionBinding *bindings;
    size_t binding_capacity;
    size_t binding_count;
} CoreActionRegistry;

bool core_action_registry_init(CoreActionRegistry *registry,
                               CoreActionDef *action_backing,
                               size_t action_capacity,
                               CoreActionBinding *binding_backing,
                               size_t binding_capacity);
bool core_action_register(CoreActionRegistry *registry, const char *action_id, const char *label);
bool core_action_bind_trigger(CoreActionRegistry *registry, const char *trigger, const char *action_id);
bool core_action_resolve_trigger(const CoreActionRegistry *registry, const char *trigger, const char **out_action_id);

#endif
