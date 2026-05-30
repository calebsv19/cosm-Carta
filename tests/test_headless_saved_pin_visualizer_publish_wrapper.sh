#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
WRAPPER="$REPO_DIR/scripts/render_saved_pin_visualizer_publish.sh"
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_visualizer_publish_wrapper.XXXXXX")
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_ROOT="$TMP_DIR/out"
STAGING_ROOT="$TMP_DIR/staged"
RESULT_COPY="$TMP_DIR/publish_result.json"
STDOUT_CAPTURE="$TMP_DIR/stdout.txt"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$RUNTIME_DIR/pins"
cp "$REPO_DIR/data/pins/examples/demo.seattle.pins.json" "$RUNTIME_DIR/pins/seattle.pins.local.json"

MAPFORGE_BINARY="$BINARY" \
MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
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

test -f "$RESULT_COPY"
test -d "$OUT_ROOT/heading_up"
test -d "$OUT_ROOT/north_up"
test -f "$OUT_ROOT/heading_up/video/route_preview.mp4"
test -f "$OUT_ROOT/north_up/video/route_preview.mp4"
test -d "$STAGING_ROOT/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematichu"
test -d "$STAGING_ROOT/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematicnu"

grep -q '^status=complete$' "$STDOUT_CAPTURE"
grep -q '^schema=mapforge-saved-pin-visualizer-publish-result/v1$' "$STDOUT_CAPTURE"
grep -Eq '^run_count=2$' "$STDOUT_CAPTURE"
grep -Eq '^run_1_orientation=heading_up$' "$STDOUT_CAPTURE"
grep -Eq '^run_2_orientation=north_up$' "$STDOUT_CAPTURE"
grep -Eq '^run_1_video_url=/artifacts/map-forge/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematichu/outputs/final/route_preview.mp4$' "$STDOUT_CAPTURE"
grep -Eq '^run_2_video_url=/artifacts/map-forge/map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematicnu/outputs/final/route_preview.mp4$' "$STDOUT_CAPTURE"
grep -Eq '"schema":[[:space:]]*"mapforge-saved-pin-visualizer-publish-result/v1"' "$RESULT_COPY"
grep -Eq '"orientation_mode":[[:space:]]*"both"' "$RESULT_COPY"
grep -Eq '"motion_profile":[[:space:]]*"cinematic"' "$RESULT_COPY"
grep -Eq '"drop_id":[[:space:]]*"map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematichu"' "$RESULT_COPY"
grep -Eq '"drop_id":[[:space:]]*"map-forge--saved-pin-route--20260520T070000Z--demostartdemogoalclosecinematicnu"' "$RESULT_COPY"
grep -Eq '"published":[[:space:]]*false' "$RESULT_COPY"

echo "headless saved-pin visualizer publish wrapper smoke passed"
