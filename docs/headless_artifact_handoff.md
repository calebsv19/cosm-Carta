# Carta Headless Artifact Handoff

This document locks the current best path from a saved-pin headless run to local review and optional video packaging.

## Current Decision

For the current `MapForge` headless lane, helper scripts are the primary handoff surface.

That means:

1. `Carta` runtime is responsible for deterministic route, preview, frame, and manifest output.
2. helper scripts are responsible for:
   - turning saved pins into a job request
   - optional video packaging
   - later staging/publish flow
3. future Codex/mobile skills should wrap these helpers instead of reimplementing their policy in prompt text.

This keeps the C runtime small and stable while the surrounding automation contract is still changing.

## Ordered Local Flow

### 1. Render from Saved Pins

Use:

```bash
./scripts/render_saved_pin_route.sh \
  --region seattle \
  --from-pin demo_start \
  --to-pin demo_goal \
  --out runs/seattle_saved_pin_frames \
  --preset frames
```

For a smoother close zoom, add a motion profile:

```bash
./scripts/render_saved_pin_route.sh \
  --region seattle_downtown \
  --from-pin West \
  --to-pin Nrth \
  --out runs/west_to_north_close_cinematic \
  --preset zoomed_in \
  --motion-profile cinematic
```

For a north-up follow that keeps the entity centered without rotating the map:

```bash
./scripts/render_saved_pin_route.sh \
  --region seattle_downtown \
  --from-pin West \
  --to-pin Nrth \
  --out runs/west_to_north_close_northup \
  --preset zoomed_in \
  --motion-profile cinematic \
  --orientation-mode north_up
```

This produces a self-contained run directory with:

- `command.txt`
- `job.resolved.json`
- `manifest.json`
- `summary.md`
- `playback_trace.json`
- `preview.bmp`
- `frames/`
- `headless_render_debug.json`

### 2. Review Locally

Current best review path:

- quick static check:
  - open `preview.bmp`
- frame-by-frame review:
  - open `frames/` in DataLab
- contract/status review:
  - inspect `summary.md`
  - inspect `manifest.json`
  - inspect `job.resolved.json`

### 3. Build a Review Video

Use:

```bash
./scripts/build_route_video.sh runs/seattle_saved_pin_frames
```

This writes:

- `video/ffmpeg_input.txt`
- `video/route_preview.mp4`

The helper:

- uses `job.resolved.json` to derive FPS when available
- falls back to `30` FPS when the run metadata does not provide one
- expects `frames/frame_*.bmp` to exist

## Why This Is The Current Best Split

The current headless producer contract is already strong:

- deterministic pin resolution
- deterministic route computation
- deterministic frame/image export
- deterministic manifest/summary output

The unstable part is still the outer automation layer:

- how later Codex/mobile commands should package requests
- how video packaging should be exposed
- how staged drops should be shaped before publish

So the current best architecture is:

- C runtime owns producer truth
- shell helpers own automation truth
- later skills wrap shell helpers

## Current Boundaries

What is in scope now:

- saved-pin route export
- preview/frame review
- optional local MP4 packaging
- local visualizer-drop staging
- VPS staged-drop upload/import when the drop uses a compliant `visualizer-run/v1` id
- one-command helper orchestration across render + stage + upload

What is intentionally not in scope yet:

- email/report notifications
- skill/plugin orchestration

## Visualizer Drop Id Rule

When staging a website-facing drop, the `drop_id` and `run_id` must match this strict form:

```text
<program>--<job-type>--<YYYYMMDDTHHMMSSZ>--<alnumnonce>
```

Notes:

- the final nonce must be lowercase alphanumeric only
- hyphenated labels like `north-south-close` are rejected by the current importer
- example:
  - `map-forge--saved-pin-route--20260520T053500Z--nrthsuthclose`

## Next Boundary

The next lane should define the first stable skill-facing command/output contract around:

- saved-pin render request
- optional frame export
- optional video packaging
- stable output folder reporting
