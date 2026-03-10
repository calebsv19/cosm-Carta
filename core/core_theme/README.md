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
