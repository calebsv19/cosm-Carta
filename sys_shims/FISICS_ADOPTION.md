# fisiCs Shim Adoption Notes (S5/S6)

Status: S5 complete, S6 baseline active

## Goals
- Validate shim include interpretation in the compiler without changing default behavior.
- Keep baseline and shim-enabled builds runnable side-by-side.
- Preserve rollback path at all times.

## Build Modes
- Baseline (default):
  - `SHIM_MODE=off`
  - binary: `fisiCs/fisics`
- Shadow shim mode:
  - `SHIM_MODE=shadow`
  - binary: `fisiCs/fisics_shim_shadow`
  - include overlay prepended to default include paths

## New Make Targets
- `make -C fisiCs shim-build-shadow`
  - builds `fisics_shim_shadow`
- `make -C fisiCs shim-parse-smoke`
  - runs preprocessor smoke tests using shim overlay path
- `make -C fisiCs shim-parse-parity`
  - validates baseline and shadow paths both pass key include tests
- `make -C fisiCs shim-parse-parity-quiet`
  - runs the same baseline/shadow checks via a quiet harness (no full IR dump on success)
- `make -C fisiCs shim-gate`
  - runs quiet parity plus `shared/sys_shims` conformance
- `make -C fisiCs shim-language-profile`
  - validates mixed local+system include behavior under shadow profile mapping
- `make -C fisiCs shim-language-profile-negative`
  - validates that incomplete profile roots are detected as invalid
- `make -C fisiCs shim-s6-gate`
  - runs S6 positive + negative profile fixtures together

## Verification Scope (Current)
- system include test path (`run_pp_system_include.sh`)
- include search-order behavior (`run_pp_include_search.sh`)
- multi-domain shim include behavior (`stdint/stddef/time/math`)
- multi-domain shim include behavior (`stdint/stddef/time/math/limits/locale/ctype/signal`)
- mixed local+system include profile fixture (`system_include_language_profile_mix.c`)
- negative profile root validation fixture (`system_include_language_profile_contract.c`)
- compiler-side strict profile pin/check (`FISICS_SHIM_PROFILE_ENFORCE`) for id/version/path-order enforcement

## Rollback
- Use baseline target/build only:
  - `make -C fisiCs SHIM_MODE=off all`
- Ignore `shim-*` targets.

## Next S6 Tasks
1. Expand profile mapping coverage beyond the initial 10 shim domains.
2. Add language-frontend specific fixture lanes as parser capabilities grow.
3. Add profile-version pinning in compiler/runtime config before default-mode changes.
