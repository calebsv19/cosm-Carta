#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

WRAPPER="$MAPFORGE_TEST_REPO_DIR/scripts/render_saved_pin_visualizer_publish.sh"
mapforge_test_setup_tmp "mapforge_visualizer_publish_plan"
trap mapforge_test_cleanup_tmp EXIT INT TERM

PLAN_ROOT="$TMP_DIR/plan_out"
PLAN_STAGING_ROOT="$TMP_DIR/plan_staged"
PLAN_RESULT="$TMP_DIR/result/heading_up_plan.json"
PLAN_STDOUT="$TMP_DIR/heading_up_stdout.txt"

MAPFORGE_BINARY="$TMP_DIR/missing-mapforge-binary" \
MAPFORGE_STAGE_TOOL="$TMP_DIR/missing-stage-tool.py" \
MAPFORGE_UPLOAD_HELPER="$TMP_DIR/missing-upload-helper.sh" \
/bin/sh "$WRAPPER" \
    --region seattle \
    --from "Demo Start" \
    --to "Demo Goal" \
    --out-root "$PLAN_ROOT" \
    --zoom-level wide \
    --motion-profile responsive \
    --orientation-mode heading_up \
    --include-video false \
    --publish false \
    --plan-only true \
    --staging-root "$PLAN_STAGING_ROOT" \
    --site-base-url https://visualizer.example.test \
    --drop-timestamp 20260520T090000Z \
    --write-result-json "$PLAN_RESULT" \
    > "$PLAN_STDOUT"

mapforge_test_assert_file "$PLAN_RESULT"
mapforge_test_assert_grep '^status=complete$' "$PLAN_STDOUT"
mapforge_test_assert_grep '^schema=mapforge-saved-pin-visualizer-publish-result/v1$' "$PLAN_STDOUT"
mapforge_test_assert_grep '^run_count=1$' "$PLAN_STDOUT"
mapforge_test_assert_grep '^run_1_orientation=heading_up$' "$PLAN_STDOUT"
mapforge_test_assert_grep '^run_1_drop_id=map-forge--saved-pin-route--20260520T090000Z--demostartdemogoalwideresponsivehu$' "$PLAN_STDOUT"
mapforge_test_assert_grep '^run_1_preview_url=https://visualizer.example.test/artifacts/map-forge/map-forge--saved-pin-route--20260520T090000Z--demostartdemogoalwideresponsivehu/preview/preview.png$' "$PLAN_STDOUT"
mapforge_test_assert_grep '^run_1_video_url=$' "$PLAN_STDOUT"
mapforge_test_assert_egrep '"published":[[:space:]]*false' "$PLAN_RESULT"
mapforge_test_assert_egrep '"run_count":[[:space:]]*1' "$PLAN_RESULT"
mapforge_test_assert_egrep '"include_video":[[:space:]]*false' "$PLAN_RESULT"
mapforge_test_assert_egrep '"video":[[:space:]]*""' "$PLAN_RESULT"
mapforge_test_assert_egrep '"video_url":[[:space:]]*""' "$PLAN_RESULT"
if [ -d "$PLAN_STAGING_ROOT" ]; then
    echo "plan-only mode should not create staged drop directories" >&2
    exit 1
fi

BOTH_ROOT="$TMP_DIR/both_out"
BOTH_STAGING_ROOT="$TMP_DIR/both_staged"
BOTH_RESULT="$TMP_DIR/result/both_plan.json"
BOTH_STDOUT="$TMP_DIR/both_stdout.txt"

MAPFORGE_BINARY="$TMP_DIR/missing-mapforge-binary" \
MAPFORGE_STAGE_TOOL="$TMP_DIR/missing-stage-tool.py" \
MAPFORGE_UPLOAD_HELPER="$TMP_DIR/missing-upload-helper.sh" \
/bin/sh "$WRAPPER" \
    --region seattle \
    --from demo_start \
    --to demo_goal \
    --out-root "$BOTH_ROOT" \
    --zoom-level close \
    --motion-profile cinematic \
    --orientation-mode both \
    --include-video true \
    --publish false \
    --plan-only true \
    --staging-root "$BOTH_STAGING_ROOT" \
    --drop-timestamp 20260520T100000Z \
    --write-result-json "$BOTH_RESULT" \
    > "$BOTH_STDOUT"

mapforge_test_assert_file "$BOTH_RESULT"
mapforge_test_assert_grep '^run_count=2$' "$BOTH_STDOUT"
mapforge_test_assert_grep '^run_1_orientation=heading_up$' "$BOTH_STDOUT"
mapforge_test_assert_grep '^run_2_orientation=north_up$' "$BOTH_STDOUT"
mapforge_test_assert_grep '^run_1_drop_id=map-forge--saved-pin-route--20260520T100000Z--demostartdemogoalclosecinematichu$' "$BOTH_STDOUT"
mapforge_test_assert_grep '^run_2_drop_id=map-forge--saved-pin-route--20260520T100000Z--demostartdemogoalclosecinematicnu$' "$BOTH_STDOUT"
mapforge_test_assert_grep '^run_1_video_url=/artifacts/map-forge/map-forge--saved-pin-route--20260520T100000Z--demostartdemogoalclosecinematichu/outputs/final/route_preview.mp4$' "$BOTH_STDOUT"
mapforge_test_assert_grep '^run_2_video_url=/artifacts/map-forge/map-forge--saved-pin-route--20260520T100000Z--demostartdemogoalclosecinematicnu/outputs/final/route_preview.mp4$' "$BOTH_STDOUT"
mapforge_test_assert_egrep '"orientation_mode":[[:space:]]*"both"' "$BOTH_RESULT"
mapforge_test_assert_egrep '"run_count":[[:space:]]*2' "$BOTH_RESULT"
mapforge_test_assert_egrep '"run_dir":[[:space:]]*".*/both_out/heading_up"' "$BOTH_RESULT"
mapforge_test_assert_egrep '"run_dir":[[:space:]]*".*/both_out/north_up"' "$BOTH_RESULT"
mapforge_test_assert_egrep '"video":[[:space:]]*".*/both_out/heading_up/video/route_preview.mp4"' "$BOTH_RESULT"
mapforge_test_assert_egrep '"video":[[:space:]]*".*/both_out/north_up/video/route_preview.mp4"' "$BOTH_RESULT"
if [ -d "$BOTH_STAGING_ROOT" ]; then
    echo "plan-only mode should not create staged drop directories for both orientations" >&2
    exit 1
fi

BAD_STDOUT="$TMP_DIR/bad_stdout.txt"
BAD_STDERR="$TMP_DIR/bad_stderr.txt"
if /bin/sh "$WRAPPER" \
    --region seattle \
    --from demo_start \
    --to demo_goal \
    --out-root "$TMP_DIR/bad_out" \
    --staging-root "$TMP_DIR/bad_staged" \
    --publish true \
    --plan-only true \
    > "$BAD_STDOUT" 2> "$BAD_STDERR"; then
    echo "expected plan-only publish=true request to fail" >&2
    exit 1
fi
mapforge_test_assert_grep 'requires --publish false' "$BAD_STDERR"
if grep -q '^status=complete$' "$BAD_STDOUT"; then
    echo "invalid plan-only publish request should not print complete status" >&2
    exit 1
fi

echo "headless saved-pin visualizer publish plan probe passed"
