# Repository Guidelines

## Project Structure & Module Organization
This repository is currently in an early planning phase. You will find project direction in:
- `MAPFORGE_SUMMARY.md`: high-level goals and behavior.
- `MAPFORGE_NORTHSTAR_PLAN.md`: phased roadmap and long-term layers.

Once implementation starts, expected structure (from the spec) is:
- `include/` and `src/` mirror one-to-one by module (e.g., `include/map/`, `src/map/`).
- `tools/` for offline build tools (OSM import, tile/graph builders).
- `assets/` for styles, fonts, icons.
- `data/regions/` for region packs.

## Build, Test, and Development Commands
Planned Makefile targets (from the spec):
- `make` or `make app`: build the main app binary.
- `make run`: run the app with the default region.
- `make tools`: build offline tooling binaries.
- `make region REGION=seattle OSM=path/to/file.osm.pbf`: build a region pack.
- `make clean`: remove build artifacts.

If these targets are not yet implemented, create or update the Makefile accordingly.

## Coding Style & Naming Conventions
- Language: C99, compile with `-Wall -Wextra -Wpedantic`.
- Indentation: 4 spaces, no tabs.
- Naming: `snake_case` for functions/variables, `PascalCase` for structs/enums, `MACRO_CASE` for macros.
- Headers and sources should mirror paths (`include/foo/bar.h` ↔ `src/foo/bar.c`).

## Testing Guidelines
No test framework is configured yet. If adding tests:
- Prefer lightweight C unit tests (e.g., a minimal custom harness or Unity/CMock).
- Place tests under `tests/` and name files `test_<module>.c`.
- Add a `make test` target that runs all tests.

## Commit & Pull Request Guidelines
No established commit style is present in this repository yet. Until it is defined:
- Use imperative, concise commit messages (e.g., "Add tile cache LRU").
- Keep PRs focused; include a brief summary and note any manual test steps.

## Configuration & Data
- Runtime configs are expected under `config/` (e.g., `config/app.config.json`).
- Region data should live under `data/regions/<region_name>/` and be treated as read-only at runtime.

## Agent Notes
When in doubt, follow the spec in `MAPFORGE_NORTHSTAR_PLAN.md` and keep rendering, routing, and tooling decoupled.
