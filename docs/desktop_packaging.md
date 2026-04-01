# Desktop Packaging

MapForge now supports standardized macOS app-bundle packaging via Makefile targets.

## Build Package

```sh
make package-desktop
```

Output:

- `dist/MapForge.app`
- includes `Contents/Resources/data/regions` populated from `$(MAPFORGE_REGIONS_DIR)` when available

## Validate Package (Automated)

```sh
make package-desktop-smoke
make package-desktop-self-test
```

`package-desktop-self-test` runs launcher checks for:
- required launcher/binary/plist files
- bundled font/config/shader resources
- required runtime/regions directories

## Desktop Copy + Refresh Flow

```sh
make package-desktop-copy-desktop
make package-desktop-sync
make package-desktop-remove
make package-desktop-refresh
```

Default desktop destination:

- `$(HOME)/Desktop/MapForge.app`

You can override destination for local verification:

```sh
make package-desktop-copy-desktop DESKTOP_APP_DIR="$PWD/dist/_desktop_smoke/MapForge.app"
```

You can override which regions tree gets bundled:

```sh
make package-desktop PACKAGE_REGIONS_SRC="$HOME/Desktop/CodeWork/map_forge/data/regions"
```

## Open Packaged App

```sh
make package-desktop-open
```

## PK2 Closeout Snapshot (2026-03-31)

Completed gates:
- `make -C map_forge clean && make -C map_forge`
- `make -C map_forge package-desktop`
- `make -C map_forge package-desktop-smoke`
- `make -C map_forge package-desktop-self-test`
- `make -C map_forge package-desktop-copy-desktop`
- `make -C map_forge package-desktop-refresh`
- `make -C map_forge package-desktop-open`

Desktop copy target validated at:
- `/Users/calebsv/Desktop/MapForge.app`

## Post-PK2 Hardening Snapshot (2026-03-31)

Real Finder-launch issues were observed after initial PK2 closeout and then fixed:
- empty bundle regions could produce `No region configured` and immediate exit when external Desktop dev path was not available in launch context
- Vulkan shader root mismatch could trigger backend fallback to SDL in packaged runs

Hardening now in place:
- launcher default backend is `vulkan` unless explicitly overridden
- launcher logs region-selection reason and startup config
- packaged shaders are copied to both:
  - `Contents/Resources/vk_renderer/shaders/*`
  - `Contents/Resources/shaders/*`
- region packs are bundled into:
  - `Contents/Resources/data/regions`

## Launcher Runtime Model

`mapforge-launcher` sets app-relative defaults only when unset:
- `VK_RENDERER_SHADER_ROOT=<app>/Contents/Resources`
- `MAPFORGE_REGIONS_DIR` selection order:
  1. explicit env override (`MAPFORGE_REGIONS_DIR`)
  2. bundle regions dir (`<app>/Contents/Resources/data/regions`) when non-empty
  3. development fallback (`$HOME/Desktop/CodeWork/map_forge/data/regions`) when available
  4. bundle regions dir as final fallback

Launcher diagnostics:
- normal runs append logs to `~/Library/Logs/MapForge/launcher.log`
- `--print-config` prints resolved launch roots without launching the app
- `--self-test` prints launch roots after validation checks
- packaged default backend is now `vulkan` via launcher (`MAPFORGE_RENDER_BACKEND=vulkan` unless explicitly overridden)

Recommended final validation before moving to next project:
1. `make -C map_forge package-desktop-self-test`
2. `/Users/calebsv/Desktop/MapForge.app/Contents/MacOS/mapforge-launcher --print-config`
3. `open /Users/calebsv/Desktop/MapForge.app`
4. `tail -n 120 ~/Library/Logs/MapForge/launcher.log`

The launcher then switches cwd to `<app>/Contents/Resources` before executing `mapforge-bin` so relative runtime paths (`config/`, `assets/`, `data/runtime/`) resolve from the bundle.

Packaged shader layout:
- canonical copy: `<app>/Contents/Resources/vk_renderer/shaders/*`
- runtime mirror: `<app>/Contents/Resources/shaders/*`

The runtime mirror ensures Vulkan pipeline shader loads succeed from packaged cwd without relying on source-tree relative paths.
