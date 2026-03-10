#include "core_action.h"

#include <assert.h>
#include <string.h>

int main(void) {
    CoreActionDef actions[4];
    CoreActionBinding bindings[4];
    CoreActionRegistry registry;
    const char *resolved = 0;

    assert(core_action_registry_init(&registry, actions, 4u, bindings, 4u));

    assert(core_action_register(&registry, "workspace.toggle_mode", "Toggle Mode"));
    assert(core_action_register(&registry, "workspace.apply", "Apply Changes"));
    assert(core_action_register(&registry, "workspace.cancel", "Cancel Changes"));
    assert(registry.action_count == 3u);

    assert(core_action_bind_trigger(&registry, "tab", "workspace.toggle_mode"));
    assert(core_action_bind_trigger(&registry, "enter", "workspace.apply"));
    assert(core_action_bind_trigger(&registry, "esc", "workspace.cancel"));
    assert(registry.binding_count == 3u);

    assert(core_action_resolve_trigger(&registry, "tab", &resolved));
    assert(strcmp(resolved, "workspace.toggle_mode") == 0);

    assert(core_action_bind_trigger(&registry, "tab", "workspace.apply"));
    assert(core_action_resolve_trigger(&registry, "tab", &resolved));
    assert(strcmp(resolved, "workspace.apply") == 0);

    assert(!core_action_bind_trigger(&registry, "h", "missing.action"));
    assert(!core_action_resolve_trigger(&registry, "missing", &resolved));

    assert(core_action_register(&registry, "workspace.remove", "Remove Pane"));
    assert(!core_action_register(&registry, "workspace.extra", "Overflow Action"));

    return 0;
}
