# Shared Theme/Font Rollout Contract

This contract defines how app-level UI theme and font adapters are integrated and validated.

## 1) App Env Contract

Every app adapter must support these variables:

- `<APP>_USE_SHARED_THEME_FONT`
- `<APP>_USE_SHARED_THEME`
- `<APP>_USE_SHARED_FONT`
- `<APP>_THEME_PRESET`
- `<APP>_FONT_PRESET`

Behavior:

- `<APP>_USE_SHARED_THEME_FONT` remains the global override.
- `<APP>_USE_SHARED_THEME` and `<APP>_USE_SHARED_FONT` can override the global toggle.
- Apps may now default shared mode on as their normal runtime path.
- If shared mode is disabled, legacy app colors/fonts must remain active.

## 2) Required Adapter Surface

Each app must expose:

- Theme resolver that maps `core_theme` tokens into local UI colors.
- Font resolver that maps `core_font` roles into local runtime font path + point size.
- Fallback path when adapter resolution fails (do not crash app startup/render).

## 3) Preset Matrix Requirement

Each app must verify theme preset resolution for:

- at least one shared blue preset (`studio_blue` or `harbor_blue`)
- `midnight_contrast`
- `soft_light`
- `standard_grey`
- `greyscale`

Current implementation note:

- Matrix validation is implemented in per-app `test-shared-theme-font-adapter` tests.

## 4) Font Tier Requirement

For UI-facing text, adapters should resolve point size by tier via `core_font_point_size_for_tier(...)`, using:

- `caption`
- `basic`
- `paragraph`
- `title`
- `header`

Current policy:

- UI-facing paths should avoid raw hardcoded point sizes unless fallback mode is active.

## 5) Runtime Contract

- Theme adapters should support runtime preset state, not env-only startup selection.
- Standard keybinds:
  - `Ctrl/Cmd+Shift+T`: next preset
  - `Ctrl/Cmd+Shift+Y`: previous preset
- The current preset should persist across relaunch when the app supports runtime cycling.

## 6) Exit Criteria Per App

- App builds and runs with shared mode disabled (fallback path).
- App builds and runs with shared mode enabled.
- `test-shared-theme-font-adapter` passes.
- Preset matrix checks pass.
- Runtime cycling visibly repaints affected UI surfaces.
- Persistence restores the last selected preset where runtime cycling is user-facing.

## 7) Current App Mapping

- DAW: `DAW_*`
- IDE: `IDE_*`
- line_drawing: `LINE_DRAWING_*`
- line_drawing3d: `LINE_DRAWING3D_*`
- ray_tracing: `RAY_TRACING_*`
- physics_sim: `PHYSICS_SIM_*`
- map_forge: `MAPFORGE_*`
