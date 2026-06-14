# core_pane_module Roadmap

## Mission

Provide a shared, deterministic pane-module contract surface that can be reused by workspace hosts.

## Immediate Steps

1. Add registry metadata export helpers for snapshot/debug tooling.
2. Add stricter config-variant and focus-required validation only after hosts need behavior beyond the current pass-through contract.
3. Add host-facing error detail codes for diagnostics UX if boolean/result-code boundaries become insufficient.

## Future Steps

1. Add provider abstraction extensions for external module discovery.
2. Add compatibility/version negotiation helpers.
3. Add optional schema-neutral module state snapshot hooks.
