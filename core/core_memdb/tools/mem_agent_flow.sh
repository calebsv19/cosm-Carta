#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

print_usage() {
    cat <<'EOF'
usage: mem_agent_flow.sh <command> [args]

commands:
  retrieve <mem_cli query args>
    - forwards to: mem_cli query
    - default limit: 24 (if --limit is not provided)
    - supports scoped filters: --workspace, --project, --kind
  retrieve-canonical <mem_cli query args>
    - forwards to: mem_cli query --canonical-only
    - default limit: 8 (if --limit is not provided)
  retrieve-pinned <mem_cli query args>
    - forwards to: mem_cli query --pinned-only
    - default limit: 8 (if --limit is not provided)
  retrieve-recent <mem_cli query args>
    - forwards to: mem_cli query
    - default limit: 24 (if --limit is not provided)
  retrieve-search --query <fts> <mem_cli query args>
    - forwards to: mem_cli query
    - default limit: 24 (if --limit is not provided)
  write <mem_cli add args>
    - forwards to: mem_cli add
    - supports scoped metadata: --workspace, --project, --kind
    - supports budget controls: --session-id, --session-max-writes
  write-linked <mem_cli add args> --link-from <id>|--link-to <id> [--link-kind <kind>] [--link-note <text>]
    - writes memory via mem_cli add, parses created id, then creates link(s) via mem_cli link-add
    - defaults --link-kind to: related
    - reuses --session-id for link audit/event when provided
  batch-write <mem_cli batch-add args>
    - forwards to: mem_cli batch-add
  health <mem_cli health args>
    - forwards to: mem_cli health
  passthrough:
    - show, pin, canonical, rollup, list, find, query, health, audit-list, event-list, event-replay-check, event-replay-apply, event-backfill
    - link-add, link-list, link-update, link-remove, neighbors
EOF
}

if [[ ! -x "${MEM_CLI}" ]]; then
    echo "mem_cli not found at ${MEM_CLI}" >&2
    echo "build it with: make -C shared/core/core_memdb all" >&2
    exit 1
fi

if [[ $# -lt 1 ]]; then
    print_usage
    exit 1
fi

command="$1"
shift || true

has_flag_arg() {
    local target="$1"
    shift || true
    for arg in "$@"; do
        if [[ "${arg}" == "${target}" ]]; then
            return 0
        fi
    done
    return 1
}

case "${command}" in
    retrieve)
        if has_flag_arg "--limit" "$@"; then
            exec "${MEM_CLI}" query "$@"
        fi
        exec "${MEM_CLI}" query "$@" --limit 24
        ;;
    retrieve-canonical)
        if has_flag_arg "--canonical-only" "$@"; then
            if has_flag_arg "--limit" "$@"; then
                exec "${MEM_CLI}" query "$@"
            fi
            exec "${MEM_CLI}" query "$@" --limit 8
        fi
        if has_flag_arg "--limit" "$@"; then
            exec "${MEM_CLI}" query "$@" --canonical-only
        fi
        exec "${MEM_CLI}" query "$@" --canonical-only --limit 8
        ;;
    retrieve-pinned)
        if has_flag_arg "--pinned-only" "$@"; then
            if has_flag_arg "--limit" "$@"; then
                exec "${MEM_CLI}" query "$@"
            fi
            exec "${MEM_CLI}" query "$@" --limit 8
        fi
        if has_flag_arg "--limit" "$@"; then
            exec "${MEM_CLI}" query "$@" --pinned-only
        fi
        exec "${MEM_CLI}" query "$@" --pinned-only --limit 8
        ;;
    retrieve-recent)
        if has_flag_arg "--limit" "$@"; then
            exec "${MEM_CLI}" query "$@"
        fi
        exec "${MEM_CLI}" query "$@" --limit 24
        ;;
    retrieve-search)
        if ! has_flag_arg "--query" "$@"; then
            echo "retrieve-search requires --query <fts>" >&2
            exit 1
        fi
        if has_flag_arg "--limit" "$@"; then
            exec "${MEM_CLI}" query "$@"
        fi
        exec "${MEM_CLI}" query "$@" --limit 24
        ;;
    write)
        exec "${MEM_CLI}" add "$@"
        ;;
    write-linked)
        add_args=()
        db_path=""
        session_id=""
        link_from=""
        link_to=""
        link_kind="related"
        link_note=""
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --link-from)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --link-from" >&2
                        exit 1
                    fi
                    link_from="$2"
                    shift 2
                    ;;
                --link-to)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --link-to" >&2
                        exit 1
                    fi
                    link_to="$2"
                    shift 2
                    ;;
                --link-kind)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --link-kind" >&2
                        exit 1
                    fi
                    link_kind="$2"
                    shift 2
                    ;;
                --link-note)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --link-note" >&2
                        exit 1
                    fi
                    link_note="$2"
                    shift 2
                    ;;
                --db)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --db" >&2
                        exit 1
                    fi
                    db_path="$2"
                    add_args+=("$1" "$2")
                    shift 2
                    ;;
                --session-id)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --session-id" >&2
                        exit 1
                    fi
                    session_id="$2"
                    add_args+=("$1" "$2")
                    shift 2
                    ;;
                *)
                    add_args+=("$1")
                    shift
                    ;;
            esac
        done

        if [[ -z "${db_path}" ]]; then
            echo "write-linked requires --db <path>" >&2
            exit 1
        fi
        if [[ -z "${link_from}" && -z "${link_to}" ]]; then
            echo "write-linked requires --link-from <id> or --link-to <id>" >&2
            exit 1
        fi

        add_output="$("${MEM_CLI}" add "${add_args[@]}")"
        printf '%s\n' "${add_output}"
        new_id="$(printf '%s\n' "${add_output}" | sed -n 's/^added id=\([0-9][0-9]*\).*/\1/p' | head -n 1)"
        if [[ -z "${new_id}" ]]; then
            echo "write-linked failed to parse created id from add output" >&2
            exit 1
        fi

        if [[ -n "${link_from}" ]]; then
            link_args=(link-add --db "${db_path}" --from "${link_from}" --to "${new_id}" --kind "${link_kind}")
            if [[ -n "${link_note}" ]]; then
                link_args+=(--note "${link_note}")
            fi
            if [[ -n "${session_id}" ]]; then
                link_args+=(--session-id "${session_id}")
            fi
            "${MEM_CLI}" "${link_args[@]}"
        fi

        if [[ -n "${link_to}" ]]; then
            link_args=(link-add --db "${db_path}" --from "${new_id}" --to "${link_to}" --kind "${link_kind}")
            if [[ -n "${link_note}" ]]; then
                link_args+=(--note "${link_note}")
            fi
            if [[ -n "${session_id}" ]]; then
                link_args+=(--session-id "${session_id}")
            fi
            "${MEM_CLI}" "${link_args[@]}"
        fi
        ;;
    batch-write)
        exec "${MEM_CLI}" batch-add "$@"
        ;;
    health|audit-list|event-list|event-replay-check|event-replay-apply|event-backfill|show|pin|canonical|rollup|list|find|query|link-add|link-list|neighbors|link-update|link-remove)
        exec "${MEM_CLI}" "${command}" "$@"
        ;;
    help|-h|--help)
        print_usage
        ;;
    *)
        echo "unknown command: ${command}" >&2
        print_usage
        exit 1
        ;;
esac
