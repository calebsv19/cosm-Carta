# core_font Manifest Format (v1)

`core_font` manifest files are line-based and deterministic for simple parsing in C.

## Version Header

`version=core_font_manifest_v1`

## Entry Lines

Each role mapping uses one `entry|...` line with pipe-delimited key/value segments.

Example:

`entry|preset=daw_default|role=ui_regular|family=Montserrat|style=Regular|weight=400|point=9|primary=include/fonts/Montserrat/Montserrat-Regular.ttf|fallback=shared/assets/fonts/Montserrat-Regular.ttf|license=OFL-1.1|source=GoogleFonts|pack=basic`

## Required Keys

- `preset`
- `role`
- `primary`

## Optional Keys

- `family`
- `style`
- `weight`
- `point`
- `fallback`
- `license`
- `source`
- `pack`
