# Desktop Packaging

MapForge now supports standardized macOS app-bundle packaging via Makefile targets.

Last updated: 2026-08-07

## Build Package

```sh
make package-desktop
```

Default host output:

- `build/targets/<target-triple>/dist/Carta.app`
- includes app/runtime scaffolding plus bundled ingest tools (`mapforge_region`, `mapforge_graph`); region payloads are not embedded by default

Release artifacts encode the target architecture:

- `Carta-<version>-macOS-arm64-stable.zip`
- `Carta-<version>-macOS-x86_64-stable.zip`

Example Intel package build from Apple Silicon:

```sh
make release-artifact TARGET_ARCH=x86_64 BUILD_TOOLCHAIN=clang PACKAGE_TOOLCHAIN=clang
```

## Production Registry Output Root

The pre-authentication release lane is create-only and does not replace the
installed Desktop app:

```sh
make RELEASE_ROOT=build/release-authenticated/<job-id> \
  release-output-root-conformance
```

`RELEASE_ROOT` must be a new job-scoped relative path beneath
`build/release-authenticated/`. The target emits the local ad-hoc ZIP, SHA-256
file, and manifest there. The manifest binds the target and release
OS/architecture/variant, ZIP basename, and exact artifact SHA-256; Developer ID
signing and notarization remain a later, separately authorized stage.

## Validate Package (Automated)

```sh
make package-desktop-smoke
make package-desktop-self-test
```

`package-desktop-self-test` runs launcher checks for:
- required launcher/binary/plist files
- bundled font/config/shader resources
- bundled ingest tools
- required runtime/regions directory scaffolding
- the bundled `--vulkan-rollout-self-test`, including validation-required
  runtime/renderer identity, initial readback, real resize/recovery, resized
  readback, and capture-dimension checks when the local Vulkan development
  runtime is available

The Vulkan package proof creates a temporary absolute validation-layer
manifest only for the self-test process. Normal packaged launch neither ships
nor requires that development validation layer. The 2026-08-05 Apple
M2/MoltenVK proof retained the existing Retina relationship: logical
`1280x720` produced `2560x1440`, and logical `1440x800` produced `2880x1600`.

## Desktop Copy + Refresh Flow

```sh
make package-desktop-copy-desktop
make package-desktop-sync
make package-desktop-remove
make package-desktop-refresh
```

Optional icon inputs:

```sh
make package-desktop-refresh \
  PACKAGE_APP_ICONSET_SRC="/absolute/path/AppIcon.iconset"
```

or

```sh
make package-desktop-refresh \
  PACKAGE_APP_ICON_SRC="/absolute/path/AppIcon.icns"
```

If either variable is supplied, packaging will bundle `Contents/Resources/AppIcon.icns` and the app plist will advertise `CFBundleIconFile=AppIcon`.

Default local icon store:

- `map_forge/tools/packaging/macos/local_app_icon/AppIcon.icns`
- `map_forge/tools/packaging/macos/local_app_icon/AppIcon.iconset`

Plain `make -C map_forge package-desktop-refresh` and `package-desktop-self-test` now look in that local store first. The local icon store is gitignored so refreshed icon copies do not dirty the normal repo worktree.

Canonical local icon source:

- `/Users/<user>/Desktop/icns/carta.icns`
- sync into the packaging lane with:
  - `bin/sync_desktop_icns.sh carta`
  - `bin/sync_desktop_icns.sh --refresh carta`

Default desktop destination:

- `$(HOME)/Desktop/Carta.app`

You can override destination for local verification:

```sh
make package-desktop-copy-desktop DESKTOP_APP_DIR="$PWD/dist/_desktop_smoke/Carta.app"
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
- `/Users/calebsv/Desktop/Carta.app`

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
- region payloads are no longer bundled into the app package by default

## Launcher Runtime Model

The app's `CFBundleExecutable` is a small native Mach-O launcher so its
Developer ID signature survives ZIP transport. It invokes the packaged
`Contents/Resources/mapforge-launcher.sh`, which sets app-relative defaults
only when unset:

Package assembly stamps `CFBundleShortVersionString` from the committed
`VERSION`. Release validation must archive and re-extract the app before
trusting its signature; an in-place `codesign --verify` alone does not prove
the downloadable ZIP preserves the signature.
Mach-O helper tools are packaged under `Contents/Helpers`, not Resources, so
they are explicit nested code for Developer ID signing and notarization.

- `VK_RENDERER_SHADER_ROOT=<app>/Contents/Resources`
- `MAPFORGE_FONT_PRESET=ide`
- `MAPFORGE_IMPORT_TOOLS_DIR` selection order:
  1. explicit env override (`MAPFORGE_IMPORT_TOOLS_DIR`)
  2. bundled tools dir (`<app>/Contents/Helpers`) when tools exist
  3. development fallback (`$HOME/Desktop/CodeWork/map_forge/build/tools`)
- `MAPFORGE_REGIONS_DIR` selection order:
  1. explicit env override (`MAPFORGE_REGIONS_DIR`)
  2. bundle regions dir (`<app>/Contents/Resources/data/regions`) when non-empty
  3. development fallback (`$HOME/Desktop/CodeWork/map_forge/data/regions`) when available
  4. writable app-support fallback (`~/Library/Application Support/Carta/regions`)
  5. `${TMPDIR:-/tmp}/Carta/regions` if app-support creation fails

Launcher diagnostics:
- normal runs append logs to `~/Library/Logs/Carta/launcher.log`
- `--print-config` prints resolved launch roots without launching the app
- `--self-test` prints launch roots after validation checks
- packaged default backend is now `vulkan` via launcher (`MAPFORGE_RENDER_BACKEND=vulkan` unless explicitly overridden)

Recommended final validation before moving to next project:
1. `make -C map_forge package-desktop-self-test`
2. `/Users/calebsv/Desktop/Carta.app/Contents/MacOS/mapforge-launcher --print-config`
3. `open /Users/calebsv/Desktop/Carta.app`
4. `tail -n 120 ~/Library/Logs/Carta/launcher.log`

Note:
- a fresh clone will still need an `AppIcon.icns` copied into `tools/packaging/macos/local_app_icon/` before plain packaging picks it up, because that lane is intentionally ignored.

The launcher then switches cwd to `<app>/Contents/Resources` before executing `mapforge-bin` so relative runtime paths (`config/`, `assets/`, `data/runtime/`) resolve from the bundle.

Packaged shader layout:
- canonical copy: `<app>/Contents/Resources/vk_renderer/shaders/*`
- runtime mirror: `<app>/Contents/Resources/shaders/*`

The runtime mirror ensures Vulkan pipeline shader loads succeed from packaged cwd without relying on source-tree relative paths.
