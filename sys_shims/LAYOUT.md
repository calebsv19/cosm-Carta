# sys_shims Layout Contract

This file defines the committed directory structure for `shared/sys_shims`.

## Required Structure
- `shared/sys_shims/include/sys_shims/`
  - public shim headers only
- `shared/sys_shims/tests/`
  - shim smoke and conformance tests
- `shared/sys_shims/README.md`
  - purpose, goals, and usage entrypoint
- `shared/sys_shims/POLICY.md`
  - behavior and ownership rules
- `shared/sys_shims/ROADMAP.md`
  - phase-oriented implementation plan
- `shared/sys_shims/LANGUAGE_PROFILE.md`
  - language/frontend include mapping and profile contract
- `shared/sys_shims/Makefile`
  - `test`, `conformance`, `clean` targets
- `shared/sys_shims/VERSION`
  - module version source of truth

## Header Rules
- File names must use `shim_*.h`.
- Headers must be standalone-include safe.
- Wrappers and aliases must use `shim_` prefixes.

## Test Rules
- `test` target must be repeatable and deterministic.
- `test-matrix` target should execute compiler matrix checks.
- `conformance` target must exist (stub allowed in S0).
- `conformance` should emit a machine-readable report artifact in `build/`.
- Tests must compile with `cc`; `gcc` is optional if available.

## Stability Rules
- Keep API additive in early phases.
- Avoid global macro redefinitions of standard symbols.
- Keep fallback compatibility with direct system includes.
