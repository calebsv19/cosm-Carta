# jobs

This directory stores route-job examples and local operator inputs for Carta's
headless routes.

## Ownership

- `examples/` contains checked-in sample job JSON files used as stable operator
  examples.
- Machine-local or private job payloads should use ignored `*.local.json` files.
- Generated run outputs belong under ignored run directories, not beside the job
  definitions.

## Pass Notes

- `R0`: keep examples small, inspectable, and source-controlled only when they
  are safe fixtures.
- `R5`: examples are candidates for deterministic headless test fixtures.
- `R4`: job payload validation, private coordinate leakage, and overwrite paths
  are security-sensitive boundaries.
