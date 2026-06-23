#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SKILL_HELPER="$REPO_DIR/scripts/run_saved_pin_route_skill.sh"
STAGE_HELPER="$REPO_DIR/scripts/stage_saved_pin_visualizer_drop.sh"
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_visualizer_drop_stage.XXXXXX")
RUNTIME_DIR="$TMP_DIR/runtime"
RUN_DIR="$TMP_DIR/run"
STAGING_ROOT="$TMP_DIR/staged"
DROP_ID="map-forge--saved-pin-route--20260520T000000Z--smoke"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$RUNTIME_DIR/pins"
cp "$REPO_DIR/data/pins/examples/demo.seattle.pins.json" "$RUNTIME_DIR/pins/seattle.pins.local.json"

MAPFORGE_BINARY="$BINARY" \
MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
/bin/sh "$SKILL_HELPER" \
    --region seattle \
    --from demo_start \
    --to demo_goal \
    --out "$RUN_DIR" \
    --zoom-level wide \
    --include-video true \
    > /dev/null

STAGE_JSON="$TMP_DIR/stage_result.json"
/bin/sh "$STAGE_HELPER" \
    --run-dir "$RUN_DIR" \
    --drop-id "$DROP_ID" \
    --staging-root "$STAGING_ROOT" \
    --write-ready \
    --overwrite \
    > "$STAGE_JSON"

DROP_DIR="$STAGING_ROOT/$DROP_ID"
test -f "$DROP_DIR/manifest.json"
test -f "$DROP_DIR/SHA256SUMS"
test -f "$DROP_DIR/READY"
test -f "$DROP_DIR/preview/preview.png"
test -f "$DROP_DIR/logs/run.log"
test -f "$DROP_DIR/outputs/final/route_preview.mp4"
test -f "$DROP_DIR/outputs/metadata/manifest.json"
test -f "$DROP_DIR/outputs/metadata/summary.md"
test -f "$DROP_DIR/outputs/metadata/job.resolved.json"
test -f "$DROP_DIR/outputs/metadata/skill_result.json"

grep -q '"schema_version": "visualizer-run/v1"' "$DROP_DIR/manifest.json"
grep -q '"program": "map-forge"' "$DROP_DIR/manifest.json"
grep -q '"job_type": "saved-pin-route"' "$DROP_DIR/manifest.json"
grep -q '"primary_output_relpath": "outputs/final/route_preview.mp4"' "$DROP_DIR/manifest.json"
grep -q '"preview_relpath": "preview/preview.png"' "$DROP_DIR/manifest.json"
grep -q 'outputs/final/route_preview.mp4' "$DROP_DIR/SHA256SUMS"
grep -q '"drop_id": "'"$DROP_ID"'"' "$STAGE_JSON"

FAIL_STDERR="$TMP_DIR/stage_fail_stderr.txt"
if /bin/sh "$STAGE_HELPER" \
    --run-dir "$TMP_DIR/missing_run" \
    --drop-id "map-forge--saved-pin-route--20260520T000000Z--missing" \
    --staging-root "$STAGING_ROOT" \
    2> "$FAIL_STDERR"; then
    echo "expected visualizer stage helper to fail for missing run dir" >&2
    exit 1
fi
grep -q 'visualizer wrapper failure: missing_run_dir' "$FAIL_STDERR"
grep -q 'run_dir='"$TMP_DIR"'/missing_run' "$FAIL_STDERR"
grep -q 'drop_dir='"$STAGING_ROOT"'/map-forge--saved-pin-route--20260520T000000Z--missing' "$FAIL_STDERR"
grep -q 'manifest='"$TMP_DIR"'/missing_run/manifest.json' "$FAIL_STDERR"
grep -q 'preview='"$TMP_DIR"'/missing_run/preview.bmp' "$FAIL_STDERR"

BAD_ROOT_STDERR="$TMP_DIR/bad_root_stderr.txt"
if /bin/sh "$STAGE_HELPER" \
    --run-dir "$RUN_DIR" \
    --drop-id "map-forge--saved-pin-route--20260520T000000Z--badroot" \
    --staging-root "$TMP_DIR/../bad_staged" \
    > /dev/null 2> "$BAD_ROOT_STDERR"; then
    echo "expected visualizer stage helper to reject parent traversal staging root" >&2
    exit 1
fi
grep -q 'invalid local artifact path for --staging-root: parent_traversal' "$BAD_ROOT_STDERR"

echo "headless visualizer drop stage smoke passed"
