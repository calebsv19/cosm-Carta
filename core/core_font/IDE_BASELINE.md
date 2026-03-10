# core_font IDE Baseline Capture (8T2)

This file records the source-of-truth IDE defaults used to seed `core_font` `ide`.

## Captured Mapping

- `ui_regular` and `ui_medium`
  - family/style: `Montserrat Medium`
  - point size: `14`
  - primary path: `include/fonts/Montserrat/Montserrat-Medium.ttf`
  - Source: `ide/src/engine/Render/render_font.c` (`initFontSystem` -> `loadFontByID(FONT_MONTSERRAT_MEDIUM)`)

- `ui_bold`
  - family/style: `Montserrat Bold`
  - point size: `14`
  - primary path: `include/fonts/Montserrat/Montserrat-Bold.ttf`
  - Source: `ide/src/engine/Render/render_font.c`

- `ui_mono`
  - primary path: `include/fonts/JetBrainsMono/JetBrainsMono-Regular.ttf`
  - fallback path: `include/fonts/FiraCode/FiraCode-Regular.ttf`
  - point size: `13`
  - Source: `ide/src/engine/Render/render_font.c` (`loadTerminalMonospaceFont` candidates)
