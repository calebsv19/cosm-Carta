#!/usr/bin/env bash
set -euo pipefail

label="command"
if [[ "${1:-}" == "--label" ]]; then
    label="${2:-command}"
    shift 2
fi

if [[ $# -eq 0 ]]; then
    echo "usage: tools/run_with_progress.sh [--label <text>] <command...>" >&2
    exit 1
fi

start_epoch="$(date +%s)"
printf '[%s] start: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$label"
"$@"
end_epoch="$(date +%s)"
printf '[%s] done: %s (%ss)\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$label" "$((end_epoch - start_epoch))"
