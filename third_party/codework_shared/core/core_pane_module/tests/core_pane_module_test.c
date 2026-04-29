#include "core_pane_module.h"

#include <assert.h>
#include <string.h>

static void noop_render(void *host_context, uint32_t pane_node_id, uint32_t instance_id) {
    (void)host_context;
    (void)pane_node_id;
    (void)instance_id;
}

static void noop_input(void *host_context,
                       uint32_t pane_node_id,
                       uint32_t instance_id,
                       const void *event) {
    (void)host_context;
    (void)pane_node_id;
    (void)instance_id;
    (void)event;
}

static CorePaneModuleDescriptor make_valid_descriptor(uint32_t type_id, const char *key) {
    CorePaneModuleDescriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.module_type_id = type_id;
    descriptor.module_key = key;
    descriptor.display_name = "Test";
    descriptor.version_major = 1u;
    descriptor.version_minor = 0u;
    descriptor.capabilities = CORE_PANE_MODULE_CAP_RENDER |
                              CORE_PANE_MODULE_CAP_INPUT_KEYBOARD |
                              CORE_PANE_MODULE_CAP_INPUT_POINTER;
    descriptor.default_config_variant = 0u;
    descriptor.provider_kind = CORE_PANE_MODULE_PROVIDER_INTERNAL;
    descriptor.render = noop_render;
    descriptor.handle_keyboard = noop_input;
    descriptor.handle_pointer = noop_input;
    return descriptor;
}

static void test_registry_register_and_lookup(void) {
    CorePaneModuleDescriptor entries[8];
    CorePaneModuleRegistry registry;
    const CorePaneModuleDescriptor *found = NULL;
    CorePaneModuleDescriptor descriptor = make_valid_descriptor(1001u, "text_panel");

    assert(core_pane_module_registry_init(&registry, entries, 8u) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_register(&registry, &descriptor) == CORE_PANE_MODULE_OK);
    assert(registry.count == 1u);

    assert(core_pane_module_find_by_type_id(&registry, 1001u, &found) == CORE_PANE_MODULE_OK);
    assert(found != NULL);
    assert(strcmp(found->module_key, "text_panel") == 0);

    found = NULL;
    assert(core_pane_module_find_by_key(&registry, "text_panel", &found) == CORE_PANE_MODULE_OK);
    assert(found != NULL);
    assert(found->module_type_id == 1001u);
}

static void test_registry_rejects_duplicates(void) {
    CorePaneModuleDescriptor entries[8];
    CorePaneModuleRegistry registry;
    CorePaneModuleDescriptor a = make_valid_descriptor(1001u, "text_panel");
    CorePaneModuleDescriptor b = make_valid_descriptor(1001u, "other_panel");
    CorePaneModuleDescriptor c = make_valid_descriptor(1002u, "text_panel");

    assert(core_pane_module_registry_init(&registry, entries, 8u) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_register(&registry, &a) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_register(&registry, &b) == CORE_PANE_MODULE_ERR_DUP_TYPE_ID);
    assert(core_pane_module_register(&registry, &c) == CORE_PANE_MODULE_ERR_DUP_KEY);
}

static void test_registry_rejects_invalid_descriptor_paths(void) {
    CorePaneModuleDescriptor entries[4];
    CorePaneModuleRegistry registry;
    CorePaneModuleDescriptor bad_key = make_valid_descriptor(1001u, "Bad-Key");
    CorePaneModuleDescriptor bad_provider = make_valid_descriptor(1002u, "list_panel");
    CorePaneModuleDescriptor bad_capability = make_valid_descriptor(1003u, "inspector_panel");

    bad_provider.provider_kind = CORE_PANE_MODULE_PROVIDER_EXTERNAL;
    bad_capability.handle_keyboard = NULL;

    assert(core_pane_module_registry_init(&registry, entries, 4u) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_register(&registry, &bad_key) == CORE_PANE_MODULE_ERR_INVALID_DESCRIPTOR);
    assert(core_pane_module_register(&registry, &bad_provider) == CORE_PANE_MODULE_ERR_PROVIDER_NOT_ALLOWED);
    assert(core_pane_module_register(&registry, &bad_capability) == CORE_PANE_MODULE_ERR_CAPABILITY_HOOK_MISMATCH);
}

static void test_binding_validation(void) {
    CorePaneModuleDescriptor entries[8];
    CorePaneModuleRegistry registry;
    CorePaneModuleDescriptor descriptor_a;
    CorePaneModuleDescriptor descriptor_b;
    uint32_t pane_ids[3] = { 1u, 2u, 3u };

    CorePaneModuleBinding good[2] = {
        { 1u, 1u, 1001u, 0u, 0u },
        { 2u, 2u, 1002u, 0u, 0u }
    };
    CorePaneModuleBinding bad_unknown_pane[1] = {
        { 10u, 99u, 1001u, 0u, 0u }
    };
    CorePaneModuleBinding bad_unknown_module[1] = {
        { 11u, 1u, 9999u, 0u, 0u }
    };
    CorePaneModuleBinding bad_dup_pane[2] = {
        { 12u, 1u, 1001u, 0u, 0u },
        { 13u, 1u, 1002u, 0u, 0u }
    };
    CorePaneModuleBinding bad_dup_instance[2] = {
        { 42u, 1u, 1001u, 0u, 0u },
        { 42u, 2u, 1002u, 0u, 0u }
    };

    descriptor_a = make_valid_descriptor(1001u, "text_panel");
    descriptor_b = make_valid_descriptor(1002u, "list_panel");

    assert(core_pane_module_registry_init(&registry, entries, 8u) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_register(&registry, &descriptor_a) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_register(&registry, &descriptor_b) == CORE_PANE_MODULE_OK);

    assert(core_pane_module_validate_bindings(&registry, good, 2u, pane_ids, 3u) == CORE_PANE_MODULE_OK);
    assert(core_pane_module_validate_bindings(&registry, bad_unknown_pane, 1u, pane_ids, 3u) ==
           CORE_PANE_MODULE_ERR_UNKNOWN_PANE_ID);
    assert(core_pane_module_validate_bindings(&registry, bad_unknown_module, 1u, pane_ids, 3u) ==
           CORE_PANE_MODULE_ERR_UNKNOWN_MODULE_TYPE);
    assert(core_pane_module_validate_bindings(&registry, bad_dup_pane, 2u, pane_ids, 3u) ==
           CORE_PANE_MODULE_ERR_DUP_PANE_ID);
    assert(core_pane_module_validate_bindings(&registry, bad_dup_instance, 2u, pane_ids, 3u) ==
           CORE_PANE_MODULE_ERR_DUP_INSTANCE_ID);
}

int main(void) {
    test_registry_register_and_lookup();
    test_registry_rejects_duplicates();
    test_registry_rejects_invalid_descriptor_paths();
    test_binding_validation();
    return 0;
}
