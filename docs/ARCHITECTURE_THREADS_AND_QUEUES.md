# MapForge Threads And Queues

This doc summarizes thread ownership, queue topology, and lifecycle ordering.

## Worker Threads

MapForge runtime uses:

- Main thread: event/update/render loop
- Route worker thread: async route compute jobs/results
- Tile loader worker pool: region tile decode/load jobs
- Vulkan poly prep thread: polygon triangulation prep
- Vulkan asset worker thread: staging/build of Vulkan tile assets

## Queue Topology (High-Level)

- Route job/result handoff:
  - guarded by `route_worker_mutex` + `route_worker_cond`
  - request-id generation checks prevent stale apply

- Tile loader:
  - tile request/result queues
  - generation tagging to drop stale region-era work

- Vulkan prep/asset:
  - prep input/output queues
  - asset stage + ready queues
  - explicit drop/evict counters and trace lanes

## Generation And Stale-Result Contract

`src/app/app_worker_contract.c` standardizes:

- world/tile generation bumps
- route request generation
- current/stale checks for async results
- stale-first then oldest eviction selection helper

## Queue Pressure Policy

Canonical policy:

1. evict stale generation entries first
2. if none stale, evict oldest queued entry
3. reject new enqueue only when eviction cannot make space

Counters separate:

- `*_evict_count` for removing queued existing items
- `*_drop_count` for rejecting new work

## Lifecycle Order Invariants

Startup:

1. state/bridge init
2. worker contract generation init
3. worker/thread init
4. enter frame loop

Region switch:

1. bump world/tile generation
2. clear old-generation queues/caches
3. reload region + graph
4. resume scheduling on new generation

Shutdown:

1. stop worker activity
2. drain/clear queues
3. free tile/Vulkan caches
4. shutdown renderer/window

## Contributor Warning

When changing queue behavior, keep trace markers and counters aligned with semantics; otherwise debugging drops/evictions becomes misleading.
