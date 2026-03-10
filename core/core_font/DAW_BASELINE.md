# core_font DAW Baseline Capture (8T1)

This file records the source-of-truth DAW defaults used to seed `core_font` `daw_default`.

## Captured Mapping

- Preset name: `daw_default`
- Primary role:
  - `ui_regular` family/style: `Montserrat Regular`
  - `point_size`: `9`
  - `primary_path`: `include/fonts/Montserrat/Montserrat-Regular.ttf`
  - Source: `daw/src/app/main.c` (`ui_font_set("include/fonts/Montserrat/Montserrat-Regular.ttf", 9)`)

## Notes
- Additional roles (`ui_medium`, `ui_bold`, `ui_mono`, `ui_mono_small`) are seeded for shared-role completeness.
- DAW capture for 8T1 focuses on preserving the currently configured default text face and size.
