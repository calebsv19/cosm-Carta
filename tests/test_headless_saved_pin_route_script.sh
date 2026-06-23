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

FAIL_OUT_DIR="$TMP_DIR/fail_run"
FAIL_STDOUT="$TMP_DIR/fail_stdout.txt"
FAIL_STDERR="$TMP_DIR/fail_stderr.txt"
if MAPFORGE_BINARY="$BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    /bin/sh "$HELPER" \
        --region missing_wrapper_region \
        --from-pin demo_start \
        --to-pin demo_goal \
        --out "$FAIL_OUT_DIR" \
        > "$FAIL_STDOUT" 2> "$FAIL_STDERR"; then
    echo "expected saved-pin helper to fail for missing region" >&2
    exit 1
fi

test -f "$FAIL_OUT_DIR/manifest.json"
grep -q 'saved-pin wrapper failure: binary_failed' "$FAIL_STDERR"
grep -q 'run_dir='"$FAIL_OUT_DIR" "$FAIL_STDERR"
grep -q 'manifest='"$FAIL_OUT_DIR"'/manifest.json' "$FAIL_STDERR"
grep -q 'summary='"$FAIL_OUT_DIR"'/summary.md' "$FAIL_STDERR"
grep -q 'manifest_status=failed' "$FAIL_STDERR"
grep -q 'failure_stage=region_lookup' "$FAIL_STDERR"
grep -q 'failure_code=map_region_not_found' "$FAIL_STDERR"
if grep -q 'saved-pin route export complete' "$FAIL_STDOUT"; then
    echo "failure path should not print success output" >&2
    exit 1
fi

ESCAPE_OUT_DIR="$TMP_DIR/escape_run"
ESCAPE_JOB_COPY="$TMP_DIR/escape_job.json"
ESCAPE_STDOUT="$TMP_DIR/escape_stdout.txt"
ESCAPE_STDERR="$TMP_DIR/escape_stderr.txt"
ESCAPE_FROM='bad"pin'
ESCAPE_TO='goal\pin'
if MAPFORGE_BINARY="$BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    /bin/sh "$HELPER" \
        --region seattle \
        --from-pin "$ESCAPE_FROM" \
        --to-pin "$ESCAPE_TO" \
        --out "$ESCAPE_OUT_DIR" \
        --write-job-copy "$ESCAPE_JOB_COPY" \
        > "$ESCAPE_STDOUT" 2> "$ESCAPE_STDERR"; then
    echo "expected escaped missing-pin request to fail after writing valid job JSON" >&2
    exit 1
fi

test -f "$ESCAPE_JOB_COPY"
python3 - "$ESCAPE_JOB_COPY" "$ESCAPE_FROM" "$ESCAPE_TO" <<'PY'
import json
import sys
from pathlib import Path

path, expected_from, expected_to = sys.argv[1:]
data = json.loads(Path(path).read_text(encoding="utf-8"))
assert data["from_pin"] == expected_from
assert data["to_pin"] == expected_to
assert data["map_region"] == "seattle"
assert data["output"]["frames"] is False
PY
grep -q 'saved-pin wrapper failure: binary_failed' "$ESCAPE_STDERR"
if grep -q 'saved-pin route export complete' "$ESCAPE_STDOUT"; then
    echo "escaped missing-pin failure should not print success output" >&2
    exit 1
fi

BAD_PATH_STDOUT="$TMP_DIR/bad_path_stdout.txt"
BAD_PATH_STDERR="$TMP_DIR/bad_path_stderr.txt"
if MAPFORGE_BINARY="$BINARY" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    /bin/sh "$HELPER" \
        --region seattle \
        --from-pin demo_start \
        --to-pin demo_goal \
        --out "$TMP_DIR/../bad_run" \
        > "$BAD_PATH_STDOUT" 2> "$BAD_PATH_STDERR"; then
    echo "expected saved-pin helper to reject parent traversal output path" >&2
    exit 1
fi
grep -q 'invalid local artifact path for --out: parent_traversal' "$BAD_PATH_STDERR"
if grep -q 'saved-pin route export complete' "$BAD_PATH_STDOUT"; then
    echo "unsafe output path failure should not print success output" >&2
    exit 1
fi

BAD_BINARY_STDOUT="$TMP_DIR/bad_binary_stdout.txt"
BAD_BINARY_STDERR="$TMP_DIR/bad_binary_stderr.txt"
if MAPFORGE_BINARY="$TMP_DIR/missing_mapforge_binary" \
    MAPFORGE_RUNTIME_DIR="$RUNTIME_DIR" \
    MAPFORGE_REGIONS_DIR="$REPO_DIR/data/regions" \
    /bin/sh "$HELPER" \
        --region seattle \
        --from-pin demo_start \
        --to-pin demo_goal \
        --out "$TMP_DIR/bad_binary_run" \
        > "$BAD_BINARY_STDOUT" 2> "$BAD_BINARY_STDERR"; then
    echo "expected saved-pin helper to reject missing MAPFORGE_BINARY override" >&2
    exit 1
fi
grep -q 'invalid trusted operator executable for MAPFORGE_BINARY: missing_file' "$BAD_BINARY_STDERR"
grep -q 'request data must not control MAPFORGE_BINARY' "$BAD_BINARY_STDERR"
if grep -q 'saved-pin route export complete' "$BAD_BINARY_STDOUT"; then
    echo "bad binary override failure should not print success output" >&2
    exit 1
fi

echo "headless saved-pin helper smoke passed"
