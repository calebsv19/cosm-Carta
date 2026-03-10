# core_trace

`core_trace` is the shared timeline/event foundation.

Current scope (v0 foundation):
- create a trace session with bounded capacities
- emit scalar samples (`time`, `lane`, `value`)
- emit marker events (`time`, `lane`, `label`)
- finalize and inspect in-memory records
- export/import trace sessions as `.pack` chunks (`TRHD`, `TRSM`, `TREV`)

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
