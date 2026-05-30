#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_headless_route_bundle.XXXXXX")
INNER_JOB="$TMP_DIR/input/route_demo_seattle.json"
BUNDLE="$TMP_DIR/job.bundle.json"
OUT_DIR="$TMP_DIR/run"
PINS_PATH="$REPO_DIR/data/pins/examples/demo.seattle.pins.json"
REGIONS_DIR="$REPO_DIR/data/regions"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$TMP_DIR/input"

cat >"$INNER_JOB" <<EOF
{
  "version": 1,
  "type": "route_playback_render",
  "map_region": "seattle",
  "pins_file": "$PINS_PATH",
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
    "start_paused": false
  },
  "output": {
    "preview_png": true,
    "frames": false,
    "frame_format": "bmp",
    "video_manifest": false
  }
}
EOF

cat >"$BUNDLE" <<EOF
{
  "schema_family": "codework_job",
  "schema_variant": "headless_bundle_v1",
  "job_id": "mapforge-bundle-smoke-001",
  "program": "map_forge",
  "tool": {
    "name": "map_forge_headless",
    "version": "0.1.0",
    "target_os": "linux",
    "target_arch": "x86_64"
  },
  "scene_payload": {
    "schema_family": "map_forge_job",
    "schema_variant": "route_playback_render_v1",
    "path": "input/route_demo_seattle.json"
  },
  "run_config": {
    "schema_family": "map_forge_job",
    "schema_variant": "route_playback_render_v1",
    "path": "input/route_demo_seattle.json"
  },
  "outputs": {
    "root": ".",
    "report_path": "output/report.json",
    "logs_dir": ".",
    "artifacts_dir": "output/artifacts"
  },
  "metadata": {
    "title": "MapForge Bundle Smoke",
    "description": "Shared outer bundle smoke for map_forge.",
    "created_by": "codex",
    "created_at": "2026-05-22T00:00:00Z"
  }
}
EOF

MAPFORGE_REGIONS_DIR="$REGIONS_DIR" \
    "$BINARY" --headless --job "$BUNDLE" --out "$OUT_DIR"

ARTIFACT_DIR="$OUT_DIR/output/artifacts"

test -f "$OUT_DIR/job.json"
test -f "$OUT_DIR/job.request.json"
test -f "$OUT_DIR/output/report.json"
test -f "$ARTIFACT_DIR/command.txt"
test -f "$ARTIFACT_DIR/job.resolved.json"
test -f "$ARTIFACT_DIR/manifest.json"
test -f "$ARTIFACT_DIR/summary.md"
test -f "$ARTIFACT_DIR/playback_trace.json"
test -f "$ARTIFACT_DIR/preview.bmp"

grep -q '"schema_variant": "headless_bundle_v1"' "$OUT_DIR/job.json"
grep -q '"job_id": "mapforge-bundle-smoke-001"' "$OUT_DIR/job.json"
grep -q '"schema_family": "codework_job_report"' "$OUT_DIR/output/report.json"
grep -q '"state": "succeeded"' "$OUT_DIR/output/report.json"
grep -F -q '"pins_file"' "$OUT_DIR/job.request.json"
grep -F -q 'demo.seattle.pins.json' "$OUT_DIR/job.request.json"
grep -q '"status":"complete"' "$ARTIFACT_DIR/manifest.json"
grep -q 'Playback Trace: `playback_trace.json`' "$ARTIFACT_DIR/summary.md"

echo "headless route bundle smoke passed"
