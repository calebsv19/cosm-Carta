#!/usr/bin/env bash
set -euo pipefail

# Convert line_drawing exports (ShapeLib JSON) into canonical ShapeAssets.
# Usage:
#   SHAPE_ASSET_DIR=shared/assets/shapes shared/shape/sync_exports.sh
# Optional args:
#   $1 = export dir (defaults to line_drawing/export)
#   SHAPE_ASSET_CONVERTER overrides the converter binary path.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXPORT_DIR="${1:-"$ROOT/line_drawing/export"}"
OUT_DIR="${SHAPE_ASSET_DIR:-"$ROOT/shared/assets/shapes"}"

mkdir -p "$OUT_DIR"

# Pick a converter: prefer ray_tracing CLI, fall back to physics_sim.
CONVERTER="${SHAPE_ASSET_CONVERTER:-}"
if [[ -z "$CONVERTER" ]]; then
    if [[ -x "$ROOT/ray_tracing/tools/cli/bin/shape_asset_tool" ]]; then
        CONVERTER="$ROOT/ray_tracing/tools/cli/bin/shape_asset_tool"
    elif [[ -x "$ROOT/physics_sim/shape_asset_tool" ]]; then
        CONVERTER="$ROOT/physics_sim/shape_asset_tool"
    fi
fi

if [[ ! -x "${CONVERTER:-}" ]]; then
    echo "[sync_exports] No converter found. Build one via:"
    echo "  (cd ray_tracing && make cli-tools)"
    echo "    or"
    echo "  (cd physics_sim && make shape_asset_tool)"
    exit 1
fi

shopt -s nullglob
files=("$EXPORT_DIR"/*.json)
if (( ${#files[@]} == 0 )); then
    echo "[sync_exports] No JSON exports found in $EXPORT_DIR"
    exit 0
fi

echo "[sync_exports] Using converter: $CONVERTER"
echo "[sync_exports] Export dir:     $EXPORT_DIR"
echo "[sync_exports] Output dir:     $OUT_DIR"

converted=0
manifest_entries=()
for f in "${files[@]}"; do
    base="$(basename "$f")"
    name="${base%.json}"
    out="$OUT_DIR/$name.asset.json"
    "$CONVERTER" --max-error 0.5 --out "$out" "$f"
    converted=$((converted + 1))
    manifest_entries+=("{\"file\":\"$name.asset.json\"}")
done

# Optional: run sanity check if available.
SANITY_BIN=()
for candidate in \
    "$ROOT/physics_sim/shape_sanity_tool" \
    "$ROOT/ray_tracing/tools/cli/bin/shape_sanity_tool" \
    "$ROOT/line_drawing/build/bin/shape_sanity_tool"; do
    [[ -x "$candidate" ]] && SANITY_BIN+=("$candidate")
done

if (( ${#SANITY_BIN[@]} > 0 )); then
    echo "[sync_exports] Running sanity check..."
    "${SANITY_BIN[0]}" "$OUT_DIR" || true
else
    echo "[sync_exports] Sanity check skipped (no shape_sanity_tool built)."
fi

# Emit a simple manifest for downstream discovery.
manifest_path="$OUT_DIR/manifest.json"
{
    echo "{"
    echo "  \"generated_by\": \"shared/shape/sync_exports.sh\","
    echo "  \"count\": ${#manifest_entries[@]},"
    echo "  \"assets\": ["
    for i in "${!manifest_entries[@]}"; do
        sep=$([[ $i -lt $(( ${#manifest_entries[@]} - 1 )) ]] && echo "," || true)
        printf "    %s%s\n" "${manifest_entries[$i]}" "$sep"
    done
    echo "  ]"
    echo "}"
} > "$manifest_path"
echo "[sync_exports] Wrote manifest: $manifest_path"

echo "[sync_exports] Converted $converted file(s) into $OUT_DIR"
