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
  write-linked <mem_cli add args> --link-from <id>...|--link-to <id>... [--link-kind <kind>] [--link-note <text>]
    - writes memory via mem_cli add, parses created id, then creates link(s) via mem_cli link-add
    - supports multiple --link-from/--link-to anchors in one write
    - defaults --link-kind to: related
    - reuses --session-id for link audit/event when provided
  write-hier-linked <mem_cli add args> [--parent-id <id>...] [--related-id <id>...]
                    [--importance low|normal|high|critical] [--max-links <1-5>] [--link-note <text>]
    - hierarchy-first linked write with bounded candidate selection
    - discovers project pillar anchors by stable id (scope/plans/decisions/issues/misc)
    - default behavior is front-loaded: 1 link typical, 2 occasional, 3+ only for high-importance bridge nodes
  batch-write <mem_cli batch-add args>
    - forwards to: mem_cli batch-add
  health <mem_cli health args>
    - forwards to: mem_cli health
  passthrough:
    - show, pin, canonical, rollup, list, find, query, health, audit-list, event-list, event-replay-check, event-replay-apply, event-backfill
    - item-retag
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

is_allowed_link_kind() {
    local kind="${1:-}"
    case "${kind}" in
        supports|depends_on|references|summarizes|implements|blocks|contradicts|related)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
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
        link_from_ids=()
        link_to_ids=()
        link_kind="related"
        link_note=""
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --link-from)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --link-from" >&2
                        exit 1
                    fi
                    link_from_ids+=("$2")
                    shift 2
                    ;;
                --link-to)
                    if [[ $# -lt 2 ]]; then
                        echo "write-linked requires value for --link-to" >&2
                        exit 1
                    fi
                    link_to_ids+=("$2")
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
        if (( ${#link_from_ids[@]} == 0 && ${#link_to_ids[@]} == 0 )); then
            echo "write-linked requires --link-from <id> or --link-to <id>" >&2
            exit 1
        fi
        if ! is_allowed_link_kind "${link_kind}"; then
            echo "write-linked invalid --link-kind: ${link_kind}" >&2
            exit 1
        fi

        add_output="$("${MEM_CLI}" add "${add_args[@]}")"
        printf '%s\n' "${add_output}"
        new_id="$(printf '%s\n' "${add_output}" | sed -n 's/^added id=\([0-9][0-9]*\).*/\1/p' | head -n 1)"
        if [[ -z "${new_id}" ]]; then
            echo "write-linked failed to parse created id from add output" >&2
            exit 1
        fi

        seen_edges="|"
        run_link_add() {
            local from_id="$1"
            local to_id="$2"
            local edge_key=""
            local link_args=()
            if [[ -z "${from_id}" || -z "${to_id}" ]]; then
                return
            fi
            if [[ "${from_id}" == "${to_id}" ]]; then
                echo "write-linked skipped self-loop edge: ${from_id} -> ${to_id}" >&2
                return
            fi
            edge_key="${from_id}:${to_id}:${link_kind}"
            if [[ "${seen_edges}" == *"|${edge_key}|"* ]]; then
                return
            fi
            seen_edges+="${edge_key}|"
            link_args=(link-add --db "${db_path}" --from "${from_id}" --to "${to_id}" --kind "${link_kind}")
            if [[ -n "${link_note}" ]]; then
                link_args+=(--note "${link_note}")
            fi
            if [[ -n "${session_id}" ]]; then
                link_args+=(--session-id "${session_id}")
            fi
            "${MEM_CLI}" "${link_args[@]}"
        }

        for link_from in "${link_from_ids[@]}"; do
            run_link_add "${link_from}" "${new_id}"
        done
        for link_to in "${link_to_ids[@]}"; do
            run_link_add "${new_id}" "${link_to}"
        done
        ;;
    write-hier-linked)
        if ! command -v jq >/dev/null 2>&1; then
            echo "write-hier-linked requires jq on PATH" >&2
            exit 1
        fi

        add_args=()
        db_path=""
        session_id=""
        workspace_key="codework"
        project_key=""
        kind_key=""
        parent_ids=()
        related_ids=()
        link_note=""
        importance="normal"
        max_links=2
        parent_link_kind="depends_on"
        primary_link_kind="related"
        related_link_kind="related"

        while [[ $# -gt 0 ]]; do
            case "$1" in
                --parent-id)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --parent-id" >&2
                        exit 1
                    fi
                    parent_ids+=("$2")
                    shift 2
                    ;;
                --related-id)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --related-id" >&2
                        exit 1
                    fi
                    related_ids+=("$2")
                    shift 2
                    ;;
                --importance)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --importance" >&2
                        exit 1
                    fi
                    importance="$2"
                    shift 2
                    ;;
                --max-links)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --max-links" >&2
                        exit 1
                    fi
                    max_links="$2"
                    shift 2
                    ;;
                --link-note)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --link-note" >&2
                        exit 1
                    fi
                    link_note="$2"
                    shift 2
                    ;;
                --db)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --db" >&2
                        exit 1
                    fi
                    db_path="$2"
                    add_args+=("$1" "$2")
                    shift 2
                    ;;
                --session-id)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --session-id" >&2
                        exit 1
                    fi
                    session_id="$2"
                    add_args+=("$1" "$2")
                    shift 2
                    ;;
                --workspace)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --workspace" >&2
                        exit 1
                    fi
                    workspace_key="$2"
                    add_args+=("$1" "$2")
                    shift 2
                    ;;
                --project)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --project" >&2
                        exit 1
                    fi
                    project_key="$2"
                    add_args+=("$1" "$2")
                    shift 2
                    ;;
                --kind)
                    if [[ $# -lt 2 ]]; then
                        echo "write-hier-linked requires value for --kind" >&2
                        exit 1
                    fi
                    kind_key="$2"
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
            echo "write-hier-linked requires --db <path>" >&2
            exit 1
        fi
        if [[ -z "${project_key}" ]]; then
            echo "write-hier-linked requires --project <key>" >&2
            exit 1
        fi
        case "${importance}" in
            low|normal|high|critical)
                ;;
            *)
                echo "write-hier-linked invalid --importance: ${importance}" >&2
                exit 1
                ;;
        esac
        case "${max_links}" in
            ''|*[!0-9]*)
                echo "write-hier-linked --max-links must be an integer in [1,5]" >&2
                exit 1
                ;;
        esac
        if (( max_links < 1 || max_links > 5 )); then
            echo "write-hier-linked --max-links must be in [1,5]" >&2
            exit 1
        fi
        if ! is_allowed_link_kind "${parent_link_kind}"; then
            echo "write-hier-linked invalid parent link kind: ${parent_link_kind}" >&2
            exit 1
        fi
        if ! is_allowed_link_kind "${primary_link_kind}"; then
            echo "write-hier-linked invalid primary link kind: ${primary_link_kind}" >&2
            exit 1
        fi
        if ! is_allowed_link_kind "${related_link_kind}"; then
            echo "write-hier-linked invalid related link kind: ${related_link_kind}" >&2
            exit 1
        fi

        find_item_id_by_stable_id() {
            local stable_id="$1"
            local query_args=()
            local query_json=""
            query_args=(query --db "${db_path}" --limit 256 --format json)
            if [[ -n "${workspace_key}" ]]; then
                query_args+=(--workspace "${workspace_key}")
            fi
            if [[ -n "${project_key}" ]]; then
                query_args+=(--project "${project_key}")
            fi
            query_json="$("${MEM_CLI}" "${query_args[@]}" 2>/dev/null || printf '[]')"
            printf '%s\n' "${query_json}" | jq -r --arg sid "${stable_id}" '[.[] | select(.stable_id == $sid)][0].id // ""'
        }

        candidate_ids=()
        candidate_kinds=()
        candidate_sources=()
        seen_candidate_ids="|"
        add_candidate() {
            local candidate_id="$1"
            local candidate_kind="$2"
            local candidate_source="$3"
            if [[ -z "${candidate_id}" || "${candidate_id}" == "null" ]]; then
                return
            fi
            if [[ "${seen_candidate_ids}" == *"|${candidate_id}|"* ]]; then
                return
            fi
            seen_candidate_ids+="${candidate_id}|"
            candidate_ids+=("${candidate_id}")
            candidate_kinds+=("${candidate_kind}")
            candidate_sources+=("${candidate_source}")
        }

        primary_pillar_stable_id=""
        case "${kind_key}" in
            plan)
                primary_pillar_stable_id="plans-${project_key}"
                ;;
            decision)
                primary_pillar_stable_id="decisions-${project_key}"
                ;;
            issue)
                primary_pillar_stable_id="issues-${project_key}"
                ;;
            scope)
                primary_pillar_stable_id="scope-${project_key}"
                ;;
            *)
                primary_pillar_stable_id="scope-${project_key}"
                ;;
        esac

        scope_pillar_id="$(find_item_id_by_stable_id "scope-${project_key}")"
        primary_pillar_id="$(find_item_id_by_stable_id "${primary_pillar_stable_id}")"
        misc_pillar_id="$(find_item_id_by_stable_id "misc-${project_key}")"

        for parent_id in "${parent_ids[@]}"; do
            add_candidate "${parent_id}" "${parent_link_kind}" "parent"
        done
        add_candidate "${primary_pillar_id}" "${primary_link_kind}" "primary-pillar"
        if [[ -n "${scope_pillar_id}" && "${scope_pillar_id}" != "${primary_pillar_id}" ]]; then
            add_candidate "${scope_pillar_id}" "related" "scope-pillar"
        fi
        for related_id in "${related_ids[@]}"; do
            add_candidate "${related_id}" "${related_link_kind}" "related-id"
        done
        add_candidate "${misc_pillar_id}" "related" "misc-fallback"

        if (( ${#candidate_ids[@]} == 0 )); then
            echo "write-hier-linked found no anchor candidates for project=${project_key}; seed pillar anchors first" >&2
            exit 1
        fi

        desired_links=1
        if (( ${#parent_ids[@]} > 0 )) && [[ -n "${primary_pillar_id}" ]]; then
            desired_links=2
        fi
        case "${importance}" in
            low)
                desired_links=1
                ;;
            normal)
                ;;
            high)
                if (( desired_links < 3 )); then
                    desired_links=3
                fi
                ;;
            critical)
                if (( desired_links < 4 )); then
                    desired_links=4
                fi
                ;;
        esac
        if (( desired_links > max_links )); then
            desired_links="${max_links}"
        fi
        if (( desired_links > ${#candidate_ids[@]} )); then
            desired_links="${#candidate_ids[@]}"
        fi

        add_output="$("${MEM_CLI}" add "${add_args[@]}")"
        printf '%s\n' "${add_output}"
        new_id="$(printf '%s\n' "${add_output}" | sed -n 's/^added id=\([0-9][0-9]*\).*/\1/p' | head -n 1)"
        if [[ -z "${new_id}" ]]; then
            echo "write-hier-linked failed to parse created id from add output" >&2
            exit 1
        fi

        links_added=0
        idx=0
        while (( idx < desired_links )); do
            target_id="${candidate_ids[$idx]}"
            target_kind="${candidate_kinds[$idx]}"
            target_source="${candidate_sources[$idx]}"
            if [[ "${target_id}" != "${new_id}" ]]; then
                link_args=(link-add --db "${db_path}" --from "${new_id}" --to "${target_id}" --kind "${target_kind}")
                if [[ -n "${link_note}" ]]; then
                    link_args+=(--note "${link_note}")
                fi
                if [[ -n "${session_id}" ]]; then
                    link_args+=(--session-id "${session_id}")
                fi
                "${MEM_CLI}" "${link_args[@]}"
                links_added=$(( links_added + 1 ))
                printf 'write-hier-linked link[%d]: %s -> %s kind=%s source=%s\n' "$((idx + 1))" "${new_id}" "${target_id}" "${target_kind}" "${target_source}"
            fi
            idx=$(( idx + 1 ))
        done
        printf 'write-hier-linked summary: new_id=%s links_added=%d importance=%s max_links=%s\n' "${new_id}" "${links_added}" "${importance}" "${max_links}"
        ;;
    batch-write)
        exec "${MEM_CLI}" batch-add "$@"
        ;;
    health|audit-list|event-list|event-replay-check|event-replay-apply|event-backfill|show|pin|canonical|item-retag|rollup|list|find|query|link-add|link-list|neighbors|link-update|link-remove)
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
