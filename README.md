# Carta

Carta is an offline-first OpenStreetMap viewer and route explorer built in C99 for local desktop use.

This repository currently lives in the CodeWork workspace under `map_forge/`, but the public-facing product name is Carta. The runtime app consumes locally built region packs, so users can download their own OSM extracts, build a region directory, and explore the map without a server dependency.

## Current Scope

- Offline map viewing with smooth pan and zoom.
- Local route solving with on-disk region data.
- Region-pack build tooling for OSM-derived inputs.
- Shared CodeWork runtime integrations for queues, workers, wake paths, tracing, theme, and font adapters.

## Build Requirements

- macOS or Linux
- C99 compiler (`cc`, `clang`, or `gcc`)
- `make`
- `SDL2`
- `SDL2_ttf`
- `json-c`
- Vulkan loader and headers when using the Vulkan renderer path

## Build And Run

```bash
make
make run
```

Useful variants:

```bash
make tools
make graph
make test
make run-ide-theme
make run-daw-theme
```

Runtime region packs are read from `data/regions/` by default. Generated build output stays in `build/`.

## Adding Your Own OSM Data

Start with the public operator docs:

- [docs/ADDING_OSM_REGIONS.md](/Users/calebsv/Desktop/CodeWork/map_forge/docs/ADDING_OSM_REGIONS.md)
- [docs/REGION_PACK_LAYOUT.md](/Users/calebsv/Desktop/CodeWork/map_forge/docs/REGION_PACK_LAYOUT.md)
- [docs/KEYBINDS.md](/Users/calebsv/Desktop/CodeWork/map_forge/docs/KEYBINDS.md)

## Repository Layout

- `src/` and `include/`: runtime application code
- `tools/`: offline region-build tooling
- `tests/`: focused C test targets and smoke checks
- `docs/`: public usage and format references
- `third_party/codework_shared/`: vendored shared CodeWork libraries
- `data/regions/`: local generated region packs, ignored from git
- `build/`: generated build output, ignored from git

## Docs

- [docs/README.md](/Users/calebsv/Desktop/CodeWork/map_forge/docs/README.md): public docs index
- [MAPFORGE_SUMMARY.md](/Users/calebsv/Desktop/CodeWork/map_forge/MAPFORGE_SUMMARY.md): product summary and architecture snapshot

Private planning and implementation docs do not live in this repository lane. They are kept in the top-level CodeWork private docs bucket:

- `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/map_forge/`

## Shared Library Workflow

Shared CodeWork libraries are vendored under `third_party/codework_shared/`.

Update flow:

```bash
git fetch shared-upstream main
git subtree pull --prefix=third_party/codework_shared shared-upstream main --squash
```

## License

This repository is licensed under Apache License 2.0. See `LICENSE`.
