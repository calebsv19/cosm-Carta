#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
HELPER="$REPO_DIR/scripts/render_saved_pin_route.sh"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_saved_pin_script.XXXXXX")
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_DIR="$TMP_DIR/run"
JOB_COPY="$TMP_DIR/generated_job.json"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

mkdir -p "$RUNTIME_DIR/pins"
cp "$REPO_DIR/data/pins/examples/demo.seattle.pins.json" "$RUNTIME_DIR/pins/seattle.pins.local.json"

MAPFORGE_BINARY="$BINARY" \
MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
/bin/sh "$HELPER" \
    --region seattle \
    --from-pin demo_start \
    --to-pin demo_goal \
    --out "$OUT_DIR" \
    --preset zoomed_out \
    --motion-profile cinematic \
    --orientation-mode north_up \
    --frames true \
    --write-job-copy "$JOB_COPY"

test -f "$JOB_COPY"
test -f "$OUT_DIR/manifest.json"
test -f "$OUT_DIR/summary.md"
test -f "$OUT_DIR/preview.bmp"
test -d "$OUT_DIR/frames"
test -f "$OUT_DIR/frames/frame_000001.bmp"

grep -q '"map_region":"seattle"' "$OUT_DIR/manifest.json"
grep -q '"from_pin":"demo_start"' "$OUT_DIR/manifest.json"
grep -q '"to_pin":"demo_goal"' "$OUT_DIR/manifest.json"
grep -q '"pins_file":"' "$OUT_DIR/job.resolved.json"
grep -q 'seattle.pins.local.json' "$OUT_DIR/job.resolved.json"
grep -Eq '"zoom":[[:space:]]*14\.0' "$JOB_COPY"
grep -Eq '"frames":[[:space:]]*true' "$JOB_COPY"
grep -Eq '"quality_profile":[[:space:]]*"final"' "$JOB_COPY"
grep -Eq '"pixel_scale":[[:space:]]*2' "$JOB_COPY"
grep -Eq '"stabilize_visible_zoom":[[:space:]]*true' "$JOB_COPY"
grep -Eq '"stabilize_tile_bands":[[:space:]]*true' "$JOB_COPY"
grep -Eq '"allow_tile_fallback":[[:space:]]*false' "$JOB_COPY"
grep -Eq '"simplify_route_screen_space":[[:space:]]*false' "$JOB_COPY"
grep -Eq '"smoothing_tau_seconds":[[:space:]]*0\.65' "$JOB_COPY"
grep -Eq '"max_turn_rate_deg_per_sec":[[:space:]]*45\.0' "$JOB_COPY"
grep -Eq '"rotate_with_heading":[[:space:]]*false' "$JOB_COPY"

echo "headless saved-pin helper smoke passed"
