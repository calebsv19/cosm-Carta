# kit_runtime_diag

`kit_runtime_diag` is a small shared diagnostics helper kit for runtime-loop instrumentation.

It standardizes two neutral pieces:
1. stage timestamp -> timing-ms derivation
2. frame input counter -> cumulative totals accumulation

## Boundary

`kit_runtime_diag` owns:
1. timing math and counter accumulation helpers
2. app-neutral diagnostics contract structs

`kit_runtime_diag` does not own:
1. input normalization policy
2. pane routing behavior
3. render behavior or backend policy
4. app-specific log payload semantics

## Current Contract

1. `kit_runtime_diag_duration_ms(...)` is a raw elapsed-time helper over caller-supplied monotonic timestamps. It returns `0.0` for non-finite inputs and otherwise preserves caller ordering, including negative deltas.
2. `kit_runtime_diag_compute_timings(...)` derives fixed stage deltas only. It does not infer missing stages or validate host stage ordering beyond rejecting non-finite stage marks.
3. Invalid or null stage input clears the output timings to zero rather than leaving partially written fields behind.
4. `kit_runtime_diag_input_totals_accumulate(...)` is a saturating cumulative counter helper over borrowed frame input snapshots.
5. `shortcut_gated_count` only accumulates `ignored_count` when `text_entry_gate_active` is true.
6. `invalidation_reason_bits` is frame-local input context and is intentionally not accumulated into totals.

## Build

```sh
make -C shared/kit/kit_runtime_diag
```

## Test

```sh
make -C shared/kit/kit_runtime_diag test
```
