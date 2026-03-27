# src Overview

This tree contains the runtime application for MapForge.
The runtime goal is to load offline region packs from `${MAPFORGE_REGIONS_DIR:-data/regions}/<region>/`, render map layers smoothly, and compute local routes.

## Directory Map

- `src/main.c`: Process entrypoint; calls `map_forge_app_main()`.
- `src/app/map_forge_app_main.c`: lifecycle-locked wrapper entry path for scaffold migration.
- `src/app/`: App orchestration, frame loop, region switching, tile pipeline, UI/HUD, playback.
- `src/camera/`: Camera movement, zoom, and world/screen transforms.
- `src/core/`: Shared runtime utilities (input, logging, timing).
- `src/map/`: Map-domain logic (Mercator/tile math, tile loading/caching, rendering policy/helpers).
- `src/render/`: Renderer backend abstraction and backend-specific retained cache logic.
- `src/route/`: Graph loading, route solving, route state, and route drawing.
- `src/ui/`: UI helpers used across overlays and text rendering.

## Runtime Flow (High Level)

1. `main.c` enters `map_forge_app_main()` and then delegates runtime loop execution into `src/app/`.
2. `src/app/` loads region metadata and configures camera + layers.
3. Tile requests go through `src/map/` loaders/managers and optional Vulkan retained cache.
4. Draw calls go through `src/render/` abstraction.
5. Route graph/path operations run via `src/route/` and are drawn into the same frame.
6. UI/debug overlays render via `src/ui/`.
