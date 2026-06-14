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

## Current Contract
- `core_font_choose_path` is a selection helper, not a filesystem validator.
- When no `path_exists_fn` callback is supplied, `core_font_choose_path` returns the first non-empty configured candidate: primary first, then fallback.
- Unknown manifest entry keys are tolerated for forward-compatible metadata growth.
- Unknown non-entry manifest lines are rejected as format errors.
- Manifest lines longer than the current parser buffer are rejected as invalid input.

## Current Limits
- The preset set is currently fixed to `daw_default` and `ide`.
- Manifest parsing validates line structure and required fields, but does not own asset installation or synchronization policy.
