# sys_shims Roadmap

## Mission
Build a reliable, opt-in shim layer for system includes that can support both C and custom language frontend integration.

## Current Phase
- S0 complete scaffold and policy baseline.
- S1 complete foundational type/preprocessor shim baseline.
- S2 complete runtime utility shim expansion and behavior tests.
- S3 complete math shim behavior and numeric accuracy baseline.
- S4 complete differential conformance harness and reporting baseline.
- S5 complete: program-by-program adoption with fallback toggles.
- S6 complete: language integration profile and compatibility matrix.
- S7 complete: extended system-domain shims (`limits`, `locale`, `ctype`, `signal`).

## Phase Milestones
- S1: foundational type/preprocessor shims (`stdbool/stddef/stdint/stdarg`).
- S2: runtime utility shims (`stdio/stdlib/string/errno/time`).
- S3: math shim behavior and numeric accuracy policy.
- S4: differential conformance harness.
- S5: incremental app adoption with fallbacks.
- S6: language integration profile and compatibility matrix.
- S7: extended system-domain shims and conformance lane expansion.
  - Added dedicated S7 regression binary: `sys_shims_extended_domains_test`.
  - Added additive helper surface for `limits/locale/ctype/signal`.

## Definition of Success
- Shim layer remains API-light and modular.
- Conformance checks catch regressions early.
- App adoption is incremental, reversible, and documented.
