# core_trace

`core_trace` is the shared timeline/event foundation.

Current scope (v0 foundation):
- create a trace session with bounded capacities
- emit scalar samples (`time`, `lane`, `value`)
- emit marker events (`time`, `lane`, `label`)
- finalize and inspect in-memory records
- export/import trace sessions as `.pack` chunks (`TRHD`, `TRSM`, `TREV`)

Current contract notes:
- lane/label validation happens before any bounded-buffer eviction, so failed emits do not mutate retained samples/markers
- finalize is the artifact boundary: `.pack` export requires a finalized session, and imported sessions are loaded as finalized snapshots
- import accepts unknown extra chunks but rejects malformed required trace chunks and oversized count/payload claims
- `phase7-check` now generates a scratch fixture under `build/` for inspection and does not rewrite the tracked fixture file
- `make fixtures` is the explicit path for refreshing the checked-in fixture at `tests/fixtures/trace_v1_sample.pack`

Build/test:

```sh
make -C shared/core/core_trace test
```

Phase 7 gate (tests + fixture + inspect):

```sh
make -C shared/core/core_trace phase7-check
```

Spec:
- `shared/core/core_trace/TRACE_V1_SPEC.md`
- `shared/core/core_trace/TRACE_LANE_POLICY_V1.md`
