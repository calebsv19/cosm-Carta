#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_coverage_contract.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

validate_tool="build/tools/mapforge_region_validate"
regions_root="$workspace/regions"

if [[ ! -x "$validate_tool" ]]; then
    make tools-build >/dev/null
fi

mkdir -p "$regions_root/coverage_ok_v1/tiles"
cat > "$regions_root/coverage_ok_v1/meta.json" <<'JSON'
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
    "root": "tiles",
    "runtime_source_policy": "filesystem_only"
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

mkdir -p "$regions_root/coverage_missing_v1/tiles"
cat > "$regions_root/coverage_missing_v1/meta.json" <<'JSON'
{
  "package_contract": {
    "family": "map_forge.region_package",
    "version": 1
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
  "output_stats": {}
}
JSON

mkdir -p "$regions_root/coverage_bad_zoom_v1/tiles"
cat > "$regions_root/coverage_bad_zoom_v1/meta.json" <<'JSON'
{
  "package_contract": {
    "family": "map_forge.region_package",
    "version": 1
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
              "z12": {"tiles": 1, "min_x": 655, "max_x": 655, "min_y": 1582, "max_y": 1582}
            }
          }
        }
      }
    }
  }
}
JSON

mkdir -p "$regions_root/coverage_bad_bounds_v1/tiles"
cat > "$regions_root/coverage_bad_bounds_v1/meta.json" <<'JSON'
{
  "package_contract": {
    "family": "map_forge.region_package",
    "version": 1
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
              "12": {"tiles": 1, "min_x": 700, "max_x": 650, "min_y": 1582, "max_y": 1582}
            }
          }
        }
      }
    }
  }
}
JSON

mkdir -p "$regions_root/coverage_legacy_missing/tiles"
cat > "$regions_root/coverage_legacy_missing/meta.json" <<'JSON'
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

MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region coverage_ok_v1 --strict-contract
MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region coverage_legacy_missing

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region coverage_missing_v1 --strict-contract; then
    echo "expected failure for package_contract v1 missing output_stats.tile_coverage" >&2
    exit 1
fi

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region coverage_bad_zoom_v1 --strict-contract; then
    echo "expected failure for invalid tile_coverage zoom key" >&2
    exit 1
fi

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region coverage_bad_bounds_v1 --strict-contract; then
    echo "expected failure for invalid tile_coverage bounds payload" >&2
    exit 1
fi

if MAPFORGE_REGIONS_DIR="$regions_root" "$validate_tool" --region coverage_bad_bounds_v1; then
    echo "expected failure for malformed v1 tile_coverage even without strict-contract" >&2
    exit 1
fi

echo "coverage metadata contract checks passed"
