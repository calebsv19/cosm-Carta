#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_runtime_policy.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

target_contract_helper="${TARGET_CONTRACT_HELPER:-../bin/desktop_release_target_contract.sh}"
target_triple="$(TARGET_ARCH="${TARGET_ARCH:-}" TARGET_OS="${TARGET_OS:-}" TARGET_VARIANT="${TARGET_VARIANT:-desktop-app}" "$target_contract_helper" get target_triple)"
tool_root="build/targets/${target_triple}/tools"
validate_tool="${tool_root}/mapforge_region_validate"
regions_root="$workspace/regions"

if [[ ! -x "$validate_tool" ]]; then
    make tools-build >/dev/null
fi

mkdir -p "$regions_root/policy_fs_default/tiles"
cat > "$regions_root/policy_fs_default/meta.json" <<'JSON'
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

mkdir -p "$regions_root/policy_fs_explicit/tiles"
cat > "$regions_root/policy_fs_explicit/meta.json" <<'JSON'
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
    "root": "tiles",
    "runtime_source_policy": "filesystem_only"
  }
}
JSON

mkdir -p "$regions_root/policy_invalid_value/tiles"
cat > "$regions_root/policy_invalid_value/meta.json" <<'JSON'
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
    "root": "tiles",
    "runtime_source_policy": "archive_strict_maybe"
  }
}
JSON

mkdir -p "$regions_root/policy_required_mismatch/tiles"
cat > "$regions_root/policy_required_mismatch/meta.json" <<'JSON'
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
    "root": "tiles",
    "runtime_source_policy": "archive_required"
  }
}
JSON

mkdir -p "$regions_root/policy_preferred_mismatch/tiles"
cat > "$regions_root/policy_preferred_mismatch/meta.json" <<'JSON'
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
    "root": "tiles",
    "runtime_source_policy": "archive_preferred"
  }
}
JSON

mkdir -p "$regions_root/policy_archive_required_missing_payload/tiles"
cat > "$regions_root/policy_archive_required_missing_payload/meta.json" <<'JSON'
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
    "archive_path": "tiles.mbtiles",
    "runtime_source_policy": "archive_required"
  }
}
JSON

MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region policy_fs_default
MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region policy_fs_explicit

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region policy_invalid_value; then
    echo "expected failure for invalid runtime_source_policy value" >&2
    exit 1
fi

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region policy_required_mismatch; then
    echo "expected failure for archive_required policy with filesystem storage" >&2
    exit 1
fi

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region policy_preferred_mismatch; then
    echo "expected failure for archive_preferred policy with filesystem storage" >&2
    exit 1
fi

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region policy_archive_required_missing_payload; then
    echo "expected failure for archive_required policy without archive payload" >&2
    exit 1
fi

echo "runtime source policy validation checks passed"
