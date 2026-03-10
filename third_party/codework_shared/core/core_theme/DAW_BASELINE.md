# core_theme DAW Baseline Capture (8T1)

This file records the source-of-truth DAW defaults used to seed `core_theme` `daw_default`.

## Captured Mapping

- `CORE_THEME_COLOR_SURFACE_0` = `{24,24,32,255}`
  - Source: `daw/src/ui/layout.c` (`ui_init_panes`, LIBRARY pane fill)
- `CORE_THEME_COLOR_SURFACE_1` = `{26,26,34,255}`
  - Source: `daw/src/ui/layout.c` (`ui_init_panes`, MENU pane fill)
- `CORE_THEME_COLOR_SURFACE_2` = `{32,32,40,255}`
  - Source: `daw/src/ui/layout.c` (`ui_init_panes`, TIMELINE pane fill)
- `CORE_THEME_COLOR_TEXT_PRIMARY` = `{220,220,230,255}`
  - Source: `daw/src/ui/layout.c` (`title_color`)
- `CORE_THEME_COLOR_TEXT_MUTED` = `{150,150,160,255}`
  - Source: `daw/src/ui/beat_grid.c`, `daw/src/ui/time_grid.c` (`label_color`)
- `CORE_THEME_COLOR_ACCENT_PRIMARY` = `{120,160,220,255}`
  - Source: `daw/src/ui/layout.c` (highlighted border)
- `CORE_THEME_COLOR_STATUS_OK` = `{130,170,100,255}`
  - Source: `daw/src/ui/effects_panel/meter_detail_lufs.c` (green meter)
- `CORE_THEME_COLOR_STATUS_WARN` = `{255,190,140,255}`
  - Source: `daw/src/ui/effects_panel/meter_detail_mid_side.c` (warn-ish meter tone)
- `CORE_THEME_COLOR_STATUS_ERROR` = `{180,60,60,220}`
  - Source: `daw/src/ui/effects_panel/list_view.c` (`fill_off`)
