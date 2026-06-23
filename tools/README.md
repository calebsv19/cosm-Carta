# tools

This directory owns Carta's offline operator tools for building and validating
local region packs.

## Ownership

- `mapforge_region*`: staged region-pack production, metadata/archive companion
  generation, tile file output, validation, and publish steps.
- `mapforge_graph*`: graph ingest, graph package output, and route graph publish
  helpers.
- Shell helpers in this directory are local operator wrappers for region
  maintenance.

Generated region data belongs under ignored data or run directories, not in
`tools/`.

## Pass Notes

- `R0`: keep this directory focused on offline build/validation tools.
- `R1`: duplication candidates include path handling, config loading, and output
  root normalization shared between graph and region tools.
- `R4`: import paths, archive extraction, publish roots, and overwrite behavior
  are security-sensitive boundaries.
