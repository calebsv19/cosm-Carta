#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
JOB="jobs/examples/route_demo_seattle_frames.json"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_headless_frames.XXXXXX")
OUT_DIR="$TMP_DIR/run"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

(
    cd "$REPO_DIR"
    "$BINARY" --headless --job "$JOB" --out "$OUT_DIR"
)

test -f "$OUT_DIR/command.txt"
test -f "$OUT_DIR/job.resolved.json"
test -f "$OUT_DIR/manifest.json"
test -f "$OUT_DIR/summary.md"
test -f "$OUT_DIR/playback_trace.json"
test -f "$OUT_DIR/preview.bmp"
test -d "$OUT_DIR/frames"
test -f "$OUT_DIR/frames/frame_000001.bmp"
test -f "$OUT_DIR/frames/frame_000360.bmp"

FRAME_COUNT=$(find "$OUT_DIR/frames" -name 'frame_*.bmp' | wc -l | tr -d '[:space:]')
test "$FRAME_COUNT" = "360"

grep -q '"status":"complete"' "$OUT_DIR/manifest.json"
grep -q '"playback_trace":"playback_trace.json"' "$OUT_DIR/manifest.json"
grep -q '"preview":"preview.bmp"' "$OUT_DIR/manifest.json"
grep -q '"frames_dir":"frames' "$OUT_DIR/manifest.json"
grep -q '"pixel_scale":1' "$OUT_DIR/job.resolved.json"
grep -q '"quality_profile":"runtime"' "$OUT_DIR/job.resolved.json"
grep -q 'Playback Trace: `playback_trace.json`' "$OUT_DIR/summary.md"
grep -q 'Preview Image: `preview.bmp`' "$OUT_DIR/summary.md"
grep -q 'Frames: `frames/`' "$OUT_DIR/summary.md"

echo "headless route frames smoke passed"
