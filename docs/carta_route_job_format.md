# Carta Route Job Format

`Carta` headless mode consumes a route-job JSON file.

## Version

- current supported versions: `1`, `2`
- current type: `route_playback_render`

## Shape

```json
{
  "version": 2,
  "type": "route_playback_render",
  "map_region": "seattle",
  "from_pin": "demo_start",
  "to_pin": "demo_goal",
  "route": {
    "mode": "walking"
  },
  "camera": {
    "width": 1280,
    "height": 720,
    "zoom": 15,
    "follow_route": true,
    "rotate_with_heading": true
  },
  "playback": {
    "duration_seconds": 12,
    "fps": 30,
    "start_paused": false,
    "heading": {
      "mode": "blended",
      "smoothing_tau_seconds": 0.35,
      "lookahead_seconds": 2.0,
      "measurement_window_seconds": 3.0,
      "max_turn_rate_deg_per_sec": 90.0
    }
  },
  "output": {
    "preview": true,
    "frames": false,
    "frame_format": "bmp",
    "video_manifest": false,
    "render_mode": "map_route_marker",
    "quality_profile": "final",
    "pixel_scale": 2,
    "stabilize_visible_zoom": true,
    "stabilize_tile_bands": true,
    "allow_tile_fallback": false,
    "simplify_route_screen_space": false
  }
}
```

## Required Fields

- `version`
- `type`
- `map_region`
- `from_pin`
- `to_pin`

## Pin Source Resolution

- `pins_file`
  - optional explicit override
  - when set, the job loads pins from that exact path after resolving it relative to the job file
- omitted `pins_file`
  - headless uses the same region-local/private pin policy as the interactive app
  - default runtime/private path: `MAPFORGE_RUNTIME_DIR/pins/<region>.pins.local.json`
  - local/dev fallback path: `data/pins/private/<region>.pins.local.json`
  - when `MAPFORGE_RUNTIME_DIR` is active and only the legacy local/dev file exists, headless loads it and reports a warning

Pin endpoint lookup order:

- exact pin `id`
- exact pin `name`
- ambiguous exact-name matches fail explicitly instead of silently picking one row

## Supported Fields In The Foundation Patch

- `route.mode`
  - `walking`
  - `walk`
  - `car`
  - `driving`
- `camera.*`
  - parsed and recorded
  - applied for output width/height, requested zoom, follow-route centering, and heading-up preview rotation
- `playback.*`
  - parsed and recorded
  - when both `playback.duration_seconds` and `playback.fps` are set, the headless runner emits a deterministic `playback_trace.json`
  - `playback.heading.*`
  - version `2` adds heading controls for `mode`, `smoothing_tau_seconds`, `lookahead_seconds`, `measurement_window_seconds`, and `max_turn_rate_deg_per_sec`
- `output.*`
  - parsed and recorded
  - version `2` accepts `preview` as the preferred alias for `preview_png`
  - `render_mode`
    - `map_route_marker`
    - `map_route`
    - `map_only`
  - `quality_profile`
    - `runtime`
    - `final`
  - `pixel_scale`
    - integer internal supersample factor
    - `1` preserves the legacy output size
    - `2` or higher renders at a larger internal resolution and downsamples back to the requested output size
  - `stabilize_visible_zoom`
    - locks the export tile zoom after the first frame so minor camera motion does not churn the layer zoom choice
  - `stabilize_tile_bands`
    - locks each layer's effective export band after the first frame
  - `allow_tile_fallback`
    - when `false`, export refuses presenter fallback during draw and uses only the exact chosen band/coord tiles
  - `simplify_route_screen_space`
    - when `false`, route rendering skips the screen-space point simplifier to avoid per-frame line shimmer
  - `preview_png: true` currently emits a headless `preview.bmp`
  - `frames: true` is supported only when both `playback.duration_seconds` and `playback.fps` are set
  - only `frame_format: "bmp"` is supported in the current slice
  - `video_manifest` is still deferred

## Current Output Contract

Running:

```bash
./mapforge --headless --job jobs/examples/route_demo_seattle.json --out runs/route_demo_seattle/
```

produces:

- `command.txt`
- `job.resolved.json`
- `manifest.json`
- `summary.md`
- `playback_trace.json` when both `playback.duration_seconds` and `playback.fps` are set
- `preview.bmp` when `output.preview_png` is `true`
- `frames/` with `frame_000001.bmp` style outputs when `output.frames` is `true` and playback settings are present

The current headless image lane now applies follow-camera playback framing, supports supersampled deterministic downsampling, and can lock export zoom/bands for more stable route imagery. It still remains a separate offscreen export lane rather than a full interactive renderer capture path.

For direct saved-pin runs that intentionally omit `pins_file`, see:

- `docs/headless_saved_pin_usage.md`

## Committed Example Jobs

- `jobs/examples/route_demo_seattle.json`
  - preview-only headless export
- `jobs/examples/route_demo_seattle_frames.json`
  - preview plus deterministic 360-frame BMP sequence at `12s * 30fps`
- `jobs/examples/route_demo_seattle_cinematic_v2.json`
  - balanced preset for general follow-camera playback
- `jobs/examples/route_demo_seattle_zoomed_in_v2.json`
  - close-follow preset with tighter zoom and faster heading response
- `jobs/examples/route_demo_seattle_zoomed_out_v2.json`
  - wide-view preset with slower heading response and lower turn-rate cap

## Suggested Presets

These presets are tuned around zoom level rather than a single global “smoothness” value.

- `zoomed_in`
  - use a higher zoom like `16.0-17.0`
  - use a lower `smoothing_tau_seconds`
  - use shorter `lookahead_seconds`
  - use a higher `max_turn_rate_deg_per_sec`
  - result: more local, reactive, street-level motion

- `balanced`
  - use a mid zoom like `15.0-16.0`
  - moderate smoothing and turn-rate cap
  - result: readable heading changes without obvious snap-turns

- `zoomed_out`
  - use a wider zoom like `13.5-14.5`
  - use a higher `smoothing_tau_seconds`
  - use longer `lookahead_seconds`
  - use a lower `max_turn_rate_deg_per_sec`
  - result: smoother large-scale directional drift with less frame-to-frame twitch
