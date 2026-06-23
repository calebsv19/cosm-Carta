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

FAIL_OUT_DIR="$TMP_DIR/fail_run"
FAIL_STDOUT="$TMP_DIR/fail_stdout.txt"
FAIL_STDERR="$TMP_DIR/fail_stderr.txt"
if MAPFORGE_BINARY="$BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    /bin/sh "$SKILL_HELPER" \
        --region missing_wrapper_region \
        --from demo_start \
        --to demo_goal \
        --out "$FAIL_OUT_DIR" \
        > "$FAIL_STDOUT" 2> "$FAIL_STDERR"; then
    echo "expected saved-pin skill helper to fail for missing region" >&2
    exit 1
fi

test -f "$FAIL_OUT_DIR/manifest.json"
grep -q 'saved-pin wrapper failure: render_helper_failed' "$FAIL_STDERR"
grep -q 'run_dir='"$FAIL_OUT_DIR" "$FAIL_STDERR"
grep -q 'manifest='"$FAIL_OUT_DIR"'/manifest.json' "$FAIL_STDERR"
grep -q 'summary='"$FAIL_OUT_DIR"'/summary.md' "$FAIL_STDERR"
grep -q 'failure_stage=region_lookup' "$FAIL_STDERR"
grep -q 'failure_code=map_region_not_found' "$FAIL_STDERR"
if grep -q '^status=complete$' "$FAIL_STDOUT"; then
    echo "failure path should not print skill completion status" >&2
    exit 1
fi

FAIL_VIDEO_HELPER="$TMP_DIR/fail_video_helper.sh"
VIDEO_FAIL_OUT_DIR="$TMP_DIR/video_fail_run"
VIDEO_FAIL_STDOUT="$TMP_DIR/video_fail_stdout.txt"
VIDEO_FAIL_STDERR="$TMP_DIR/video_fail_stderr.txt"
cat > "$FAIL_VIDEO_HELPER" <<'EOF'
#!/bin/sh
echo "fake video helper failure" >&2
exit 1
EOF

if MAPFORGE_BINARY="$BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    MAPFORGE_VIDEO_HELPER="$FAIL_VIDEO_HELPER" \
    /bin/sh "$SKILL_HELPER" \
        --region seattle \
        --from demo_start \
        --to demo_goal \
        --out "$VIDEO_FAIL_OUT_DIR" \
        --include-video true \
        > "$VIDEO_FAIL_STDOUT" 2> "$VIDEO_FAIL_STDERR"; then
    echo "expected saved-pin skill helper to fail when video helper fails" >&2
    exit 1
fi

test -f "$VIDEO_FAIL_OUT_DIR/manifest.json"
grep -q 'fake video helper failure' "$VIDEO_FAIL_STDERR"
grep -q 'saved-pin wrapper failure: video_helper_failed' "$VIDEO_FAIL_STDERR"
grep -q 'run_dir='"$VIDEO_FAIL_OUT_DIR" "$VIDEO_FAIL_STDERR"
grep -q 'manifest='"$VIDEO_FAIL_OUT_DIR"'/manifest.json' "$VIDEO_FAIL_STDERR"
grep -q 'summary='"$VIDEO_FAIL_OUT_DIR"'/summary.md' "$VIDEO_FAIL_STDERR"
grep -q 'manifest_status=complete' "$VIDEO_FAIL_STDERR"
if grep -q '^status=complete$' "$VIDEO_FAIL_STDOUT"; then
    echo "video failure path should not print skill completion status" >&2
    exit 1
fi

BAD_VIDEO_HELPER_STDOUT="$TMP_DIR/bad_video_helper_stdout.txt"
BAD_VIDEO_HELPER_STDERR="$TMP_DIR/bad_video_helper_stderr.txt"
if MAPFORGE_BINARY="$BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    MAPFORGE_VIDEO_HELPER="$TMP_DIR/missing_video_helper.sh" \
    /bin/sh "$SKILL_HELPER" \
        --region seattle \
        --from demo_start \
        --to demo_goal \
        --out "$TMP_DIR/bad_video_helper_run" \
        --include-video true \
        > "$BAD_VIDEO_HELPER_STDOUT" 2> "$BAD_VIDEO_HELPER_STDERR"; then
    echo "expected saved-pin skill helper to reject missing MAPFORGE_VIDEO_HELPER override" >&2
    exit 1
fi
grep -q 'invalid trusted operator script for MAPFORGE_VIDEO_HELPER: missing_file' "$BAD_VIDEO_HELPER_STDERR"
grep -q 'request data must not control MAPFORGE_VIDEO_HELPER' "$BAD_VIDEO_HELPER_STDERR"
if grep -q '^status=complete$' "$BAD_VIDEO_HELPER_STDOUT"; then
    echo "bad video helper override failure should not print skill completion status" >&2
    exit 1
fi

echo "headless saved-pin skill contract smoke passed"
