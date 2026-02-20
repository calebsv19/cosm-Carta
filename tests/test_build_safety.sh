#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_build_safety.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

region_tool="build/tools/mapforge_region"
graph_tool="build/tools/mapforge_graph"
region_root="$workspace/regions/test_city"

if [[ ! -x "$region_tool" || ! -x "$graph_tool" ]]; then
    make tools graph >/dev/null
fi

cat > "$workspace/base.osm" <<'OSM'
<?xml version='1.0' encoding='UTF-8'?>
<osm version="0.6" generator="test">
  <node id="1" lat="47.6000" lon="-122.3300" />
  <node id="2" lat="47.6010" lon="-122.3310" />
  <way id="10">
    <nd ref="1"/>
    <nd ref="2"/>
    <tag k="highway" v="residential"/>
  </way>
</osm>
OSM

cat > "$workspace/updated.osm" <<'OSM'
<?xml version='1.0' encoding='UTF-8'?>
<osm version="0.6" generator="test">
  <node id="1" lat="47.6000" lon="-122.3300" />
  <node id="2" lat="47.6010" lon="-122.3310" />
  <node id="3" lat="47.6020" lon="-122.3320" />
  <way id="10">
    <nd ref="1"/>
    <nd ref="2"/>
    <nd ref="3"/>
    <tag k="highway" v="residential"/>
  </way>
</osm>
OSM

"$region_tool" --region test_city --osm "$workspace/base.osm" --out "$region_root" --min-z 12 --max-z 12 --keep-old 2 --prune-days 30 --no-legacy-tiles
"$graph_tool" --region test_city --osm "$workspace/base.osm" --out "$region_root" --keep-old 2 --prune-days 30

[[ -f "$region_root/meta.json" ]]
[[ -f "$region_root/graph/graph.bin" ]]
[[ -d "$workspace/regions/.staging" ]]
[[ -d "$workspace/regions/.graph_staging" ]]

base_meta_hash="$(shasum -a 256 "$region_root/meta.json" | awk '{print $1}')"
base_graph_hash="$(shasum -a 256 "$region_root/graph/graph.bin" | awk '{print $1}')"

"$region_tool" --region test_city --osm "$workspace/updated.osm" --out "$region_root" --min-z 12 --max-z 12 --keep-old 2 --prune-days 30 --no-legacy-tiles
"$graph_tool" --region test_city --osm "$workspace/updated.osm" --out "$region_root" --keep-old 2 --prune-days 30

[[ -d "$workspace/regions/.snapshots/test_city" ]]
new_meta_hash="$(shasum -a 256 "$region_root/meta.json" | awk '{print $1}')"
new_graph_hash="$(shasum -a 256 "$region_root/graph/graph.bin" | awk '{print $1}')"

if [[ "$base_meta_hash" == "$new_meta_hash" ]]; then
    echo "meta.json did not change across rebuild" >&2
    exit 1
fi
if [[ "$base_graph_hash" == "$new_graph_hash" ]]; then
    echo "graph.bin did not change across rebuild" >&2
    exit 1
fi

if "$region_tool" --region test_city --osm "$workspace/does_not_exist.osm" --out "$region_root" --min-z 12 --max-z 12 --keep-old 2 --prune-days 30 --no-legacy-tiles; then
    echo "expected region build failure for missing osm" >&2
    exit 1
fi

after_fail_meta_hash="$(shasum -a 256 "$region_root/meta.json" | awk '{print $1}')"
if [[ "$after_fail_meta_hash" != "$new_meta_hash" ]]; then
    echo "failed region build mutated active output" >&2
    exit 1
fi

echo "build safety checks passed"
