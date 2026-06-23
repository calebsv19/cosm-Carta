#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

JOB="jobs/examples/route_demo_seattle_frames.json"
mapforge_test_setup_tmp "mapforge_headless_frames"
OUT_DIR="$TMP_DIR/run"

trap mapforge_test_cleanup_tmp EXIT INT TERM

(
    cd "$MAPFORGE_TEST_REPO_DIR"
    "$MAPFORGE_TEST_BINARY" --headless --job "$JOB" --out "$OUT_DIR"
)

mapforge_test_assert_file "$OUT_DIR/command.txt"
mapforge_test_assert_file "$OUT_DIR/job.resolved.json"
mapforge_test_assert_file "$OUT_DIR/manifest.json"
mapforge_test_assert_file "$OUT_DIR/summary.md"
mapforge_test_assert_file "$OUT_DIR/playback_trace.json"
mapforge_test_assert_file "$OUT_DIR/preview.bmp"
mapforge_test_assert_dir "$OUT_DIR/frames"
mapforge_test_assert_file "$OUT_DIR/frames/frame_000001.bmp"
mapforge_test_assert_file "$OUT_DIR/frames/frame_000360.bmp"

FRAME_COUNT=$(find "$OUT_DIR/frames" -name 'frame_*.bmp' | wc -l | tr -d '[:space:]')
test "$FRAME_COUNT" = "360"

mapforge_test_assert_grep '"status":"complete"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"playback_trace":"playback_trace.json"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"preview":"preview.bmp"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"frames_dir":"frames' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"pixel_scale":1' "$OUT_DIR/job.resolved.json"
mapforge_test_assert_grep '"quality_profile":"runtime"' "$OUT_DIR/job.resolved.json"
mapforge_test_assert_grep 'Playback Trace: `playback_trace.json`' "$OUT_DIR/summary.md"
mapforge_test_assert_grep 'Preview Image: `preview.bmp`' "$OUT_DIR/summary.md"
mapforge_test_assert_grep 'Frames: `frames/`' "$OUT_DIR/summary.md"

echo "headless route frames smoke passed"
