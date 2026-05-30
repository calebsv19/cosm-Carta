# Carta Headless Saved-Pin Usage

`Carta` can now render headless routes directly from the saved runtime/private pin store without requiring a committed example `pins_file`.

## Default Pin Source Contract

When a headless job omits `pins_file`, `Carta` resolves pins in this order:

1. `MAPFORGE_RUNTIME_DIR/pins/<region>.pins.local.json`
2. `data/pins/private/<region>.pins.local.json` as a legacy/dev-local fallback when runtime mode is active

Pin endpoint lookup prefers:

1. exact pin `id`
2. exact pin `name`

Ambiguous exact-name matches fail explicitly.

## Fastest Entry Path

Use the helper:

```bash
./scripts/render_saved_pin_route.sh \
  --region seattle \
  --from-pin demo_start \
  --to-pin demo_goal \
  --out runs/saved_pin_demo
```

The helper:

- generates a version `2` job request
- omits `pins_file` on purpose so runtime/private saved pins are used
- defaults `MAPFORGE_REGIONS_DIR` to `data/regions` when not already set
- preserves `MAPFORGE_RUNTIME_DIR` from the caller environment

## Presets

The helper currently supports:

- `balanced`
  - default preview-oriented follow camera
- `zoomed_in`
  - closer street-level framing
- `zoomed_out`
  - wider, smoother framing
- `frames`
  - same baseline camera with frame export enabled

## Motion Profiles

The helper also supports heading/motion profiles through `--motion-profile`:

- `responsive`
  - faster heading response
  - higher turn-rate cap
  - better for close zooms when you want a more literal path-following feel
- `balanced`
  - current default
- `cinematic`
  - slower heading response
  - lower turn-rate cap
  - better for close zooms when you want a smoother gliding camera

The helper also supports orientation through `--orientation-mode`:

- `heading_up`
  - rotate with the current route heading
- `north_up`
  - keep north facing upward while still following the entity position

Example:

```bash
MAPFORGE_RUNTIME_DIR="$HOME/Library/Application Support/Carta/runtime" \
./scripts/render_saved_pin_route.sh \
  --region seattle \
  --from-pin demo_start \
  --to-pin demo_goal \
  --out runs/seattle_saved_pin_frames \
  --preset frames
```

For a close zoom with smoother motion:

```bash
MAPFORGE_RUNTIME_DIR="$HOME/Library/Application Support/Carta/runtime" \
./scripts/render_saved_pin_route.sh \
  --region seattle_downtown \
  --from-pin West \
  --to-pin Nrth \
  --out runs/west_to_north_close_cinematic \
  --preset zoomed_in \
  --motion-profile cinematic \
  --orientation-mode north_up
```

## Output Artifacts

Current headless runs emit a self-contained folder with:

- `command.txt`
- `job.resolved.json`
- `manifest.json`
- `summary.md`
- `playback_trace.json` when playback duration and fps are both set
- `preview.bmp` when preview export is enabled
- `frames/` when frame export is enabled
- `headless_render_debug.json` when image export runs

## Current Visualizer-Friendly Path

The current best local review path is:

1. run a headless export into `runs/<name>/`
2. open `frames/` directly in DataLab when frame export is enabled
3. use `preview.bmp` for quick static verification
4. inspect `summary.md` and `manifest.json` for status, warnings, and artifact names

Video packaging is still a helper-layer concern. The core runtime remains responsible for deterministic frame/image output, not `ffmpeg` orchestration.

Current helper-layer artifact handoff details are in:

- `docs/headless_artifact_handoff.md`
