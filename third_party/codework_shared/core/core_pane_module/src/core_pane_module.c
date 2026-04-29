#include "core_pane_module.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int key_is_valid(const char *key) {
    size_t i;
    if (!key || key[0] == '\0') {
        return 0;
    }
    for (i = 0u; key[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)key[i];
        if (c == '_') {
            continue;
        }
        if (!islower((int)c) && !isdigit((int)c)) {
            return 0;
        }
    }
    return 1;
}

static int pane_id_exists(const uint32_t *leaf_pane_ids,
                          uint32_t leaf_pane_count,
                          uint32_t pane_id) {
    uint32_t i;
    if (!leaf_pane_ids || leaf_pane_count == 0u || pane_id == 0u) {
        return 0;
    }
    for (i = 0u; i < leaf_pane_count; ++i) {
        if (leaf_pane_ids[i] == pane_id) {
            return 1;
        }
    }
    return 0;
}

static CorePaneModuleResult validate_descriptor(const CorePaneModuleDescriptor *descriptor) {
    if (!descriptor || descriptor->module_type_id == 0u ||
        !descriptor->module_key || !descriptor->display_name) {
        return CORE_PANE_MODULE_ERR_INVALID_DESCRIPTOR;
    }
    if (!key_is_valid(descriptor->module_key)) {
        return CORE_PANE_MODULE_ERR_INVALID_DESCRIPTOR;
    }
    if (descriptor->provider_kind != CORE_PANE_MODULE_PROVIDER_INTERNAL) {
        return CORE_PANE_MODULE_ERR_PROVIDER_NOT_ALLOWED;
    }
    if ((descriptor->capabilities & CORE_PANE_MODULE_CAP_RENDER) != 0u && descriptor->render == NULL) {
        return CORE_PANE_MODULE_ERR_CAPABILITY_HOOK_MISMATCH;
    }
    if ((descriptor->capabilities & CORE_PANE_MODULE_CAP_INPUT_KEYBOARD) != 0u &&
        descriptor->handle_keyboard == NULL) {
        return CORE_PANE_MODULE_ERR_CAPABILITY_HOOK_MISMATCH;
    }
    if ((descriptor->capabilities & CORE_PANE_MODULE_CAP_INPUT_POINTER) != 0u &&
        descriptor->handle_pointer == NULL) {
        return CORE_PANE_MODULE_ERR_CAPABILITY_HOOK_MISMATCH;
    }
    return CORE_PANE_MODULE_OK;
}

CorePaneModuleResult core_pane_module_registry_init(CorePaneModuleRegistry *registry,
                                                    CorePaneModuleDescriptor *entries,
                                                    uint32_t capacity) {
    if (!registry || !entries || capacity == 0u) {
        return CORE_PANE_MODULE_ERR_INVALID_ARG;
    }
    registry->entries = entries;
    registry->count = 0u;
    registry->capacity = capacity;
    return CORE_PANE_MODULE_OK;
}

CorePaneModuleResult core_pane_module_register(CorePaneModuleRegistry *registry,
                                               const CorePaneModuleDescriptor *descriptor) {
    uint32_t i;
    CorePaneModuleResult validation;

    if (!registry || !registry->entries || !descriptor) {
        return CORE_PANE_MODULE_ERR_INVALID_ARG;
    }
    validation = validate_descriptor(descriptor);
    if (validation != CORE_PANE_MODULE_OK) {
        return validation;
    }
    if (registry->count >= registry->capacity) {
        return CORE_PANE_MODULE_ERR_REGISTRY_FULL;
    }

    for (i = 0u; i < registry->count; ++i) {
        if (registry->entries[i].module_type_id == descriptor->module_type_id) {
            return CORE_PANE_MODULE_ERR_DUP_TYPE_ID;
        }
        if (strcmp(registry->entries[i].module_key, descriptor->module_key) == 0) {
            return CORE_PANE_MODULE_ERR_DUP_KEY;
        }
    }

    registry->entries[registry->count] = *descriptor;
    registry->count += 1u;
    return CORE_PANE_MODULE_OK;
}

CorePaneModuleResult core_pane_module_find_by_type_id(const CorePaneModuleRegistry *registry,
                                                      uint32_t module_type_id,
                                                      const CorePaneModuleDescriptor **out_descriptor) {
    uint32_t i;
    if (!registry || !out_descriptor || module_type_id == 0u) {
        return CORE_PANE_MODULE_ERR_INVALID_ARG;
    }
    for (i = 0u; i < registry->count; ++i) {
        if (registry->entries[i].module_type_id == module_type_id) {
            *out_descriptor = &registry->entries[i];
            return CORE_PANE_MODULE_OK;
        }
    }
    return CORE_PANE_MODULE_ERR_NOT_FOUND;
}

CorePaneModuleResult core_pane_module_find_by_key(const CorePaneModuleRegistry *registry,
                                                  const char *module_key,
                                                  const CorePaneModuleDescriptor **out_descriptor) {
    uint32_t i;
    if (!registry || !module_key || !out_descriptor) {
        return CORE_PANE_MODULE_ERR_INVALID_ARG;
    }
    for (i = 0u; i < registry->count; ++i) {
        if (strcmp(registry->entries[i].module_key, module_key) == 0) {
            *out_descriptor = &registry->entries[i];
            return CORE_PANE_MODULE_OK;
        }
    }
    return CORE_PANE_MODULE_ERR_NOT_FOUND;
}

CorePaneModuleResult core_pane_module_validate_bindings(const CorePaneModuleRegistry *registry,
                                                        const CorePaneModuleBinding *bindings,
                                                        uint32_t binding_count,
                                                        const uint32_t *leaf_pane_ids,
                                                        uint32_t leaf_pane_count) {
    uint32_t i;
    uint32_t j;
    if (!registry || !bindings || (binding_count > 0u && (!leaf_pane_ids || leaf_pane_count == 0u))) {
        return CORE_PANE_MODULE_ERR_INVALID_ARG;
    }

    for (i = 0u; i < binding_count; ++i) {
        const CorePaneModuleDescriptor *descriptor = NULL;
        const CorePaneModuleBinding *binding = &bindings[i];

        if (binding->instance_id == 0u || binding->pane_node_id == 0u || binding->module_type_id == 0u) {
            return CORE_PANE_MODULE_ERR_INVALID_ARG;
        }
        if (!pane_id_exists(leaf_pane_ids, leaf_pane_count, binding->pane_node_id)) {
            return CORE_PANE_MODULE_ERR_UNKNOWN_PANE_ID;
        }
        if (core_pane_module_find_by_type_id(registry,
                                             binding->module_type_id,
                                             &descriptor) != CORE_PANE_MODULE_OK) {
            return CORE_PANE_MODULE_ERR_UNKNOWN_MODULE_TYPE;
        }
        (void)descriptor;

        for (j = i + 1u; j < binding_count; ++j) {
            if (bindings[j].instance_id == binding->instance_id) {
                return CORE_PANE_MODULE_ERR_DUP_INSTANCE_ID;
            }
            if (bindings[j].pane_node_id == binding->pane_node_id) {
                return CORE_PANE_MODULE_ERR_DUP_PANE_ID;
            }
        }
    }

    return CORE_PANE_MODULE_OK;
}
