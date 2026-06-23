#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

mapforge_test_setup_tmp "mapforge_headless_route_bundle"
INNER_JOB="$TMP_DIR/input/route_demo_seattle.json"
BUNDLE="$TMP_DIR/job.bundle.json"
OUT_DIR="$TMP_DIR/run"
PINS_PATH="$MAPFORGE_TEST_REPO_DIR/data/pins/examples/demo.seattle.pins.json"
REGIONS_DIR="$MAPFORGE_TEST_REPO_DIR/data/regions"

trap mapforge_test_cleanup_tmp EXIT INT TERM

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
    "$MAPFORGE_TEST_BINARY" --headless --job "$BUNDLE" --out "$OUT_DIR"

ARTIFACT_DIR="$OUT_DIR/output/artifacts"

mapforge_test_assert_file "$OUT_DIR/job.json"
mapforge_test_assert_file "$OUT_DIR/job.request.json"
mapforge_test_assert_file "$OUT_DIR/output/report.json"
mapforge_test_assert_file "$ARTIFACT_DIR/command.txt"
mapforge_test_assert_file "$ARTIFACT_DIR/job.resolved.json"
mapforge_test_assert_file "$ARTIFACT_DIR/manifest.json"
mapforge_test_assert_file "$ARTIFACT_DIR/summary.md"
mapforge_test_assert_file "$ARTIFACT_DIR/playback_trace.json"
mapforge_test_assert_file "$ARTIFACT_DIR/preview.bmp"

mapforge_test_assert_grep '"schema_variant": "headless_bundle_v1"' "$OUT_DIR/job.json"
mapforge_test_assert_grep '"job_id": "mapforge-bundle-smoke-001"' "$OUT_DIR/job.json"
mapforge_test_assert_grep '"schema_family": "codework_job_report"' "$OUT_DIR/output/report.json"
mapforge_test_assert_grep '"state": "succeeded"' "$OUT_DIR/output/report.json"
mapforge_test_assert_fixed_grep '"pins_file"' "$OUT_DIR/job.request.json"
mapforge_test_assert_fixed_grep 'demo.seattle.pins.json' "$OUT_DIR/job.request.json"
mapforge_test_assert_grep '"status":"complete"' "$ARTIFACT_DIR/manifest.json"
mapforge_test_assert_grep 'Playback Trace: `playback_trace.json`' "$ARTIFACT_DIR/summary.md"

echo "headless route bundle smoke passed"
