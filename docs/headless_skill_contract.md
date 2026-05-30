# Carta Headless Saved-Pin Skill Contract

This document locks the first skill-facing command/output contract for `Carta` saved-pin headless runs.

## Current Decision

The first stable Codex/mobile-facing surface is a single wrapper:

```bash
./scripts/run_saved_pin_route_skill.sh
```

This wrapper sits above:

- `scripts/render_saved_pin_route.sh`
- `scripts/build_route_video.sh`

It owns:

- the narrow argument vocabulary a skill should use
- the mapping from friendly zoom language to current helper presets
- the machine-readable result surface

It does **not** replace the lower helpers. It standardizes how later skills should call them.

## Stable Input Contract

Required:

- `--region <region>`
- `--from <pin>`
- `--to <pin>`
- `--out <dir>`

Supported options:

- `--zoom-level close | balanced | wide`
- `--motion-profile responsive | balanced | cinematic`
- `--orientation-mode heading_up | north_up`
- `--render-mode map_route_marker | map_route | map_only`
- `--include-video true | false`
- `--include-frames true | false`
- `--write-job-copy <path>`
- `--write-result-copy <path>`

## Friendly Intent Mapping

For requests like:

- “show me the route from north pin to south pin at a very close zoom level”

the current mapping is:

- “very close zoom” -> `--zoom-level close`
- “normal/default zoom” -> `--zoom-level balanced`
- “wide/overview zoom” -> `--zoom-level wide`
- “snappy / realistic” -> `--motion-profile responsive`
- “default movement” -> `--motion-profile balanced`
- “smooth / cinematic / gliding” -> `--motion-profile cinematic`
- “follow heading” -> `--orientation-mode heading_up`
- “keep north up / no direction following” -> `--orientation-mode north_up`
- “make me a video” -> `--include-video true`
- “frames only” -> `--include-frames true --include-video false`

## Stable Output Contract

The wrapper prints newline-delimited key/value output:

```text
status=complete
schema=mapforge-saved-pin-skill-result/v1
run_dir=/abs/path/to/run
manifest=/abs/path/to/run/manifest.json
summary=/abs/path/to/run/summary.md
resolved_job=/abs/path/to/run/job.resolved.json
preview=/abs/path/to/run/preview.bmp
frames_dir=/abs/path/to/run/frames
video=/abs/path/to/run/video/route_preview.mp4
result_json=/abs/path/to/run/skill_result.json
```

The wrapper also writes:

- `<run_dir>/skill_result.json`

Schema:

- `schema`
  - `mapforge-saved-pin-skill-result/v1`
- `status`
- `region`
- `from_pin`
- `to_pin`
- `zoom_level`
- `motion_profile`
- `orientation_mode`
- `render_mode`
- `include_frames`
- `include_video`
- `run_dir`
- `manifest`
- `summary`
- `resolved_job`
- `preview`
- `frames_dir`
- `video`

## Example Commands

### Close Zoom Review

```bash
./scripts/run_saved_pin_route_skill.sh \
  --region seattle \
  --from north_pin \
  --to south_pin \
  --out runs/north_to_south_close \
  --zoom-level close

### Close Zoom With Cinematic Motion

```bash
./scripts/run_saved_pin_route_skill.sh \
  --region seattle \
  --from west_pin \
  --to north_pin \
  --out runs/west_to_north_close_cinematic \
  --zoom-level close \
  --motion-profile cinematic \
  --orientation-mode heading_up \
  --include-video true
```
```

### Wide Zoom With Video

```bash
./scripts/run_saved_pin_route_skill.sh \
  --region seattle \
  --from north_pin \
  --to south_pin \
  --out runs/north_to_south_wide_video \
  --zoom-level wide \
  --include-video true
```

## Current Skill Guidance

Later Codex/mobile skills should:

1. resolve the intended region and saved pin names/ids
2. choose one of:
   - `close`
   - `balanced`
   - `wide`
3. choose one of:
   - `responsive`
   - `balanced`
   - `cinematic`
4. choose one of:
   - `heading_up`
   - `north_up`
5. decide whether video is needed
6. call `run_saved_pin_route_skill.sh`
7. report back using:
   - `run_dir`
   - `preview`
   - `frames_dir`
   - `video`
   - `summary`

## Intentional Limits

This first contract does not yet cover:

- multi-pin sequences
- one-command publish/VPS staging
- email/report notifications
- natural-language parsing inside the script itself

Those stay in later lanes. The current goal is one small, stable command and one small, stable result schema.
