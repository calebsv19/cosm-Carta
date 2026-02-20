#!/usr/bin/env bash
set -euo pipefail

osm_dir="${HOME}/Desktop/osm_maps"
regions_dir="data/regions"
min_z="10"
max_z="18"
mode="missing"
region=""
osm_path=""
keep_old="1"
prune_days="30"
resume="0"
fail_fast="1"
replace="0"
prune_dry_run="0"
declare -a extra_args=()

usage() {
    cat <<USAGE
usage: tools/build_regions.sh [options]
  --osm-dir <dir>
  --regions-dir <dir>
  --min-z <z>
  --max-z <z>
  --all | --missing
  --region <name>
  --osm <file.osm>
  --keep-old <n>
  --prune-days <days>
  --resume
  --no-fail-fast
  --help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --osm-dir) osm_dir="$2"; shift 2 ;;
        --regions-dir) regions_dir="$2"; shift 2 ;;
        --min-z) min_z="$2"; shift 2 ;;
        --max-z) max_z="$2"; shift 2 ;;
        --all) mode="all"; shift ;;
        --missing) mode="missing"; shift ;;
        --region) region="$2"; shift 2 ;;
        --osm) osm_path="$2"; shift 2 ;;
        --keep-old) keep_old="$2"; shift 2 ;;
        --prune-days) prune_days="$2"; shift 2 ;;
        --resume) resume="1"; shift ;;
        --no-fail-fast) fail_fast="0"; shift ;;
        --replace) replace="1"; shift ;;
        --prune-dry-run) prune_dry_run="1"; shift ;;
        --help) usage; exit 0 ;;
        *) extra_args+=("$1"); shift ;;
    esac
done

region_tool="build/tools/mapforge_region"
graph_tool="build/tools/mapforge_graph"

if [[ ! -x "$region_tool" || ! -x "$graph_tool" ]]; then
    make tools graph
fi

if [[ -n "$region" && -z "$osm_path" ]]; then
    if [[ -f "$osm_dir/${region}.osm" ]]; then
        osm_path="$osm_dir/${region}.osm"
    elif [[ -f "$osm_dir/${region}.osm.xml" ]]; then
        osm_path="$osm_dir/${region}.osm.xml"
    else
        echo "missing OSM file for region '$region' in $osm_dir" >&2
        exit 1
    fi
fi

declare -a regions=()
declare -a osms=()

if [[ -n "$region" ]]; then
    regions+=("$region")
    osms+=("$osm_path")
else
    shopt -s nullglob
    for file in "$osm_dir"/*.osm "$osm_dir"/*.osm.xml; do
        base="$(basename "$file")"
        base="${base%.osm.xml}"
        base="${base%.osm}"
        regions+=("$base")
        osms+=("$file")
    done
    shopt -u nullglob
fi

if [[ ${#regions[@]} -eq 0 ]]; then
    echo "no regions discovered; provide --region/--osm or place .osm files under $osm_dir" >&2
    exit 1
fi

is_complete() {
    local region_root="$1"
    [[ -f "$region_root/meta.json" && -f "$region_root/graph/graph.bin" ]]
}

declare -a succeeded=()
declare -a failed=()

for i in "${!regions[@]}"; do
    name="${regions[$i]}"
    osm="${osms[$i]}"
    out="$regions_dir/$name"

    if [[ ! -f "$osm" ]]; then
        echo "skip $name: missing OSM file $osm"
        failed+=("$name (missing osm)")
        if [[ "$fail_fast" == "1" ]]; then
            break
        fi
        continue
    fi

    if [[ "$resume" == "1" ]] && is_complete "$out"; then
        echo "resume skip: $name already complete"
        continue
    fi
    if [[ "$mode" == "missing" ]] && is_complete "$out"; then
        echo "missing-mode skip: $name already complete"
        continue
    fi

    echo "[$((i + 1))/${#regions[@]}] build region: $name"
    if ! "$region_tool" --region "$name" --osm "$osm" --out "$out" --min-z "$min_z" --max-z "$max_z" --keep-old "$keep_old" --prune-days "$prune_days" \
        $( [[ "$replace" == "1" ]] && printf -- '--replace' ) \
        $( [[ "$prune_dry_run" == "1" ]] && printf -- '--prune-dry-run' ) \
        "${extra_args[@]}"; then
        failed+=("$name (region tool failed)")
        if [[ "$fail_fast" == "1" ]]; then
            break
        fi
        continue
    fi

    if ! "$graph_tool" --region "$name" --osm "$osm" --out "$out" --keep-old "$keep_old" --prune-days "$prune_days" \
        $( [[ "$replace" == "1" ]] && printf -- '--replace' ) \
        $( [[ "$prune_dry_run" == "1" ]] && printf -- '--prune-dry-run' ); then
        failed+=("$name (graph tool failed)")
        if [[ "$fail_fast" == "1" ]]; then
            break
        fi
        continue
    fi

    succeeded+=("$name")
done

echo
echo "batch summary"
echo "  success: ${#succeeded[@]}"
for name in "${succeeded[@]}"; do
    echo "    - $name"
done
echo "  failed: ${#failed[@]}"
for name in "${failed[@]}"; do
    echo "    - $name"
done

if [[ ${#failed[@]} -gt 0 ]]; then
    exit 1
fi
