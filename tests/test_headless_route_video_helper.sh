#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
HELPER_RENDER="$REPO_DIR/scripts/render_saved_pin_route.sh"
HELPER_VIDEO="$REPO_DIR/scripts/build_route_video.sh"
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_headless_video_helper.XXXXXX")
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_DIR="$TMP_DIR/run"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "headless route video helper smoke skipped: ffmpeg not available"
    exit 0
fi

mkdir -p "$RUNTIME_DIR/pins"
cp "$REPO_DIR/data/pins/examples/demo.seattle.pins.json" "$RUNTIME_DIR/pins/seattle.pins.local.json"

MAPFORGE_BINARY="$BINARY" \
MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
/bin/sh "$HELPER_RENDER" \
    --region seattle \
    --from-pin demo_start \
    --to-pin demo_goal \
    --out "$OUT_DIR" \
    --preset frames

/bin/sh "$HELPER_VIDEO" "$OUT_DIR"

test -f "$OUT_DIR/video/ffmpeg_input.txt"
test -f "$OUT_DIR/video/route_preview.mp4"
grep -q "file '" "$OUT_DIR/video/ffmpeg_input.txt"

echo "headless route video helper smoke passed"
