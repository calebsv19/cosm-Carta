#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
DIFF_DIR="$ROOT/shared/core/core_scene_compile"
DIFF_BIN="$DIFF_DIR/build/scene_contract_diff"
FIX_DIR="$ROOT/shared/assets/scenes/trio_contract"

make -C "$DIFF_DIR" scene-contract-diff >/dev/null

"$DIFF_BIN" \
  "$FIX_DIR/scene_runtime_min.json" \
  "$FIX_DIR/scene_runtime_min_reordered.json" >/dev/null

tmp_actual="$(mktemp -t scene_contract_diff_actual).json"
touch "$tmp_actual"
cleanup() {
  rm -f "$tmp_actual"
}
trap cleanup EXIT

cp "$FIX_DIR/scene_runtime_min.json" "$tmp_actual"
if ! perl -0pi -e 's/"space_mode_default"\s*:\s*"2d"/"space_mode_default":"3d"/' "$tmp_actual"; then
  echo "failed to prepare drift fixture" >&2
  exit 1
fi

if "$DIFF_BIN" "$FIX_DIR/scene_runtime_min.json" "$tmp_actual" >/dev/null 2>&1; then
  echo "scene_contract_diff smoke failed: expected drift detection miss" >&2
  exit 1
fi

echo "scene contract diff smoke passed"
