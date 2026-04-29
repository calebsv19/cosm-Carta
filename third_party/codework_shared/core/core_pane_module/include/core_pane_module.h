#ifndef CORE_PANE_MODULE_H
#define CORE_PANE_MODULE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CorePaneModuleProviderKind {
    CORE_PANE_MODULE_PROVIDER_INTERNAL = 0,
    CORE_PANE_MODULE_PROVIDER_EXTERNAL = 1
} CorePaneModuleProviderKind;

typedef enum CorePaneModuleCapability {
    CORE_PANE_MODULE_CAP_RENDER = 1u << 0,
    CORE_PANE_MODULE_CAP_INPUT_KEYBOARD = 1u << 1,
    CORE_PANE_MODULE_CAP_INPUT_POINTER = 1u << 2,
    CORE_PANE_MODULE_CAP_FOCUS_REQUIRED = 1u << 3
} CorePaneModuleCapability;

typedef enum CorePaneModuleResult {
    CORE_PANE_MODULE_OK = 0,
    CORE_PANE_MODULE_ERR_INVALID_ARG = 1,
    CORE_PANE_MODULE_ERR_INVALID_DESCRIPTOR = 2,
    CORE_PANE_MODULE_ERR_DUP_TYPE_ID = 3,
    CORE_PANE_MODULE_ERR_DUP_KEY = 4,
    CORE_PANE_MODULE_ERR_REGISTRY_FULL = 5,
    CORE_PANE_MODULE_ERR_CAPABILITY_HOOK_MISMATCH = 6,
    CORE_PANE_MODULE_ERR_PROVIDER_NOT_ALLOWED = 7,
    CORE_PANE_MODULE_ERR_NOT_FOUND = 8,
    CORE_PANE_MODULE_ERR_DUP_INSTANCE_ID = 9,
    CORE_PANE_MODULE_ERR_DUP_PANE_ID = 10,
    CORE_PANE_MODULE_ERR_UNKNOWN_MODULE_TYPE = 11,
    CORE_PANE_MODULE_ERR_UNKNOWN_PANE_ID = 12
} CorePaneModuleResult;

typedef void (*CorePaneModuleRenderFn)(void *host_context,
                                       uint32_t pane_node_id,
                                       uint32_t instance_id);
typedef void (*CorePaneModuleInputFn)(void *host_context,
                                      uint32_t pane_node_id,
                                      uint32_t instance_id,
                                      const void *event);

typedef struct CorePaneModuleDescriptor {
    uint32_t module_type_id;
    const char *module_key;
    const char *display_name;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t capabilities;
    uint16_t default_config_variant;
    CorePaneModuleProviderKind provider_kind;
    CorePaneModuleRenderFn render;
    CorePaneModuleInputFn handle_keyboard;
    CorePaneModuleInputFn handle_pointer;
} CorePaneModuleDescriptor;

typedef struct CorePaneModuleBinding {
    uint32_t instance_id;
    uint32_t pane_node_id;
    uint32_t module_type_id;
    uint16_t config_variant;
    uint16_t runtime_flags;
} CorePaneModuleBinding;

typedef struct CorePaneModuleRegistry {
    CorePaneModuleDescriptor *entries;
    uint32_t count;
    uint32_t capacity;
} CorePaneModuleRegistry;

CorePaneModuleResult core_pane_module_registry_init(CorePaneModuleRegistry *registry,
                                                    CorePaneModuleDescriptor *entries,
                                                    uint32_t capacity);

CorePaneModuleResult core_pane_module_register(CorePaneModuleRegistry *registry,
                                               const CorePaneModuleDescriptor *descriptor);

CorePaneModuleResult core_pane_module_find_by_type_id(const CorePaneModuleRegistry *registry,
                                                      uint32_t module_type_id,
                                                      const CorePaneModuleDescriptor **out_descriptor);

CorePaneModuleResult core_pane_module_find_by_key(const CorePaneModuleRegistry *registry,
                                                  const char *module_key,
                                                  const CorePaneModuleDescriptor **out_descriptor);

CorePaneModuleResult core_pane_module_validate_bindings(const CorePaneModuleRegistry *registry,
                                                        const CorePaneModuleBinding *bindings,
                                                        uint32_t binding_count,
                                                        const uint32_t *leaf_pane_ids,
                                                        uint32_t leaf_pane_count);

#ifdef __cplusplus
}
#endif

#endif
