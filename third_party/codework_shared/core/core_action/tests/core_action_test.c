#include "core_action.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static void fill_string(char *dst, size_t len, char ch) {
    size_t i;
    for (i = 0u; i < len; ++i) {
        dst[i] = ch;
    }
    dst[len] = '\0';
}

int main(void) {
    CoreActionDef actions[4];
    CoreActionBinding bindings[4];
    CoreActionRegistry registry;
    const char *resolved = 0;
    char exact_action_id[CORE_ACTION_MAX_ID_LENGTH + 1];
    char overlong_action_id[CORE_ACTION_MAX_ID_LENGTH + 2];
    char exact_label[CORE_ACTION_MAX_LABEL_LENGTH + 1];
    char overlong_label[CORE_ACTION_MAX_LABEL_LENGTH + 2];
    char exact_trigger[CORE_ACTION_MAX_TRIGGER_LENGTH + 1];
    char overlong_trigger[CORE_ACTION_MAX_TRIGGER_LENGTH + 2];

    fill_string(exact_action_id, CORE_ACTION_MAX_ID_LENGTH, 'a');
    fill_string(overlong_action_id, CORE_ACTION_MAX_ID_LENGTH + 1, 'b');
    fill_string(exact_label, CORE_ACTION_MAX_LABEL_LENGTH, 'c');
    fill_string(overlong_label, CORE_ACTION_MAX_LABEL_LENGTH + 1, 'd');
    fill_string(exact_trigger, CORE_ACTION_MAX_TRIGGER_LENGTH, 'e');
    fill_string(overlong_trigger, CORE_ACTION_MAX_TRIGGER_LENGTH + 1, 'f');

    assert(!core_action_registry_init(NULL, actions, 4u, bindings, 4u));
    assert(!core_action_registry_init(&registry, NULL, 4u, bindings, 4u));
    assert(!core_action_registry_init(&registry, actions, 0u, bindings, 4u));
    assert(!core_action_registry_init(&registry, actions, 4u, NULL, 4u));
    assert(!core_action_registry_init(&registry, actions, 4u, bindings, 0u));
    assert(core_action_registry_init(&registry, actions, 4u, bindings, 4u));

    assert(!core_action_register(NULL, "workspace.toggle_mode", "Toggle Mode"));
    assert(!core_action_register(&registry, NULL, "Toggle Mode"));
    assert(!core_action_register(&registry, "workspace.toggle_mode", NULL));
    assert(!core_action_register(&registry, "", "Toggle Mode"));
    assert(!core_action_register(&registry, "workspace.toggle_mode", ""));
    assert(core_action_register(&registry, exact_action_id, exact_label));
    assert(registry.action_count == 1u);
    assert(!core_action_register(&registry, overlong_action_id, "Overflow Action Id"));
    assert(!core_action_register(&registry, "workspace.too_long_label", overlong_label));
    assert(registry.action_count == 1u);

    assert(core_action_register(&registry, "workspace.toggle_mode", "Toggle Mode"));
    assert(core_action_register(&registry, "workspace.apply", "Apply Changes"));
    assert(core_action_register(&registry, "workspace.cancel", "Cancel Changes"));
    assert(registry.action_count == 4u);

    assert(core_action_register(&registry, "workspace.apply", "Apply Changes Duplicate"));
    assert(registry.action_count == 4u);
    assert(!core_action_register(&registry, "workspace.remove", "Remove Pane"));
    assert(registry.action_count == 4u);

    assert(!core_action_bind_trigger(NULL, "tab", "workspace.toggle_mode"));
    assert(!core_action_bind_trigger(&registry, NULL, "workspace.toggle_mode"));
    assert(!core_action_bind_trigger(&registry, "tab", NULL));
    assert(!core_action_bind_trigger(&registry, "", "workspace.toggle_mode"));
    assert(!core_action_bind_trigger(&registry, "h", "missing.action"));
    assert(core_action_bind_trigger(&registry, exact_trigger, exact_action_id));
    assert(registry.binding_count == 1u);
    assert(!core_action_bind_trigger(&registry, overlong_trigger, exact_action_id));
    assert(registry.binding_count == 1u);
    assert(core_action_bind_trigger(&registry, "tab", "workspace.toggle_mode"));
    assert(core_action_bind_trigger(&registry, "enter", "workspace.apply"));
    assert(core_action_bind_trigger(&registry, "esc", "workspace.cancel"));
    assert(core_action_bind_trigger(&registry, "tab", "workspace.apply"));
    assert(registry.binding_count == 4u);
    assert(!core_action_bind_trigger(&registry, "space", "workspace.toggle_mode"));
    assert(registry.binding_count == 4u);

    assert(core_action_resolve_trigger(&registry, "tab", &resolved));
    assert(strcmp(resolved, "workspace.apply") == 0);
    assert(core_action_resolve_trigger(&registry, exact_trigger, &resolved));
    assert(strcmp(resolved, exact_action_id) == 0);

    resolved = "sentinel";
    assert(!core_action_resolve_trigger(NULL, "tab", &resolved));
    assert(strcmp(resolved, "sentinel") == 0);
    assert(!core_action_resolve_trigger(&registry, NULL, &resolved));
    assert(strcmp(resolved, "sentinel") == 0);
    assert(!core_action_resolve_trigger(&registry, "tab", NULL));
    assert(strcmp(resolved, "sentinel") == 0);
    assert(!core_action_resolve_trigger(&registry, "missing", &resolved));
    assert(resolved == NULL);

    return 0;
}
