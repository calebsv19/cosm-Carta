#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_headless_runtime_pins.XXXXXX")
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_DIR="$TMP_DIR/run"
JOB="$TMP_DIR/runtime_pins_job.json"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$RUNTIME_DIR/pins"
cp "$REPO_DIR/data/pins/examples/demo.seattle.pins.json" "$RUNTIME_DIR/pins/seattle.pins.local.json"

cat > "$JOB" <<'EOF'
{
  "version": 2,
  "type": "route_playback_render",
  "map_region": "seattle",
  "from_pin": "demo_start",
  "to_pin": "demo_goal",
  "route": {
    "mode": "walking"
  },
  "playback": {
    "duration_seconds": 12,
    "fps": 30
  },
  "output": {
    "preview": true,
    "frames": false,
    "frame_format": "bmp",
    "video_manifest": false,
    "render_mode": "map_route_marker"
  }
}
EOF

MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
"$BINARY" --headless --job "$JOB" --out "$OUT_DIR"

test -f "$OUT_DIR/manifest.json"
test -f "$OUT_DIR/summary.md"
test -f "$OUT_DIR/preview.bmp"

grep -q '"from_pin":"demo_start"' "$OUT_DIR/manifest.json"
grep -q '"to_pin":"demo_goal"' "$OUT_DIR/manifest.json"
grep -q '"pins_file":"' "$OUT_DIR/job.resolved.json"
grep -q 'seattle.pins.local.json' "$OUT_DIR/job.resolved.json"
grep -q 'job.pins_file was omitted' "$OUT_DIR/summary.md"

echo "headless runtime pins smoke passed"
