#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_archive_metrics.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

target_contract_helper="${TARGET_CONTRACT_HELPER:-../bin/desktop_release_target_contract.sh}"
target_triple="$(TARGET_ARCH="${TARGET_ARCH:-}" TARGET_OS="${TARGET_OS:-}" TARGET_VARIANT="${TARGET_VARIANT:-desktop-app}" "$target_contract_helper" get target_triple)"
tool_root="build/targets/${target_triple}/tools"
region_tool="${tool_root}/mapforge_region"
validate_tool="${tool_root}/mapforge_region_validate"
region_root="$workspace/regions/rollup_city"

if [[ ! -x "$region_tool" ]]; then
    make tools >/dev/null
fi
if [[ ! -x "$validate_tool" ]]; then
    make tools-build >/dev/null
fi

cat > "$workspace/base.osm" <<'OSM'
<?xml version='1.0' encoding='UTF-8'?>
<osm version="0.6" generator="test">
  <node id="1" lat="47.6000" lon="-122.3300" />
  <node id="2" lat="47.6006" lon="-122.3307" />
  <node id="3" lat="47.6012" lon="-122.3312" />
  <node id="4" lat="47.6002" lon="-122.3310" />
  <node id="5" lat="47.6002" lon="-122.3304" />
  <way id="10">
    <nd ref="1"/>
    <nd ref="2"/>
    <nd ref="3"/>
    <tag k="highway" v="residential"/>
  </way>
  <way id="20">
    <nd ref="1"/>
    <nd ref="4"/>
    <nd ref="2"/>
    <nd ref="5"/>
    <nd ref="1"/>
    <tag k="building" v="yes"/>
  </way>
</osm>
OSM

"$region_tool" \
    --region rollup_city \
    --osm "$workspace/base.osm" \
    --out "$region_root" \
    --min-z 12 \
    --max-z 12 \
    --keep-old 1 \
    --prune-days 30 \
    --no-legacy-tiles \
    --emit-archive \
    --archive-path tiles.mbtiles

[[ -f "$region_root/meta.json" ]]
[[ -f "$region_root/meta.dataset.json" ]]
[[ -f "$region_root/tiles.mbtiles" ]]
MAPFORGE_REGIONS_DIR="$workspace/regions" "$validate_tool" --region rollup_city >/dev/null

if ! rg -q "\"archive_rollups\": \\{" "$region_root/meta.json"; then
    echo "meta.json missing archive_rollups object" >&2
    exit 1
fi
if ! rg -q "\"tile_coverage\": \\{" "$region_root/meta.json"; then
    echo "meta.json missing tile_coverage object" >&2
    exit 1
fi
if ! rg -q "\"build_manifest\": \\{" "$region_root/meta.json"; then
    echo "meta.json missing build_manifest object" >&2
    exit 1
fi
if ! rg -q "\"canonical_input_format\": \"osm.pbf\"" "$region_root/meta.json"; then
    echo "meta.json build_manifest missing canonical_input_format" >&2
    exit 1
fi
if ! rg -q "\"detected_source_kind\": \"xml\"" "$region_root/meta.json"; then
    echo "meta.json build_manifest missing detected_source_kind=xml for XML fixture" >&2
    exit 1
fi
if ! rg -q "\"compat_source_mode\": true" "$region_root/meta.json"; then
    echo "meta.json build_manifest missing compat_source_mode=true for XML fixture" >&2
    exit 1
fi
if ! rg -q "\"runtime_source_policy\": \"archive_preferred\"" "$region_root/meta.json"; then
    echo "meta.json missing runtime_source_policy=archive_preferred for archive build" >&2
    exit 1
fi
if ! rg -q "\"source_hash_fnv1a64\": \"0x[0-9a-f]{16}\"" "$region_root/meta.json"; then
    echo "meta.json build_manifest missing deterministic source hash" >&2
    exit 1
fi
if ! rg -q "\"local\": \\{\"rows\": [1-9][0-9]*, \"bytes\": [1-9][0-9]*\\}" "$region_root/meta.json"; then
    echo "meta.json missing non-empty local archive rollup entry" >&2
    exit 1
fi
if ! rg -q "\"local\": \\{" "$region_root/meta.json"; then
    echo "meta.json tile_coverage missing local layer object" >&2
    exit 1
fi
if ! rg -q "\"zoom_bounds\": \\{" "$region_root/meta.json"; then
    echo "meta.json tile_coverage missing zoom_bounds object" >&2
    exit 1
fi
if ! rg -q "\"12\": \\{\"tiles\": [1-9][0-9]*, \"min_x\": [0-9]+, \"max_x\": [0-9]+, \"min_y\": [0-9]+, \"max_y\": [0-9]+\\}" "$region_root/meta.json"; then
    echo "meta.json tile_coverage missing zoom=12 bounds payload" >&2
    exit 1
fi

if ! rg -q "\"archive_rollups_table\": \"map_forge_archive_rollups_v1\"" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json missing archive_rollups_table metadata" >&2
    exit 1
fi
if ! rg -q "\"canonical_input_format\": \"osm.pbf\"" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json missing canonical_input_format metadata" >&2
    exit 1
fi
if ! rg -q "\"detected_source_kind\": \"xml\"" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json missing detected_source_kind metadata" >&2
    exit 1
fi
if ! rg -q "\"name\": \"map_forge_archive_rollups_v1\"" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json missing archive rollup table item" >&2
    exit 1
fi
if ! rg -q "\"rows\": 28" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json archive rollup table row count unexpected" >&2
    exit 1
fi
if ! rg -q "\"archive_tile_rows\": [1-9][0-9]*" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json summary missing non-zero archive_tile_rows" >&2
    exit 1
fi
if ! rg -q "\"archive_bytes_written\": [1-9][0-9]*" "$region_root/meta.dataset.json"; then
    echo "meta.dataset.json summary missing non-zero archive_bytes_written" >&2
    exit 1
fi

echo "archive metrics rollup checks passed"
