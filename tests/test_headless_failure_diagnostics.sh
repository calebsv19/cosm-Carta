#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

mapforge_test_setup_tmp "mapforge_headless_diag"
JOB="$TMP_DIR/missing_region_job.json"
OUT_DIR="$TMP_DIR/run"

trap mapforge_test_cleanup_tmp EXIT INT TERM

cat > "$JOB" <<'JSON'
{
  "version": 2,
  "type": "route_playback_render",
  "map_region": "missing_diag_region",
  "from_pin": "demo_start",
  "to_pin": "demo_goal",
  "output": {
    "preview": true,
    "frame_format": "bmp"
  }
}
JSON

if "$MAPFORGE_TEST_BINARY" --headless --job "$JOB" --out "$OUT_DIR" >"$TMP_DIR/stdout.txt" 2>"$TMP_DIR/stderr.txt"; then
    echo "expected headless diagnostic job to fail" >&2
    exit 1
fi

mapforge_test_assert_file "$OUT_DIR/command.txt"
mapforge_test_assert_file "$OUT_DIR/job.resolved.json"
mapforge_test_assert_file "$OUT_DIR/manifest.json"
mapforge_test_assert_file "$OUT_DIR/summary.md"

mapforge_test_assert_grep '"status":"failed"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"stage":"region_lookup"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"code":"map_region_not_found"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep '"context":"missing_diag_region"' "$OUT_DIR/manifest.json"
mapforge_test_assert_grep 'Failure Diagnostics:' "$OUT_DIR/summary.md"
mapforge_test_assert_grep 'Stage: `region_lookup`' "$OUT_DIR/summary.md"
mapforge_test_assert_grep 'Code: `map_region_not_found`' "$OUT_DIR/summary.md"

echo "headless failure diagnostics smoke passed"
