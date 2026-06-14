# core_theme

Canonical semantic UI theme token and preset contract.

## Scope
- Semantic color tokens
- Scale tokens (spacing/radius/text scale)
- Stable preset IDs and names
- Preset lookup and active-preset selection
- Runtime override hook via environment variable

## Dependencies
- `core_base`

## Baselines
- DAW capture reference: `DAW_BASELINE.md`
- IDE capture reference: `IDE_BASELINE.md`

## Status
- Bootstrap implementation with unit tests.
- `2.0.0`: replaced program-specific preset names with centralized theme names, added `greyscale`, preserved legacy preset-name aliases for input parsing, and refined `midnight_contrast` / `soft_light` values.

## Current Contract
- `core_theme` currently exposes a process-global selected preset through `core_theme_select_preset*`, `core_theme_selected_preset_id`, and `core_theme_selected_preset`.
- That selected-preset state is mutable shared process state; it is not documented as thread-safe or isolated per renderer, host, or session.
- `core_theme_apply_env_override` is a selection hook only: it reads an environment variable, resolves the preset, and updates the same global selected-preset state.
- Preset lookup by name accepts both current canonical names and a small set of legacy aliases.

## Roadmap Only
- Override merge behavior and serialization are still roadmap work; the current implementation does not expose override objects, merge APIs, or encode/decode helpers.
