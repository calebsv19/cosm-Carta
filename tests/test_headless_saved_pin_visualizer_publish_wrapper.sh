#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

WRAPPER="$MAPFORGE_TEST_REPO_DIR/scripts/render_saved_pin_visualizer_publish.sh"
mapforge_test_setup_tmp "mapforge_visualizer_publish_wrapper"
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_ROOT="$TMP_DIR/out"
STAGING_ROOT="$TMP_DIR/staged"
RESULT_COPY="$TMP_DIR/result/nested/publish_result.json"
STDOUT_CAPTURE="$TMP_DIR/stdout.txt"

trap mapforge_test_cleanup_tmp EXIT INT TERM

mapforge_test_install_demo_pins "$RUNTIME_DIR" seattle

MAPFORGE_BINARY="$MAPFORGE_TEST_BINARY" \
MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$MAPFORGE_TEST_REPO_DIR/data/regions" \
/bin/sh "$WRAPPER" \
    --region seattle \
    --from demo_start \
    --to demo_goal \
    --out-root "$OUT_ROOT" \
    --zoom-level close \
    --motion-profile cinematic \
    --orientation-mode both \
    --include-video true \
    --publish false \
    --staging-root "$STAGING_ROOT" \
    --drop-timestamp 20260520T070000Z \
    --write-result-json "$RESULT_COPY" \
    > "$STDOUT_CAPTURE"

mapforge_test_assert_file "$RESULT_COPY"
mapforge_test_assert_dir "$OUT_ROOT/heading_up"
mapforge_test_assert_dir "$OUT_ROOT/north_up"
mapforge_test_assert_file "$OUT_ROOT/heading_up/video/route_preview.mp4"
mapforge_test_assert_file "$OUT_ROOT/north_up/video/route_preview.mp4"
mapforge_test_assert_dir "$STAGING_ROOT/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematichu"
mapforge_test_assert_dir "$STAGING_ROOT/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematicnu"

mapforge_test_assert_grep '^status=complete$' "$STDOUT_CAPTURE"
mapforge_test_assert_grep '^schema=mapforge-saved-pin-visualizer-publish-result/v1$' "$STDOUT_CAPTURE"
mapforge_test_assert_egrep '^run_count=2$' "$STDOUT_CAPTURE"
mapforge_test_assert_egrep '^run_1_orientation=heading_up$' "$STDOUT_CAPTURE"
mapforge_test_assert_egrep '^run_2_orientation=north_up$' "$STDOUT_CAPTURE"
mapforge_test_assert_egrep '^run_1_video_url=/artifacts/map-forge/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematichu/outputs/final/route_preview.mp4$' "$STDOUT_CAPTURE"
mapforge_test_assert_egrep '^run_2_video_url=/artifacts/map-forge/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematicnu/outputs/final/route_preview.mp4$' "$STDOUT_CAPTURE"
mapforge_test_assert_egrep '"schema":[[:space:]]*"mapforge-saved-pin-visualizer-publish-result/v1"' "$RESULT_COPY"
mapforge_test_assert_egrep '"orientation_mode":[[:space:]]*"both"' "$RESULT_COPY"
mapforge_test_assert_egrep '"motion_profile":[[:space:]]*"cinematic"' "$RESULT_COPY"
mapforge_test_assert_egrep '"drop_id":[[:space:]]*"map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematichu"' "$RESULT_COPY"
mapforge_test_assert_egrep '"drop_id":[[:space:]]*"map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematicnu"' "$RESULT_COPY"
mapforge_test_assert_egrep '"published":[[:space:]]*false' "$RESULT_COPY"

FAIL_OUT_ROOT="$TMP_DIR/fail_out"
FAIL_STAGING_ROOT="$TMP_DIR/fail_staged"
FAIL_STDOUT="$TMP_DIR/fail_stdout.txt"
FAIL_STDERR="$TMP_DIR/fail_stderr.txt"
if MAPFORGE_BINARY="$MAPFORGE_TEST_BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$MAPFORGE_TEST_REPO_DIR/data/regions" \
    MAPFORGE_STAGE_TOOL="$TMP_DIR/missing_stage_tool.py" \
    /bin/sh "$WRAPPER" \
        --region seattle \
        --from demo_start \
        --to demo_goal \
        --out-root "$FAIL_OUT_ROOT" \
        --zoom-level close \
        --motion-profile cinematic \
        --orientation-mode heading_up \
        --include-video true \
        --publish false \
        --staging-root "$FAIL_STAGING_ROOT" \
        --drop-timestamp 20260520T080000Z \
        > "$FAIL_STDOUT" 2> "$FAIL_STDERR"; then
    echo "expected visualizer publish wrapper to fail when stage helper fails" >&2
    exit 1
fi
mapforge_test_assert_grep 'visualizer wrapper failure: stage_helper_failed' "$FAIL_STDERR"
mapforge_test_assert_grep 'run_dir='"$FAIL_OUT_ROOT" "$FAIL_STDERR"
mapforge_test_assert_grep 'drop_dir='"$FAIL_STAGING_ROOT"'/map-forge--saved-pin-route--20260520T080000Z--demostartdemogoalclosecinematichu' "$FAIL_STDERR"
mapforge_test_assert_grep 'manifest='"$FAIL_OUT_ROOT"'/manifest.json' "$FAIL_STDERR"
mapforge_test_assert_grep 'preview='"$FAIL_OUT_ROOT"'/preview.bmp' "$FAIL_STDERR"
mapforge_test_assert_grep 'log_source='"$FAIL_STAGING_ROOT"'/map-forge--saved-pin-route--20260520T080000Z--demostartdemogoalclosecinematichu/logs/run.log' "$FAIL_STDERR"
mapforge_test_assert_grep 'invalid trusted operator script for MAPFORGE_STAGE_TOOL: missing_file' "$FAIL_STDERR"
mapforge_test_assert_grep 'request data must not control MAPFORGE_STAGE_TOOL' "$FAIL_STDERR"
if grep -q '^status=complete$' "$FAIL_STDOUT"; then
    echo "visualizer publish failure should not print complete status" >&2
    exit 1
fi

BAD_OUT_STDOUT="$TMP_DIR/bad_out_stdout.txt"
BAD_OUT_STDERR="$TMP_DIR/bad_out_stderr.txt"
if MAPFORGE_BINARY="$MAPFORGE_TEST_BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$MAPFORGE_TEST_REPO_DIR/data/regions" \
    /bin/sh "$WRAPPER" \
        --region seattle \
        --from demo_start \
        --to demo_goal \
        --out-root / \
        --publish false \
        --staging-root "$STAGING_ROOT" \
        > "$BAD_OUT_STDOUT" 2> "$BAD_OUT_STDERR"; then
    echo "expected visualizer publish wrapper to reject root out-root" >&2
    exit 1
fi
mapforge_test_assert_grep 'invalid local artifact path for --out-root: root' "$BAD_OUT_STDERR"
if grep -q '^status=complete$' "$BAD_OUT_STDOUT"; then
    echo "unsafe out-root failure should not print complete status" >&2
    exit 1
fi

echo "headless saved-pin visualizer publish wrapper smoke passed"
