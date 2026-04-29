# core_pane_snapshot Roadmap

## Mission

Provide deterministic snapshot validation primitives shared by pane-host runtimes.

## Immediate Steps

1. Add structured validation report output (error index/details).
2. Add optional compatibility-mode gates for future minor schema revisions.
3. Add helper transforms for import normalization.

## Future Steps

1. Add chunk-level decode/encode adapters layered above this core.
2. Add snapshot diff helpers for diagnostics and review tools.
3. Add optional JSON debug export helpers if shared ownership becomes beneficial.
