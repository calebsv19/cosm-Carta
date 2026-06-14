# core_config

Small typed runtime configuration table for workspace-shell level settings.

## Scope
- Fixed-capacity key/value table
- Typed values (`bool`, `int64`, `double`, `string`)
- Deterministic upsert/get APIs
- Copied key storage and copied string storage
- Value copy-out on successful get

## Boundaries
- No file IO or persistence (`core_pack`/app policy own that)
- No schema graph semantics (`core_data` owns richer structures)
- No action routing or input policy (`core_action` owns that)
- No delete, iteration, merge/diff, or namespace helpers in the current surface

## Current Contract Notes
- Keys are copied into fixed entry storage and must be non-empty and at most `CORE_CONFIG_MAX_KEY_LENGTH` bytes.
- String values are copied into fixed entry storage and must be at most `CORE_CONFIG_MAX_STRING_LENGTH` bytes.
- Get returns a copied `CoreConfigValue`; callers do not borrow internal entry storage.
- The public API returns only `bool`, so invalid input, full-table insertion failure, and missing-key lookup are intentionally collapsed into host-interpreted failure.
- Existing keys can still be updated when the table is already full; only insertion of a new key fails on capacity pressure.
- `core_config_set_double(...)` now rejects non-finite values (`NaN`, `Inf`) at the shared boundary.
- Key meaning, fallback policy, persistence, merge/diff behavior, and preference/UI semantics remain host-owned.

## Status
- Hardened fixed-capacity scaffold (`v0.1.1`) with key/string boundary coverage, deterministic failed-get clearing, type-replacement tests, and non-finite double rejection.
