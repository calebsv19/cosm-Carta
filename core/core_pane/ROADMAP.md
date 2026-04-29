# core_pane Roadmap

## Mission

Provide a renderer-agnostic pane-tree contract that can be adopted by multiple CodeWork programs for consistent split-pane behavior.

## Immediate Steps

1. Add richer hit metadata for cursor-feedback adapters in app/kit layers.
2. Add collapse-state semantics and constraint-aware restoration behavior.
3. Add structured diagnostics detail payloads for validation and solve failures.

## Future Steps

1. Add optional serialization helper structs (schema-neutral, no parser ownership).
2. Add deterministic replay tests for drag sequences and resize transitions.
3. Provide a small integration bridge for `kit_ui` consumers (`kit_pane`, optional).
