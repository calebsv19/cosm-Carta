# Carta Memory-Check Audit

`map_forge` provides a default-off fisiCs memory-check lane for the route
service allocation path:

```sh
make -C map_forge memory-check-audit
```

The audit rebuilds the existing route-service test with the
`physics-units,memory-check` overlay, links the fisiCs memory-check runtime,
and runs the generated focused test binary. This keeps the diagnostic pass
separate from the normal Clang build, desktop packaging, and release flow.

Report files:

- `map_forge/build/memory_check/map_forge.stdout`
- `map_forge/build/memory_check/map_forge.stderr`

## Current Baseline

Last audited: 2026-06-07

```text
[fisics:memory-check] summary: active=0 leaked_bytes=0 allocs=4 frees=4 double_free=0 unknown_free=0 tracker_failures=0
```
