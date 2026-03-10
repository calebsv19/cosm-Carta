# core_config

Small typed runtime configuration table for workspace-shell level settings.

## Scope
- Fixed-capacity key/value table
- Typed values (`bool`, `int64`, `double`, `string`)
- Deterministic upsert/get APIs

## Boundaries
- No file IO or persistence (`core_pack`/app policy own that)
- No schema graph semantics (`core_data` owns richer structures)
- No action routing or input policy (`core_action` owns that)

## Status
- Initial scaffold (`v0.1.0`) with bounded table tests.
