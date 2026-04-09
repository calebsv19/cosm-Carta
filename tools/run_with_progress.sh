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

heartbeat_sec="${RUN_WITH_PROGRESS_HEARTBEAT_SEC:-5}"
if ! [[ "$heartbeat_sec" =~ ^[0-9]+$ ]] || [[ "$heartbeat_sec" -lt 1 ]]; then
    heartbeat_sec=5
fi

start_epoch="$(date +%s)"
printf '[%s] start: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$label"

"$@" &
cmd_pid=$!
heartbeat_pid=""
spinner_pid=""

cleanup() {
    if [[ -n "$heartbeat_pid" ]] && kill -0 "$heartbeat_pid" 2>/dev/null; then
        kill "$heartbeat_pid" 2>/dev/null || true
    fi
    if [[ -n "$spinner_pid" ]] && kill -0 "$spinner_pid" 2>/dev/null; then
        kill "$spinner_pid" 2>/dev/null || true
    fi
    if kill -0 "$cmd_pid" 2>/dev/null; then
        kill "$cmd_pid" 2>/dev/null || true
    fi
}

trap cleanup INT TERM

(
    while kill -0 "$cmd_pid" 2>/dev/null; do
        sleep "$heartbeat_sec"
        if ! kill -0 "$cmd_pid" 2>/dev/null; then
            break
        fi
        now_epoch="$(date +%s)"
        printf '[%s] running: %s (%ss elapsed)\n' \
            "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
            "$label" \
            "$((now_epoch - start_epoch))"
    done
) &
heartbeat_pid=$!

if [[ -t 2 ]]; then
    (
        frames=("|" "/" "-" "\\")
        frame_idx=0
        while kill -0 "$cmd_pid" 2>/dev/null; do
            now_epoch="$(date +%s)"
            printf '\r[%s] running: %s %s (%ss elapsed)' \
                "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
                "$label" \
                "${frames[$frame_idx]}" \
                "$((now_epoch - start_epoch))" >&2
            frame_idx=$(((frame_idx + 1) % ${#frames[@]}))
            sleep 0.2
        done
    ) &
    spinner_pid=$!
fi

set +e
wait "$cmd_pid"
status=$?
set -e

if kill -0 "$heartbeat_pid" 2>/dev/null; then
    kill "$heartbeat_pid" 2>/dev/null || true
fi
wait "$heartbeat_pid" 2>/dev/null || true
if [[ -n "$spinner_pid" ]] && kill -0 "$spinner_pid" 2>/dev/null; then
    kill "$spinner_pid" 2>/dev/null || true
fi
wait "$spinner_pid" 2>/dev/null || true
if [[ -t 2 ]]; then
    printf '\r%*s\r' 120 '' >&2
fi

trap - INT TERM

end_epoch="$(date +%s)"
if [[ "$status" -eq 0 ]]; then
    printf '[%s] done: %s (%ss)\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$label" "$((end_epoch - start_epoch))"
else
    printf '[%s] failed: %s (%ss, exit=%s)\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "$label" \
        "$((end_epoch - start_epoch))" \
        "$status"
fi

exit "$status"
