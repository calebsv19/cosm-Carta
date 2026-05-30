#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SKILL_HELPER="$REPO_DIR/scripts/run_saved_pin_route_skill.sh"
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_saved_pin_skill.XXXXXX")
RUNTIME_DIR="$TMP_DIR/runtime"
OUT_DIR="$TMP_DIR/run"
STDOUT_CAPTURE="$TMP_DIR/stdout.txt"
RESULT_COPY="$TMP_DIR/result_copy.json"

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
    --out "$OUT_DIR" \
    --zoom-level close \
    --motion-profile cinematic \
    --orientation-mode north_up \
    --include-video true \
    --write-result-copy "$RESULT_COPY" \
    > "$STDOUT_CAPTURE"

test -f "$OUT_DIR/skill_result.json"
test -f "$RESULT_COPY"
test -f "$OUT_DIR/video/route_preview.mp4"

grep -q '^status=complete$' "$STDOUT_CAPTURE"
grep -q '^schema=mapforge-saved-pin-skill-result/v1$' "$STDOUT_CAPTURE"
grep -q '^run_dir='"$OUT_DIR"'$' "$STDOUT_CAPTURE"
grep -q '^zoom_level=close$' "$STDOUT_CAPTURE"
grep -q '^motion_profile=cinematic$' "$STDOUT_CAPTURE"
grep -q '^orientation_mode=north_up$' "$STDOUT_CAPTURE"
grep -q '^video='"$OUT_DIR"'/video/route_preview.mp4$' "$STDOUT_CAPTURE"
grep -Eq '"schema":[[:space:]]*"mapforge-saved-pin-skill-result/v1"' "$OUT_DIR/skill_result.json"
grep -Eq '"zoom_level":[[:space:]]*"close"' "$OUT_DIR/skill_result.json"
grep -Eq '"motion_profile":[[:space:]]*"cinematic"' "$OUT_DIR/skill_result.json"
grep -Eq '"orientation_mode":[[:space:]]*"north_up"' "$OUT_DIR/skill_result.json"
grep -Eq '"include_video":[[:space:]]*true' "$OUT_DIR/skill_result.json"
grep -Eq '"video":[[:space:]]*"'"$OUT_DIR"'/video/route_preview\.mp4"' "$OUT_DIR/skill_result.json"

echo "headless saved-pin skill contract smoke passed"
