# core_workspace_authoring_session

`core_workspace_authoring_session` is a renderer-neutral state machine for
entering a paused workspace authoring session, validating and applying a draft,
canceling it, and recovering to a fail-safe runtime state. It owns transition
legality, capability declaration, result reporting, and the runtime mutation
gate. Hosts retain their draft data, validation rules, persistence, rendering,
and resume behavior through callbacks.

It deliberately does not model pane trees, module catalogs, UI widgets, or
runtime scheduling. Those remain in `core_layout`, `core_pane_module`, the
relevant kit, and each host application.
