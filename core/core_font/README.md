# core_font

Shared font role and fallback policy contract.

## Scope
- Font role IDs and names (`ui_regular`, `ui_medium`, `ui_bold`, `ui_mono`, `ui_mono_small`)
- Text size tier contract (`basic`, `paragraph`, `title`, `header`, `caption`)
- Preset IDs and names (`daw_default`, `ide`)
- Role resolution inside a preset with primary/fallback path hints
- Manifest parsing for role mappings + license/source metadata
- Deterministic path choice with fallback chain logic

## Dependencies
- `core_base`

## Baselines
- DAW capture reference: `DAW_BASELINE.md`
- IDE capture reference: `IDE_BASELINE.md`

## Status
- Bootstrap implementation with unit tests.
- Preset fallback paths now resolve to checked-in shared assets under `shared/assets/fonts/` for deterministic TTF availability in kit Vulkan UIs.
