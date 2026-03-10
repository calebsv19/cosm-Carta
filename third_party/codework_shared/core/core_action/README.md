# core_action

Renderer-agnostic action registry and trigger binding table.

## Scope
- Register action metadata (`id`, `label`)
- Bind simple trigger tokens to action ids
- Resolve trigger -> action deterministically

## Boundaries
- No platform keycode decoding (app/kit adapter owns that)
- No command execution policy (app runtime owns that)
- No UI command palette rendering

## Status
- Initial scaffold (`v0.1.0`) with registry and trigger-resolution tests.
