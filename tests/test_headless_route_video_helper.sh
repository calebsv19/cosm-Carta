#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

HELPER_RENDER="$MAPFORGE_TEST_REPO_DIR/scripts/render_saved_pin_route.sh"
HELPER_VIDEO="$MAPFORGE_TEST_REPO_DIR/scripts/build_route_video.sh"
mapforge_test_setup_tmp "mapforge_headless_video_helper"
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_DIR="$TMP_DIR/run"

trap mapforge_test_cleanup_tmp EXIT INT TERM

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "headless route video helper smoke skipped: ffmpeg not available"
    exit 0
fi

mapforge_test_install_demo_pins "$RUNTIME_DIR" seattle

MAPFORGE_BINARY="$MAPFORGE_TEST_BINARY" \
MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$MAPFORGE_TEST_REPO_DIR/data/regions" \
/bin/sh "$HELPER_RENDER" \
    --region seattle \
    --from-pin demo_start \
    --to-pin demo_goal \
    --out "$OUT_DIR" \
    --preset frames

/bin/sh "$HELPER_VIDEO" "$OUT_DIR"

mapforge_test_assert_file "$OUT_DIR/video/ffmpeg_input.txt"
mapforge_test_assert_file "$OUT_DIR/video/route_preview.mp4"
mapforge_test_assert_grep "file '" "$OUT_DIR/video/ffmpeg_input.txt"

echo "headless route video helper smoke passed"
