#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_validate_strict.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

target_contract_helper="${TARGET_CONTRACT_HELPER:-../bin/desktop_release_target_contract.sh}"
target_triple="$(TARGET_ARCH="${TARGET_ARCH:-}" TARGET_OS="${TARGET_OS:-}" TARGET_VARIANT="${TARGET_VARIANT:-desktop-app}" "$target_contract_helper" get target_triple)"
tool_root="build/targets/${target_triple}/tools"
validate_tool="${tool_root}/mapforge_region_validate"
regions_root="$workspace/regions"

if [[ ! -x "$validate_tool" ]]; then
    make tools-build >/dev/null
fi

mkdir -p "$regions_root/strict_ok_fs/tiles"
mkdir -p "$regions_root/strict_ok_fs/graph"
: > "$regions_root/strict_ok_fs/graph/graph.bin"
cat > "$regions_root/strict_ok_fs/meta.json" <<'JSON'
{
  "bounds": {
    "min_lat": 47.6000,
    "max_lat": 47.6020,
    "min_lon": -122.3320,
    "max_lon": -122.3300
  },
  "tile": {
    "min_z": 12,
    "max_z": 12,
    "extent": 4096
  },
  "tile_store": {
    "kind": "filesystem_tree",
    "root": "tiles"
  }
}
JSON

mkdir -p "$regions_root/strict_fail_archive/tiles"
: > "$regions_root/strict_fail_archive/tiles.mbtiles"
cat > "$regions_root/strict_fail_archive/meta.json" <<'JSON'
{
  "bounds": {
    "min_lat": 47.6000,
    "max_lat": 47.6020,
    "min_lon": -122.3320,
    "max_lon": -122.3300
  },
  "tile": {
    "min_z": 12,
    "max_z": 12,
    "extent": 4096
  },
  "tile_store": {
    "kind": "archive_indexed",
    "root": "tiles",
    "archive_path": "tiles.mbtiles"
  }
}
JSON

MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region strict_ok_fs
MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region strict_ok_fs --strict
MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region strict_fail_archive

strict_fail_stderr="$workspace/strict_fail_stderr.txt"
if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region strict_fail_archive --strict 2>"$strict_fail_stderr"; then
    echo "expected strict validation failure when archive fallback tree is present" >&2
    exit 1
fi
grep -q 'diagnostic_stage=validation region=strict_fail_archive' "$strict_fail_stderr"
grep -q 'repair_hint=Provide a readable archive payload or run without --strict when tree fallback is acceptable.' "$strict_fail_stderr"

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --strict; then
    echo "expected strict validation to fail when any region uses archive fallback" >&2
    exit 1
fi

echo "region strict validation checks passed"
