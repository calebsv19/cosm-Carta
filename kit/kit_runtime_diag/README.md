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

## Build

```sh
make -C shared/kit/kit_runtime_diag
```

## Test

```sh
make -C shared/kit/kit_runtime_diag test
```
