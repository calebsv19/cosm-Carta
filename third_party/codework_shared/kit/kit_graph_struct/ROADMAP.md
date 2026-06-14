# kit_graph_struct Roadmap

## Current State

`kit_graph_struct` is a bounded shared structural-graph kit. The hardened contract now covers:

- layered tree and DAG layout
- shared viewport helpers
- edge-route and edge-label placement helpers
- simple draw submission through `kit_render`
- explicit invalid-input rejection for ambiguous identity and edge-array cases

## Deferred Growth

The next lane is not more inline hardening. It is boundary-focused follow-up:

1. decomposition planning for the oversized `src/kit_graph_struct.c` file
2. optional richer routed-edge draw helpers if a host app actually needs shared reuse
3. broader graph interaction helpers only if they stay host-agnostic
4. automated Vulkan/visual parity checks if the harness becomes part of normal verification

## Explicit Non-Goals For This Lane

- no Memory Console graph UX migration
- no retained graph model ownership
- no persistence/import/export helpers
- no copied label storage or text-lifetime ownership shift inside `kit_graph_struct`
