#include "core_action.h"

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

static int copy_bounded(char *dst, size_t dst_cap, const char *src) {
    size_t len;

    if (!dst || !src || dst_cap == 0u) {
        return 0;
    }

    len = bounded_strlen(src, dst_cap);
    if (len == 0u || len >= dst_cap) {
        return 0;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
    return 1;
}

static int find_action_index(const CoreActionRegistry *registry, const char *action_id, uint32_t *out_index) {
    size_t i;

    if (!registry || !action_id) {
        return 0;
    }

    for (i = 0u; i < registry->action_capacity; ++i) {
        if (registry->actions[i].active &&
            strncmp(registry->actions[i].action_id, action_id, CORE_ACTION_MAX_ID_LENGTH + 1u) == 0) {
            if (out_index) {
                *out_index = (uint32_t)i;
            }
            return 1;
        }
    }
    return 0;
}

static CoreActionBinding *find_binding(CoreActionRegistry *registry, const char *trigger) {
    size_t i;

    if (!registry || !trigger) {
        return 0;
    }

    for (i = 0u; i < registry->binding_capacity; ++i) {
        if (registry->bindings[i].active &&
            strncmp(registry->bindings[i].trigger, trigger, CORE_ACTION_MAX_TRIGGER_LENGTH + 1u) == 0) {
            return &registry->bindings[i];
        }
    }
    return 0;
}

static CoreActionBinding *find_free_binding(CoreActionRegistry *registry) {
    size_t i;

    if (!registry) {
        return 0;
    }

    for (i = 0u; i < registry->binding_capacity; ++i) {
        if (!registry->bindings[i].active) {
            return &registry->bindings[i];
        }
    }
    return 0;
}

bool core_action_registry_init(CoreActionRegistry *registry,
                               CoreActionDef *action_backing,
                               size_t action_capacity,
                               CoreActionBinding *binding_backing,
                               size_t binding_capacity) {
    if (!registry || !action_backing || action_capacity == 0u || !binding_backing || binding_capacity == 0u) {
        return false;
    }

    memset(action_backing, 0, sizeof(*action_backing) * action_capacity);
    memset(binding_backing, 0, sizeof(*binding_backing) * binding_capacity);

    registry->actions = action_backing;
    registry->action_capacity = action_capacity;
    registry->action_count = 0u;
    registry->bindings = binding_backing;
    registry->binding_capacity = binding_capacity;
    registry->binding_count = 0u;
    return true;
}

bool core_action_register(CoreActionRegistry *registry, const char *action_id, const char *label) {
    size_t i;

    if (!registry || !action_id || !label) {
        return false;
    }

    if (find_action_index(registry, action_id, 0)) {
        return true;
    }

    for (i = 0u; i < registry->action_capacity; ++i) {
        if (!registry->actions[i].active) {
            if (!copy_bounded(registry->actions[i].action_id,
                              sizeof(registry->actions[i].action_id),
                              action_id) ||
                !copy_bounded(registry->actions[i].label,
                              sizeof(registry->actions[i].label),
                              label)) {
                return false;
            }
            registry->actions[i].active = true;
            registry->action_count += 1u;
            return true;
        }
    }

    return false;
}

bool core_action_bind_trigger(CoreActionRegistry *registry, const char *trigger, const char *action_id) {
    CoreActionBinding *binding;
    uint32_t action_index = 0u;

    if (!registry || !trigger || !action_id) {
        return false;
    }

    if (!find_action_index(registry, action_id, &action_index)) {
        return false;
    }

    binding = find_binding(registry, trigger);
    if (!binding) {
        binding = find_free_binding(registry);
        if (!binding) {
            return false;
        }
        if (!copy_bounded(binding->trigger, sizeof(binding->trigger), trigger)) {
            return false;
        }
        binding->active = true;
        registry->binding_count += 1u;
    }

    binding->action_index = action_index;
    return true;
}

bool core_action_resolve_trigger(const CoreActionRegistry *registry, const char *trigger, const char **out_action_id) {
    size_t i;

    if (!registry || !trigger || !out_action_id) {
        return false;
    }

    for (i = 0u; i < registry->binding_capacity; ++i) {
        const CoreActionBinding *binding = &registry->bindings[i];

        if (binding->active &&
            strncmp(binding->trigger, trigger, CORE_ACTION_MAX_TRIGGER_LENGTH + 1u) == 0 &&
            binding->action_index < registry->action_capacity &&
            registry->actions[binding->action_index].active) {
            *out_action_id = registry->actions[binding->action_index].action_id;
            return true;
        }
    }

    return false;
}
