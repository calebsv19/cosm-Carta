#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_validate_contract.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

target_contract_helper="${TARGET_CONTRACT_HELPER:-../bin/desktop_release_target_contract.sh}"
target_triple="$(TARGET_ARCH="${TARGET_ARCH:-}" TARGET_OS="${TARGET_OS:-}" TARGET_VARIANT="${TARGET_VARIANT:-desktop-app}" "$target_contract_helper" get target_triple)"
tool_root="build/targets/${target_triple}/tools"
validate_tool="${tool_root}/mapforge_region_validate"
regions_root="$workspace/regions"

if [[ ! -x "$validate_tool" ]]; then
    make tools-build >/dev/null
fi

mkdir -p "$regions_root/contract_ok/tiles"
cat > "$regions_root/contract_ok/meta.json" <<'JSON'
{
  "package_contract": {
    "family": "map_forge.region_package",
    "version": 1,
    "tile_store_contract": "map_forge.tile_store.v1"
  },
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
  },
  "output_stats": {
    "tile_coverage": {
      "rows": 1,
      "bands": {
        "default": {
          "local": {
            "tile_count": 1,
            "zoom_bounds": {
              "12": {"tiles": 1, "min_x": 655, "max_x": 655, "min_y": 1582, "max_y": 1582}
            }
          }
        }
      }
    }
  }
}
JSON

mkdir -p "$regions_root/contract_legacy/tiles"
cat > "$regions_root/contract_legacy/meta.json" <<'JSON'
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

mkdir -p "$regions_root/contract_bad_version/tiles"
cat > "$regions_root/contract_bad_version/meta.json" <<'JSON'
{
  "package_contract": {
    "family": "map_forge.region_package",
    "version": 0
  },
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

MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region contract_ok
MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region contract_ok --strict-contract

MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region contract_legacy
legacy_stderr="$workspace/contract_legacy_stderr.txt"
if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region contract_legacy --strict-contract 2>"$legacy_stderr"; then
    echo "expected strict-contract validation failure for legacy package" >&2
    exit 1
fi
grep -q 'diagnostic_stage=validation region=contract_legacy' "$legacy_stderr"
grep -q 'repair_hint=Rebuild with the current region tool to emit package_contract metadata.' "$legacy_stderr"

bad_version_stderr="$workspace/contract_bad_version_stderr.txt"
if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region contract_bad_version 2>"$bad_version_stderr"; then
    echo "expected validation failure for package_contract.version=0" >&2
    exit 1
fi
grep -q 'diagnostic_stage=validation region=contract_bad_version' "$bad_version_stderr"
grep -q 'repair_hint=Set package_contract.version to 1 or rebuild with the current region tool.' "$bad_version_stderr"

echo "region contract validation checks passed"
