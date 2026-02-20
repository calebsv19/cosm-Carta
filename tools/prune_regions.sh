#!/usr/bin/env bash
set -euo pipefail

regions_dir="data/regions"
prune_days="30"
keep_old="1"
dry_run="0"

usage() {
    cat <<USAGE
usage: tools/prune_regions.sh [options]
  --regions-dir <dir>
  --prune-days <days>
  --keep-old <n>
  --dry-run
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --regions-dir) regions_dir="$2"; shift 2 ;;
        --prune-days) prune_days="$2"; shift 2 ;;
        --keep-old) keep_old="$2"; shift 2 ;;
        --dry-run) dry_run="1"; shift ;;
        --help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ ! -d "$regions_dir" ]]; then
    echo "regions dir not found: $regions_dir"
    exit 0
fi

run_cmd() {
    if [[ "$dry_run" == "1" ]]; then
        echo "dry-run: $*"
    else
        eval "$*"
    fi
}

prune_snapshot_family() {
    local root="$1"
    if [[ ! -d "$root" ]]; then
        return
    fi

    while IFS= read -r region_dir; do
        mapfile -t snapshots < <(find "$region_dir" -mindepth 1 -maxdepth 1 -type d -print0 | xargs -0 ls -1dt 2>/dev/null || true)
        local count="${#snapshots[@]}"
        for ((idx=0; idx<count; idx++)); do
            snapshot="${snapshots[$idx]}"
            if (( idx >= keep_old )); then
                run_cmd "rm -rf \"$snapshot\""
                continue
            fi
            if [[ "$prune_days" -gt 0 ]]; then
                if find "$snapshot" -maxdepth 0 -type d -mtime "+$prune_days" | grep -q .; then
                    run_cmd "rm -rf \"$snapshot\""
                fi
            fi
        done
        if [[ -z "$(ls -A "$region_dir" 2>/dev/null)" ]]; then
            run_cmd "rmdir \"$region_dir\""
        fi
    done < <(find "$root" -mindepth 1 -maxdepth 1 -type d)
}

prune_staging_root() {
    local root="$1"
    if [[ ! -d "$root" || "$prune_days" -le 0 ]]; then
        return
    fi
    while IFS= read -r stale; do
        run_cmd "rm -rf \"$stale\""
    done < <(find "$root" -mindepth 1 -maxdepth 1 -type d -mtime "+$prune_days")
}

prune_snapshot_family "$regions_dir/.snapshots"
prune_snapshot_family "$regions_dir/.graph_snapshots"
prune_staging_root "$regions_dir/.staging"
prune_staging_root "$regions_dir/.graph_staging"

echo "prune complete"
