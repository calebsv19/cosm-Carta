# Carta Route Job Format

`Carta` headless mode consumes a route-job JSON file.

## Version

- current version: `1`
- next supported version: `2`
- current type: `route_playback_render`

## Shape

```json
{
  "version": 2,
  "type": "route_playback_render",
  "map_region": "seattle",
  "pins_file": "data/pins/examples/demo.seattle.pins.json",
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
    "render_mode": "map_route_marker"
  }
}
```

## Required Fields

- `version`
- `type`
- `map_region`
- `pins_file`
- `from_pin`
- `to_pin`

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

The current headless image lane now applies follow-camera playback framing and boots the region tile source for export, but it still does not fully match the interactive renderer or packaged video output path.

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
