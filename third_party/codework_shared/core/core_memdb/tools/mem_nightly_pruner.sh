#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'EOF'
usage: mem_nightly_pruner.sh --db <path> --run-dir <dir> [options]

required:
  --db <path>                  SQLite DB path
  --run-dir <dir>              Output directory for this run

options:
  --plan <path>                Plan file path (default: <run-dir>/pruner_plan.json)
  --session-id <id>            Session id for write audit rows
  --apply                      Execute DB writes (default: dry-run only)
  --allow-empty-apply          Allow apply mode when zero operations are approved
  --max-links-added <n>        Override budget cap
  --max-links-updated <n>      Override budget cap
  --max-rollups <n>            Override budget cap
  --max-nodes-affected <n>     Override budget cap
  --rollup-chunk-max-items <n> Override rollup chunk size per rollup command
  --rollup-workspace <key>     Override rollup scope workspace filter
  --rollup-project <key>       Override rollup scope project filter
  --rollup-kind <value>        Override rollup scope kind filter
  -h, --help                   Show this help

Plan format expected:
  .budgets.max_links_added
  .budgets.max_links_updated
  .budgets.max_rollups
  .budgets.max_nodes_affected
  .operations.rollup.enabled
  .operations.rollup.before_ns
  .operations.rollup.chunk_max_items (optional)
  .operations.rollup.scope.workspace (optional)
  .operations.rollup.scope.project (optional)
  .operations.rollup.scope.kind (optional)
  .operations.connection_pass.enabled (optional)
  .operations.connection_pass.link_kind (optional, default: related)
  .operations.connection_pass.anchor_kind (optional, default: summarizes)
  .operations.connection_pass.anchor_item_id (optional, default: 0)
  .operations.connection_pass.ensure_min_degree (optional, default: true)
  .operations.connection_pass.propagate_neighbor_links (optional, default: false)
  .operations.connection_pass.max_neighbor_links_per_rollup (optional, default: 0)
  .operations.connection_pass.neighbor_scan_max_edges (optional, default: 24)
  .operations.connection_pass.neighbor_scan_max_nodes (optional, default: 16)
  .operations.link_additions[] {from,to,kind,note?}
    OR .operations.link_additions {enabled,items:[...]}
  .operations.link_updates[] {id,kind,weight?,note?}
    OR .operations.link_updates {enabled,items:[...]}
EOF
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

db_path=""
run_dir=""
plan_path=""
session_id=""
apply_mode=false
allow_empty_apply=false
budget_links_added_override=""
budget_links_updated_override=""
budget_rollups_override=""
budget_nodes_override=""
rollup_chunk_override=""
rollup_workspace_override=""
rollup_project_override=""
rollup_kind_override=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --db)
            db_path="${2:-}"
            shift 2
            ;;
        --run-dir)
            run_dir="${2:-}"
            shift 2
            ;;
        --plan)
            plan_path="${2:-}"
            shift 2
            ;;
        --session-id)
            session_id="${2:-}"
            shift 2
            ;;
        --apply)
            apply_mode=true
            shift
            ;;
        --allow-empty-apply)
            allow_empty_apply=true
            shift
            ;;
        --max-links-added)
            budget_links_added_override="${2:-}"
            shift 2
            ;;
        --max-links-updated)
            budget_links_updated_override="${2:-}"
            shift 2
            ;;
        --max-rollups)
            budget_rollups_override="${2:-}"
            shift 2
            ;;
        --max-nodes-affected)
            budget_nodes_override="${2:-}"
            shift 2
            ;;
        --rollup-chunk-max-items)
            rollup_chunk_override="${2:-}"
            shift 2
            ;;
        --rollup-workspace)
            rollup_workspace_override="${2:-}"
            shift 2
            ;;
        --rollup-project)
            rollup_project_override="${2:-}"
            shift 2
            ;;
        --rollup-kind)
            rollup_kind_override="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "unknown arg: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "${db_path}" || -z "${run_dir}" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -x "${MEM_CLI}" ]]; then
    echo "mem_cli not found at ${MEM_CLI}" >&2
    echo "build it with: make -C shared/core/core_memdb all" >&2
    exit 1
fi

mkdir -p "${run_dir}"
if [[ -z "${plan_path}" ]]; then
    plan_path="${run_dir}/pruner_plan.json"
fi
if [[ ! -f "${plan_path}" ]]; then
    echo "plan file not found: ${plan_path}" >&2
    exit 1
fi
if [[ -z "${session_id}" ]]; then
    session_id="mem-nightly-$(date +%Y%m%d)"
fi

if ! jq -e '((.version | type) == "number") and (.version >= 1) and (.version <= 2)' "${plan_path}" > /dev/null; then
    echo "plan validation failed: expected numeric version in [1,2]" >&2
    exit 1
fi

health_before_path="${run_dir}/pruner_health_before.json"
health_after_path="${run_dir}/pruner_health_after.json"
report_path="${run_dir}/pruner_apply_report.json"

"${MEM_CLI}" health --db "${db_path}" --format json > "${health_before_path}"

max_links_added="$(jq '.budgets.max_links_added // 20' "${plan_path}")"
max_links_updated="$(jq '.budgets.max_links_updated // 20' "${plan_path}")"
max_rollups="$(jq '.budgets.max_rollups // 1' "${plan_path}")"
max_nodes_affected="$(jq '.budgets.max_nodes_affected // 40' "${plan_path}")"

if [[ -n "${budget_links_added_override}" ]]; then
    max_links_added="${budget_links_added_override}"
fi
if [[ -n "${budget_links_updated_override}" ]]; then
    max_links_updated="${budget_links_updated_override}"
fi
if [[ -n "${budget_rollups_override}" ]]; then
    max_rollups="${budget_rollups_override}"
fi
if [[ -n "${budget_nodes_override}" ]]; then
    max_nodes_affected="${budget_nodes_override}"
fi

link_add_ops_json="$(mktemp)"
link_update_ops_json="$(mktemp)"

jq '
    if (.operations.link_additions | type) == "array" then
        .operations.link_additions
    elif (.operations.link_additions | type) == "object" then
        if ((.operations.link_additions.enabled // true) | not) then
            []
        elif ((.operations.link_additions.items // null) | type) == "array" then
            .operations.link_additions.items
        else
            []
        end
    else
        []
    end
' "${plan_path}" > "${link_add_ops_json}"

jq '
    if (.operations.link_updates | type) == "array" then
        .operations.link_updates
    elif (.operations.link_updates | type) == "object" then
        if ((.operations.link_updates.enabled // true) | not) then
            []
        elif ((.operations.link_updates.items // null) | type) == "array" then
            .operations.link_updates.items
        else
            []
        end
    else
        []
    end
' "${plan_path}" > "${link_update_ops_json}"

planned_link_add_count="$(jq 'length' "${link_add_ops_json}")"
planned_link_update_count="$(jq 'length' "${link_update_ops_json}")"
rollup_enabled="$(jq -r '.operations.rollup.enabled // false' "${plan_path}")"
rollup_before_ns="$(jq -r '.operations.rollup.before_ns // .proposals.rollup.before_ns // 0' "${plan_path}")"
eligible_rollup_count="$(jq '.proposals.rollup.eligible_count // 0' "${plan_path}")"
rollup_chunk_max_items="$(jq '.operations.rollup.chunk_max_items // 0' "${plan_path}")"
rollup_scope_workspace="$(jq -r '.operations.rollup.scope.workspace // .workspace // ""' "${plan_path}")"
rollup_scope_project="$(jq -r '.operations.rollup.scope.project // .project // ""' "${plan_path}")"
rollup_scope_kind="$(jq -r '.operations.rollup.scope.kind // ""' "${plan_path}")"
connection_pass_enabled="$(jq -r '.operations.connection_pass.enabled // false' "${plan_path}")"
connection_link_kind="$(jq -r '.operations.connection_pass.link_kind // "related"' "${plan_path}")"
connection_anchor_kind="$(jq -r '.operations.connection_pass.anchor_kind // "summarizes"' "${plan_path}")"
connection_anchor_item_id="$(jq '.operations.connection_pass.anchor_item_id // 0' "${plan_path}")"
connection_ensure_min_degree="$(jq -r '.operations.connection_pass.ensure_min_degree // true' "${plan_path}")"
connection_propagate_neighbor_links="$(jq -r '.operations.connection_pass.propagate_neighbor_links // false' "${plan_path}")"
connection_max_neighbor_links_per_rollup="$(jq '.operations.connection_pass.max_neighbor_links_per_rollup // 0' "${plan_path}")"
connection_neighbor_scan_max_edges="$(jq '.operations.connection_pass.neighbor_scan_max_edges // 24' "${plan_path}")"
connection_neighbor_scan_max_nodes="$(jq '.operations.connection_pass.neighbor_scan_max_nodes // 16' "${plan_path}")"

if [[ -n "${rollup_chunk_override}" ]]; then
    rollup_chunk_max_items="${rollup_chunk_override}"
fi
if [[ -n "${rollup_workspace_override}" ]]; then
    rollup_scope_workspace="${rollup_workspace_override}"
fi
if [[ -n "${rollup_project_override}" ]]; then
    rollup_scope_project="${rollup_project_override}"
fi
if [[ -n "${rollup_kind_override}" ]]; then
    rollup_scope_kind="${rollup_kind_override}"
fi

if [[ "${connection_pass_enabled}" != "true" && "${connection_pass_enabled}" != "false" ]]; then
    echo "invalid operations.connection_pass.enabled value: ${connection_pass_enabled}" >&2
    exit 1
fi
if ! is_allowed_link_kind "${connection_link_kind}"; then
    echo "invalid operations.connection_pass.link_kind value: ${connection_link_kind}" >&2
    exit 1
fi
if ! is_allowed_link_kind "${connection_anchor_kind}"; then
    echo "invalid operations.connection_pass.anchor_kind value: ${connection_anchor_kind}" >&2
    exit 1
fi
if [[ "${connection_anchor_item_id}" -lt 0 ]]; then
    echo "invalid operations.connection_pass.anchor_item_id value: ${connection_anchor_item_id}" >&2
    exit 1
fi
if [[ "${connection_ensure_min_degree}" != "true" && "${connection_ensure_min_degree}" != "false" ]]; then
    echo "invalid operations.connection_pass.ensure_min_degree value: ${connection_ensure_min_degree}" >&2
    exit 1
fi
if [[ "${connection_propagate_neighbor_links}" != "true" && "${connection_propagate_neighbor_links}" != "false" ]]; then
    echo "invalid operations.connection_pass.propagate_neighbor_links value: ${connection_propagate_neighbor_links}" >&2
    exit 1
fi
if [[ "${connection_max_neighbor_links_per_rollup}" -lt 0 ]]; then
    echo "invalid operations.connection_pass.max_neighbor_links_per_rollup value: ${connection_max_neighbor_links_per_rollup}" >&2
    exit 1
fi
if [[ "${connection_neighbor_scan_max_edges}" -lt 1 ]]; then
    echo "invalid operations.connection_pass.neighbor_scan_max_edges value: ${connection_neighbor_scan_max_edges}" >&2
    exit 1
fi
if [[ "${connection_neighbor_scan_max_nodes}" -lt 1 ]]; then
    echo "invalid operations.connection_pass.neighbor_scan_max_nodes value: ${connection_neighbor_scan_max_nodes}" >&2
    exit 1
fi

if (( planned_link_add_count > max_links_added )); then
    echo "budget violation: link additions (${planned_link_add_count}) exceed cap (${max_links_added})" >&2
    exit 1
fi
if (( planned_link_update_count > max_links_updated )); then
    echo "budget violation: link updates (${planned_link_update_count}) exceed cap (${max_links_updated})" >&2
    exit 1
fi

planned_rollups=0
if [[ "${rollup_enabled}" == "true" ]]; then
    if [[ "${rollup_before_ns}" -le 0 ]]; then
        echo "invalid rollup before_ns: ${rollup_before_ns}" >&2
        exit 1
    fi
    if [[ "${rollup_chunk_max_items}" -lt 0 ]]; then
        echo "invalid rollup chunk_max_items: ${rollup_chunk_max_items}" >&2
        exit 1
    fi
    if [[ "${eligible_rollup_count}" -gt "${max_nodes_affected}" ]]; then
        echo "budget violation: rollup eligible nodes (${eligible_rollup_count}) exceed cap (${max_nodes_affected})" >&2
        exit 1
    fi
    if [[ "${eligible_rollup_count}" -le 0 ]]; then
        planned_rollups=0
    elif [[ "${rollup_chunk_max_items}" -gt 0 ]]; then
        planned_rollups=$(( (eligible_rollup_count + rollup_chunk_max_items - 1) / rollup_chunk_max_items ))
    else
        planned_rollups=1
    fi
fi
if (( planned_rollups > max_rollups )); then
    echo "budget violation: rollups (${planned_rollups}) exceed cap (${max_rollups})" >&2
    exit 1
fi

planned_connection_links=0
if [[ "${rollup_enabled}" == "true" && "${connection_pass_enabled}" == "true" && "${planned_rollups}" -gt 0 ]]; then
    # Base pass connects rollup nodes as a chain plus one optional anchor link.
    planned_connection_links="${planned_rollups}"
    if [[ "${connection_propagate_neighbor_links}" == "true" && "${connection_max_neighbor_links_per_rollup}" -gt 0 ]]; then
        planned_connection_links=$(( planned_connection_links + (planned_rollups * connection_max_neighbor_links_per_rollup) ))
    fi
fi

planned_total_link_adds=$(( planned_link_add_count + planned_connection_links ))
if (( planned_total_link_adds > max_links_added )); then
    echo "budget violation: link additions + connection pass (${planned_total_link_adds}) exceed cap (${max_links_added})" >&2
    exit 1
fi

planned_total_ops=$(( planned_link_add_count + planned_link_update_count + planned_rollups + planned_connection_links ))
if [[ "${apply_mode}" == "true" && "${planned_total_ops}" -eq 0 ]]; then
    if [[ "${allow_empty_apply}" == "true" ]]; then
        echo "warning: apply mode requested with zero approved operations; continuing because --allow-empty-apply was set." >&2
    else
        echo "apply mode requested but plan has zero approved operations; refusing apply. pass --allow-empty-apply to permit intentional no-op apply." >&2
        exit 2
    fi
fi

results_ndjson="$(mktemp)"
results_json="$(mktemp)"
rollup_item_ids_file="$(mktemp)"
connection_targets_ndjson="$(mktemp)"
trap 'rm -f "${results_ndjson}" "${results_json}" "${rollup_item_ids_file}" "${connection_targets_ndjson}" "${link_add_ops_json}" "${link_update_ops_json}"' EXIT
touch "${results_ndjson}"
touch "${rollup_item_ids_file}"
touch "${connection_targets_ndjson}"

had_error=0
applied_link_add=0
applied_link_update=0
applied_rollup=0
applied_connection_links=0

if [[ "${apply_mode}" == "true" && "${rollup_enabled}" == "true" && "${connection_pass_enabled}" == "true" && "${connection_propagate_neighbor_links}" == "true" && "${planned_rollups}" -gt 0 && "${connection_max_neighbor_links_per_rollup}" -gt 0 ]]; then
    eligible_rollup_ids=()
    while IFS= read -r eligible_id; do
        if [[ -n "${eligible_id}" ]]; then
            eligible_rollup_ids+=( "${eligible_id}" )
        fi
    done < <(jq -r '.proposals.rollup.eligible_item_ids[]? | select(type=="number") | tostring' "${plan_path}")
    if (( ${#eligible_rollup_ids[@]} > 0 )); then
        stale_id_tokens="|"
        for stale_id in "${eligible_rollup_ids[@]}"; do
            stale_id_tokens+="${stale_id}|"
        done

        planned_nodes_count="${#eligible_rollup_ids[@]}"
        if (( planned_nodes_count > max_nodes_affected )); then
            planned_nodes_count="${max_nodes_affected}"
        fi

        plan_group_chunk="${rollup_chunk_max_items}"
        if (( plan_group_chunk <= 0 )); then
            plan_group_chunk="${planned_nodes_count}"
        fi
        if (( plan_group_chunk <= 0 )); then
            plan_group_chunk=1
        fi

        plan_group_index=0
        plan_offset=0
        while (( plan_offset < planned_nodes_count && plan_group_index < max_rollups )); do
            plan_remaining=$(( planned_nodes_count - plan_offset ))
            plan_group_size="${plan_group_chunk}"
            if (( plan_group_size > plan_remaining )); then
                plan_group_size="${plan_remaining}"
            fi
            if (( plan_group_size <= 0 )); then
                break
            fi

            plan_group_sources=( "${eligible_rollup_ids[@]:plan_offset:plan_group_size}" )
            plan_group_targets=()
            plan_group_target_tokens="|"
            plan_group_scan_errors=0

            for source_item_id in "${plan_group_sources[@]}"; do
                set +e
                source_neighbors_json="$("${MEM_CLI}" neighbors --db "${db_path}" --item-id "${source_item_id}" --max-edges "${connection_neighbor_scan_max_edges}" --max-nodes "${connection_neighbor_scan_max_nodes}" --format json 2>&1)"
                source_neighbors_status=$?
                set -e
                if (( source_neighbors_status != 0 )); then
                    plan_group_scan_errors=$(( plan_group_scan_errors + 1 ))
                    continue
                fi

                while IFS= read -r neighbor_item_id; do
                    if [[ -z "${neighbor_item_id}" || "${neighbor_item_id}" == "null" ]]; then
                        continue
                    fi
                    if [[ "${neighbor_item_id}" == "${source_item_id}" ]]; then
                        continue
                    fi
                    if [[ "${stale_id_tokens}" == *"|${neighbor_item_id}|"* ]]; then
                        continue
                    fi
                    if [[ "${plan_group_target_tokens}" == *"|${neighbor_item_id}|"* ]]; then
                        continue
                    fi
                    plan_group_targets+=( "${neighbor_item_id}" )
                    plan_group_target_tokens+="${neighbor_item_id}|"
                    if (( ${#plan_group_targets[@]} >= connection_max_neighbor_links_per_rollup )); then
                        break
                    fi
                done < <(printf '%s\n' "${source_neighbors_json}" | jq -r '.[]? | (.neighbor_id // empty) | tostring')

                if (( ${#plan_group_targets[@]} >= connection_max_neighbor_links_per_rollup )); then
                    break
                fi
            done

            plan_group_sources_json="$(printf '%s\n' "${plan_group_sources[@]}" | jq -Rsc 'split("\n") | map(select(length>0) | tonumber)')"
            plan_group_targets_json="$(printf '%s\n' "${plan_group_targets[@]}" | jq -Rsc 'split("\n") | map(select(length>0) | tonumber)')"
            jq -nc \
                --argjson group_index "${plan_group_index}" \
                --argjson source_item_ids "${plan_group_sources_json}" \
                --argjson target_item_ids "${plan_group_targets_json}" \
                --argjson scan_errors "${plan_group_scan_errors}" \
                '{group_index:$group_index,source_item_ids:$source_item_ids,target_item_ids:$target_item_ids,scan_errors:$scan_errors}' >> "${connection_targets_ndjson}"

            plan_offset=$(( plan_offset + plan_group_size ))
            plan_group_index=$(( plan_group_index + 1 ))
        done
    fi
fi

while IFS= read -r op; do
    from_id="$(jq -r '.from' <<<"${op}")"
    to_id="$(jq -r '.to' <<<"${op}")"
    kind="$(jq -r '.kind' <<<"${op}")"
    note="$(jq -r '.note // ""' <<<"${op}")"
    if [[ -z "${from_id}" || -z "${to_id}" || -z "${kind}" || "${from_id}" == "null" || "${to_id}" == "null" || "${kind}" == "null" ]]; then
        echo "invalid link_add operation: ${op}" >&2
        had_error=1
        continue
    fi

    cmd=( "${MEM_CLI}" link-add --db "${db_path}" --from "${from_id}" --to "${to_id}" --kind "${kind}" --session-id "${session_id}" )
    if [[ -n "${note}" ]]; then
        cmd+=( --note "${note}" )
    fi

    if [[ "${apply_mode}" == "true" ]]; then
        set +e
        cmd_output="$("${cmd[@]}" 2>&1)"
        cmd_status=$?
        set -e
        if (( cmd_status == 0 )); then
            result_status="ok"
            applied_link_add=$(( applied_link_add + 1 ))
        else
            result_status="error"
            had_error=1
        fi
    else
        cmd_output="dry-run (not applied)"
        result_status="planned"
    fi

    jq -nc \
        --arg op_type "link-add" \
        --arg status "${result_status}" \
        --arg command "${cmd[*]}" \
        --arg output "${cmd_output}" \
        --argjson from "${from_id}" \
        --argjson to "${to_id}" \
        --arg kind "${kind}" \
        '{op_type:$op_type,status:$status,command:$command,output:$output,from:$from,to:$to,kind:$kind}' >> "${results_ndjson}"
done < <(jq -c '.[]?' "${link_add_ops_json}")

while IFS= read -r op; do
    link_id="$(jq -r '.id' <<<"${op}")"
    kind="$(jq -r '.kind' <<<"${op}")"
    note="$(jq -r '.note // ""' <<<"${op}")"
    weight="$(jq -r '.weight // ""' <<<"${op}")"
    if [[ -z "${link_id}" || -z "${kind}" || "${link_id}" == "null" || "${kind}" == "null" ]]; then
        echo "invalid link_update operation: ${op}" >&2
        had_error=1
        continue
    fi

    cmd=( "${MEM_CLI}" link-update --db "${db_path}" --id "${link_id}" --kind "${kind}" --session-id "${session_id}" )
    if [[ -n "${weight}" ]]; then
        cmd+=( --weight "${weight}" )
    fi
    if [[ -n "${note}" ]]; then
        cmd+=( --note "${note}" )
    fi

    if [[ "${apply_mode}" == "true" ]]; then
        set +e
        cmd_output="$("${cmd[@]}" 2>&1)"
        cmd_status=$?
        set -e
        if (( cmd_status == 0 )); then
            result_status="ok"
            applied_link_update=$(( applied_link_update + 1 ))
        else
            result_status="error"
            had_error=1
        fi
    else
        cmd_output="dry-run (not applied)"
        result_status="planned"
    fi

    jq -nc \
        --arg op_type "link-update" \
        --arg status "${result_status}" \
        --arg command "${cmd[*]}" \
        --arg output "${cmd_output}" \
        --argjson id "${link_id}" \
        --arg kind "${kind}" \
        '{op_type:$op_type,status:$status,command:$command,output:$output,id:$id,kind:$kind}' >> "${results_ndjson}"
done < <(jq -c '.[]?' "${link_update_ops_json}")

if [[ "${rollup_enabled}" == "true" ]]; then
    if [[ "${apply_mode}" != "true" ]]; then
        cmd=( "${MEM_CLI}" rollup --db "${db_path}" --before "${rollup_before_ns}" --session-id "${session_id}" )
        if [[ -n "${rollup_scope_workspace}" ]]; then
            cmd+=( --workspace "${rollup_scope_workspace}" )
        fi
        if [[ -n "${rollup_scope_project}" ]]; then
            cmd+=( --project "${rollup_scope_project}" )
        fi
        if [[ -n "${rollup_scope_kind}" ]]; then
            cmd+=( --kind "${rollup_scope_kind}" )
        fi
        if [[ "${rollup_chunk_max_items}" -gt 0 ]]; then
            cmd+=( --limit "${rollup_chunk_max_items}" )
        fi
        jq -nc \
            --arg op_type "rollup" \
            --arg status "planned" \
            --arg command "${cmd[*]}" \
            --arg output "dry-run (not applied)" \
            --arg before_ns "${rollup_before_ns}" \
            --argjson eligible_count "${eligible_rollup_count}" \
            --argjson planned_runs "${planned_rollups}" \
            --arg workspace "${rollup_scope_workspace}" \
            --arg project "${rollup_scope_project}" \
            --arg kind "${rollup_scope_kind}" \
            --argjson chunk_max_items "${rollup_chunk_max_items}" \
            '{op_type:$op_type,status:$status,command:$command,output:$output,before_ns:$before_ns,eligible_count:$eligible_count,planned_runs:$planned_runs,workspace:$workspace,project:$project,kind:$kind,chunk_max_items:$chunk_max_items}' >> "${results_ndjson}"
    else
        rolled_nodes_total=0
        while :; do
            if (( applied_rollup >= max_rollups )); then
                if (( rolled_nodes_total < eligible_rollup_count )); then
                    had_error=1
                    jq -nc \
                        --arg op_type "rollup" \
                        --arg status "error" \
                        --arg command "" \
                        --arg output "budget violation: runtime rollup count reached max_rollups before processing eligible set" \
                        --arg before_ns "${rollup_before_ns}" \
                        --argjson eligible_count "${eligible_rollup_count}" \
                        --argjson rolled_total "${rolled_nodes_total}" \
                        '{op_type:$op_type,status:$status,command:$command,output:$output,before_ns:$before_ns,eligible_count:$eligible_count,rolled_total:$rolled_total}' >> "${results_ndjson}"
                fi
                break
            fi

            remaining_nodes=$(( max_nodes_affected - rolled_nodes_total ))
            if (( remaining_nodes <= 0 )); then
                break
            fi

            iteration_limit="${rollup_chunk_max_items}"
            if (( iteration_limit <= 0 || iteration_limit > remaining_nodes )); then
                iteration_limit="${remaining_nodes}"
            fi

            cmd=( "${MEM_CLI}" rollup --db "${db_path}" --before "${rollup_before_ns}" --session-id "${session_id}" --limit "${iteration_limit}" )
            if [[ -n "${rollup_scope_workspace}" ]]; then
                cmd+=( --workspace "${rollup_scope_workspace}" )
            fi
            if [[ -n "${rollup_scope_project}" ]]; then
                cmd+=( --project "${rollup_scope_project}" )
            fi
            if [[ -n "${rollup_scope_kind}" ]]; then
                cmd+=( --kind "${rollup_scope_kind}" )
            fi

            set +e
            cmd_output="$("${cmd[@]}" 2>&1)"
            cmd_status=$?
            set -e
            if (( cmd_status != 0 )); then
                had_error=1
                jq -nc \
                    --arg op_type "rollup" \
                    --arg status "error" \
                    --arg command "${cmd[*]}" \
                    --arg output "${cmd_output}" \
                    --arg before_ns "${rollup_before_ns}" \
                    --argjson eligible_count "${eligible_rollup_count}" \
                    --argjson rolled_total "${rolled_nodes_total}" \
                    '{op_type:$op_type,status:$status,command:$command,output:$output,before_ns:$before_ns,eligible_count:$eligible_count,rolled_total:$rolled_total}' >> "${results_ndjson}"
                break
            fi

            if [[ "${cmd_output}" == rollup:\ no\ eligible\ rows\ before* ]]; then
                jq -nc \
                    --arg op_type "rollup" \
                    --arg status "noop" \
                    --arg command "${cmd[*]}" \
                    --arg output "${cmd_output}" \
                    --arg before_ns "${rollup_before_ns}" \
                    --argjson eligible_count "${eligible_rollup_count}" \
                    --argjson rolled_total "${rolled_nodes_total}" \
                    '{op_type:$op_type,status:$status,command:$command,output:$output,before_ns:$before_ns,eligible_count:$eligible_count,rolled_total:$rolled_total}' >> "${results_ndjson}"
                break
            fi

            if [[ "${cmd_output}" =~ rolled\ up\ ([0-9]+)\ rows\ into\ id=([0-9]+) ]]; then
                rolled_this_run="${BASH_REMATCH[1]}"
                rollup_item_id="${BASH_REMATCH[2]}"
                printf '%s\n' "${rollup_item_id}" >> "${rollup_item_ids_file}"
                rolled_nodes_total=$(( rolled_nodes_total + rolled_this_run ))
                applied_rollup=$(( applied_rollup + 1 ))
                jq -nc \
                    --arg op_type "rollup" \
                    --arg status "ok" \
                    --arg command "${cmd[*]}" \
                    --arg output "${cmd_output}" \
                    --arg before_ns "${rollup_before_ns}" \
                    --arg workspace "${rollup_scope_workspace}" \
                    --arg project "${rollup_scope_project}" \
                    --arg kind "${rollup_scope_kind}" \
                    --argjson eligible_count "${eligible_rollup_count}" \
                    --argjson rolled_count "${rolled_this_run}" \
                    --argjson rolled_total "${rolled_nodes_total}" \
                    --argjson rollup_item_id "${rollup_item_id}" \
                    '{op_type:$op_type,status:$status,command:$command,output:$output,before_ns:$before_ns,workspace:$workspace,project:$project,kind:$kind,eligible_count:$eligible_count,rolled_count:$rolled_count,rolled_total:$rolled_total,rollup_item_id:$rollup_item_id}' >> "${results_ndjson}"
                if (( rolled_this_run <= 0 )); then
                    break
                fi
                continue
            fi

            had_error=1
            jq -nc \
                --arg op_type "rollup" \
                --arg status "error" \
                --arg command "${cmd[*]}" \
                --arg output "unexpected rollup output shape" \
                --arg raw_output "${cmd_output}" \
                --arg before_ns "${rollup_before_ns}" \
                '{op_type:$op_type,status:$status,command:$command,output:$output,raw_output:$raw_output,before_ns:$before_ns}' >> "${results_ndjson}"
            break
        done
    fi
fi

if [[ "${rollup_enabled}" == "true" && "${connection_pass_enabled}" == "true" ]]; then
    if [[ "${apply_mode}" != "true" ]]; then
        jq -nc \
            --arg op_type "connection-pass" \
            --arg status "planned" \
            --arg output "dry-run (not applied)" \
            --arg kind "${connection_link_kind}" \
            --arg anchor_kind "${connection_anchor_kind}" \
            --argjson ensure_min_degree "${connection_ensure_min_degree}" \
            --argjson propagate_neighbor_links "${connection_propagate_neighbor_links}" \
            --argjson max_neighbor_links_per_rollup "${connection_max_neighbor_links_per_rollup}" \
            --argjson neighbor_scan_max_edges "${connection_neighbor_scan_max_edges}" \
            --argjson neighbor_scan_max_nodes "${connection_neighbor_scan_max_nodes}" \
            --argjson planned_connection_links "${planned_connection_links}" \
            '{op_type:$op_type,status:$status,output:$output,kind:$kind,anchor_kind:$anchor_kind,ensure_min_degree:$ensure_min_degree,propagate_neighbor_links:$propagate_neighbor_links,max_neighbor_links_per_rollup:$max_neighbor_links_per_rollup,neighbor_scan_max_edges:$neighbor_scan_max_edges,neighbor_scan_max_nodes:$neighbor_scan_max_nodes,planned_connection_links:$planned_connection_links}' >> "${results_ndjson}"
    else
        rollup_item_ids=()
        while IFS= read -r rid; do
            if [[ -n "${rid}" ]]; then
                rollup_item_ids+=( "${rid}" )
            fi
        done < "${rollup_item_ids_file}"

        if (( ${#rollup_item_ids[@]} > 0 )); then
            rollup_id_tokens="|"
            for rid in "${rollup_item_ids[@]}"; do
                rollup_id_tokens+="${rid}|"
            done

            anchor_item_id="${connection_anchor_item_id}"
            if (( anchor_item_id <= 0 )); then
                query_args=( "${MEM_CLI}" query --db "${db_path}" --limit 20 --format json )
                if [[ -n "${rollup_scope_workspace}" ]]; then
                    query_args+=( --workspace "${rollup_scope_workspace}" )
                fi
                if [[ -n "${rollup_scope_project}" ]]; then
                    query_args+=( --project "${rollup_scope_project}" )
                fi
                scope_json="$("${query_args[@]}" --kind scope)"
                while IFS= read -r candidate_id; do
                    if [[ -n "${candidate_id}" && "${candidate_id}" != "null" && "${rollup_id_tokens}" != *"|${candidate_id}|"* ]]; then
                        anchor_item_id="${candidate_id}"
                        break
                    fi
                done < <(printf '%s\n' "${scope_json}" | jq -r '.[].id')

                if (( anchor_item_id <= 0 )); then
                    recent_json="$("${query_args[@]}")"
                    while IFS= read -r candidate_id; do
                        if [[ -n "${candidate_id}" && "${candidate_id}" != "null" && "${rollup_id_tokens}" != *"|${candidate_id}|"* ]]; then
                            anchor_item_id="${candidate_id}"
                            break
                        fi
                    done < <(printf '%s\n' "${recent_json}" | jq -r '.[].id')
                fi
            fi

            if (( anchor_item_id <= 0 )); then
                jq -nc \
                    --arg op_type "connection-pass" \
                    --arg status "noop" \
                    --arg output "no non-rollup anchor item found in scope; chain-only connectivity will be used" \
                    --argjson rollup_count "${#rollup_item_ids[@]}" \
                    '{op_type:$op_type,status:$status,output:$output,rollup_count:$rollup_count}' >> "${results_ndjson}"
            fi

            if (( anchor_item_id > 0 )); then
                first_rollup_id="${rollup_item_ids[0]}"
                if [[ "${first_rollup_id}" != "${anchor_item_id}" ]]; then
                    if (( applied_link_add >= max_links_added )); then
                        had_error=1
                        jq -nc \
                            --arg op_type "connection-link-add" \
                            --arg status "error" \
                            --arg output "budget violation: max_links_added reached before adding anchor link" \
                            --argjson from "${first_rollup_id}" \
                            --argjson to "${anchor_item_id}" \
                            --arg kind "${connection_anchor_kind}" \
                            '{op_type:$op_type,status:$status,output:$output,from:$from,to:$to,kind:$kind}' >> "${results_ndjson}"
                    else
                        cmd=( "${MEM_CLI}" link-add --db "${db_path}" --from "${first_rollup_id}" --to "${anchor_item_id}" --kind "${connection_anchor_kind}" --session-id "${session_id}" --note "nightly connection pass anchor link" )
                        set +e
                        cmd_output="$("${cmd[@]}" 2>&1)"
                        cmd_status=$?
                        set -e
                        if (( cmd_status == 0 )); then
                            result_status="ok"
                            applied_link_add=$(( applied_link_add + 1 ))
                            applied_connection_links=$(( applied_connection_links + 1 ))
                        else
                            result_status="error"
                            had_error=1
                        fi
                        jq -nc \
                            --arg op_type "connection-link-add" \
                            --arg status "${result_status}" \
                            --arg command "${cmd[*]}" \
                            --arg output "${cmd_output}" \
                            --argjson from "${first_rollup_id}" \
                            --argjson to "${anchor_item_id}" \
                            --arg kind "${connection_anchor_kind}" \
                            --arg phase "anchor" \
                            '{op_type:$op_type,status:$status,command:$command,output:$output,from:$from,to:$to,kind:$kind,phase:$phase}' >> "${results_ndjson}"
                    fi
                fi
            fi

            if (( ${#rollup_item_ids[@]} > 1 )); then
                idx=1
                while (( idx < ${#rollup_item_ids[@]} )); do
                    from_id="${rollup_item_ids[$((idx - 1))]}"
                    to_id="${rollup_item_ids[$idx]}"
                    if (( applied_link_add >= max_links_added )); then
                        had_error=1
                        jq -nc \
                            --arg op_type "connection-link-add" \
                            --arg status "error" \
                            --arg output "budget violation: max_links_added reached during rollup chain linking" \
                            --argjson from "${from_id}" \
                            --argjson to "${to_id}" \
                            --arg kind "${connection_link_kind}" \
                            '{op_type:$op_type,status:$status,output:$output,from:$from,to:$to,kind:$kind}' >> "${results_ndjson}"
                        break
                    fi
                    cmd=( "${MEM_CLI}" link-add --db "${db_path}" --from "${from_id}" --to "${to_id}" --kind "${connection_link_kind}" --session-id "${session_id}" --note "nightly rollup chain link" )
                    set +e
                    cmd_output="$("${cmd[@]}" 2>&1)"
                    cmd_status=$?
                    set -e
                    if (( cmd_status == 0 )); then
                        result_status="ok"
                        applied_link_add=$(( applied_link_add + 1 ))
                        applied_connection_links=$(( applied_connection_links + 1 ))
                    else
                        result_status="error"
                        had_error=1
                    fi
                    jq -nc \
                        --arg op_type "connection-link-add" \
                        --arg status "${result_status}" \
                        --arg command "${cmd[*]}" \
                        --arg output "${cmd_output}" \
                        --argjson from "${from_id}" \
                        --argjson to "${to_id}" \
                        --arg kind "${connection_link_kind}" \
                        --arg phase "chain" \
                        '{op_type:$op_type,status:$status,command:$command,output:$output,from:$from,to:$to,kind:$kind,phase:$phase}' >> "${results_ndjson}"
                    idx=$(( idx + 1 ))
                done
            fi

            if [[ "${connection_propagate_neighbor_links}" == "true" && "${connection_max_neighbor_links_per_rollup}" -gt 0 ]]; then
                neighbor_group_index=0
                while (( neighbor_group_index < ${#rollup_item_ids[@]} )); do
                    rollup_id="${rollup_item_ids[$neighbor_group_index]}"
                    while IFS= read -r target_id; do
                        if [[ -z "${target_id}" || "${target_id}" == "null" ]]; then
                            continue
                        fi
                        if [[ "${target_id}" == "${rollup_id}" ]]; then
                            continue
                        fi
                        if [[ "${rollup_id_tokens}" == *"|${target_id}|"* ]]; then
                            continue
                        fi
                        if (( applied_link_add >= max_links_added )); then
                            had_error=1
                            jq -nc \
                                --arg op_type "connection-neighbor-link-add" \
                                --arg status "error" \
                                --arg output "budget violation: max_links_added reached during neighbor-link propagation" \
                                --argjson from "${rollup_id}" \
                                --argjson to "${target_id}" \
                                --arg kind "${connection_link_kind}" \
                                --argjson group_index "${neighbor_group_index}" \
                                '{op_type:$op_type,status:$status,output:$output,from:$from,to:$to,kind:$kind,group_index:$group_index}' >> "${results_ndjson}"
                            break
                        fi
                        cmd=( "${MEM_CLI}" link-add --db "${db_path}" --from "${rollup_id}" --to "${target_id}" --kind "${connection_link_kind}" --session-id "${session_id}" --note "nightly neighbor propagation link" )
                        set +e
                        cmd_output="$("${cmd[@]}" 2>&1)"
                        cmd_status=$?
                        set -e
                        if (( cmd_status == 0 )); then
                            result_status="ok"
                            applied_link_add=$(( applied_link_add + 1 ))
                            applied_connection_links=$(( applied_connection_links + 1 ))
                        elif [[ "${cmd_output}" == *"UNIQUE constraint failed"* || "${cmd_output}" == *"already exists"* || "${cmd_output}" == *"duplicate"* ]]; then
                            result_status="noop"
                        else
                            result_status="error"
                            had_error=1
                        fi
                        jq -nc \
                            --arg op_type "connection-neighbor-link-add" \
                            --arg status "${result_status}" \
                            --arg command "${cmd[*]}" \
                            --arg output "${cmd_output}" \
                            --argjson from "${rollup_id}" \
                            --argjson to "${target_id}" \
                            --arg kind "${connection_link_kind}" \
                            --argjson group_index "${neighbor_group_index}" \
                            '{op_type:$op_type,status:$status,command:$command,output:$output,from:$from,to:$to,kind:$kind,group_index:$group_index}' >> "${results_ndjson}"
                    done < <(jq -r --argjson idx "${neighbor_group_index}" 'select(.group_index == $idx) | .target_item_ids[]?' "${connection_targets_ndjson}")
                    neighbor_group_index=$(( neighbor_group_index + 1 ))
                done
            fi

            if [[ "${connection_ensure_min_degree}" == "true" ]]; then
                for rollup_id in "${rollup_item_ids[@]}"; do
                    set +e
                    links_output="$("${MEM_CLI}" link-list --db "${db_path}" --item-id "${rollup_id}" 2>&1)"
                    links_status=$?
                    set -e
                    if (( links_status != 0 )); then
                        had_error=1
                        jq -nc \
                            --arg op_type "connection-check" \
                            --arg status "error" \
                            --arg output "${links_output}" \
                            --argjson item_id "${rollup_id}" \
                            '{op_type:$op_type,status:$status,output:$output,item_id:$item_id}' >> "${results_ndjson}"
                        continue
                    fi
                    if [[ "${links_output}" == no\ links\ for\ item_id=* ]]; then
                        target_id="${anchor_item_id}"
                        if (( target_id <= 0 )); then
                            if (( ${#rollup_item_ids[@]} > 1 )); then
                                target_id="${rollup_item_ids[0]}"
                            else
                                target_id=0
                            fi
                        fi
                        if (( target_id > 0 )) && [[ "${target_id}" != "${rollup_id}" ]]; then
                            if (( applied_link_add >= max_links_added )); then
                                had_error=1
                                jq -nc \
                                    --arg op_type "connection-link-add" \
                                    --arg status "error" \
                                    --arg output "budget violation: max_links_added reached during min-degree enforcement" \
                                    --argjson from "${rollup_id}" \
                                    --argjson to "${target_id}" \
                                    --arg kind "${connection_link_kind}" \
                                    '{op_type:$op_type,status:$status,output:$output,from:$from,to:$to,kind:$kind}' >> "${results_ndjson}"
                                break
                            fi
                            cmd=( "${MEM_CLI}" link-add --db "${db_path}" --from "${rollup_id}" --to "${target_id}" --kind "${connection_link_kind}" --session-id "${session_id}" --note "nightly min-degree fallback link" )
                            set +e
                            cmd_output="$("${cmd[@]}" 2>&1)"
                            cmd_status=$?
                            set -e
                            if (( cmd_status == 0 )); then
                                result_status="ok"
                                applied_link_add=$(( applied_link_add + 1 ))
                                applied_connection_links=$(( applied_connection_links + 1 ))
                            else
                                result_status="error"
                                had_error=1
                            fi
                            jq -nc \
                                --arg op_type "connection-link-add" \
                                --arg status "${result_status}" \
                                --arg command "${cmd[*]}" \
                                --arg output "${cmd_output}" \
                                --argjson from "${rollup_id}" \
                                --argjson to "${target_id}" \
                                --arg kind "${connection_link_kind}" \
                                --arg phase "min-degree" \
                                '{op_type:$op_type,status:$status,command:$command,output:$output,from:$from,to:$to,kind:$kind,phase:$phase}' >> "${results_ndjson}"
                        else
                            had_error=1
                            jq -nc \
                                --arg op_type "connection-check" \
                                --arg status "error" \
                                --arg output "unable to enforce min-degree: no valid anchor/target found for unlinked rollup node" \
                                --argjson item_id "${rollup_id}" \
                                '{op_type:$op_type,status:$status,output:$output,item_id:$item_id}' >> "${results_ndjson}"
                        fi
                    fi
                done
            fi
        fi
    fi
fi

"${MEM_CLI}" health --db "${db_path}" --format json > "${health_after_path}"

jq -s '.' "${results_ndjson}" > "${results_json}"
generated_at_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

jq -n \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg db_path "${db_path}" \
    --arg plan_path "${plan_path}" \
    --arg session_id "${session_id}" \
    --argjson apply "${apply_mode}" \
    --argjson had_error "${had_error}" \
    --argjson max_links_added "${max_links_added}" \
    --argjson max_links_updated "${max_links_updated}" \
    --argjson max_rollups "${max_rollups}" \
    --argjson max_nodes_affected "${max_nodes_affected}" \
    --argjson planned_link_add_count "${planned_link_add_count}" \
    --argjson planned_connection_links "${planned_connection_links}" \
    --argjson planned_link_update_count "${planned_link_update_count}" \
    --argjson planned_rollups "${planned_rollups}" \
    --argjson eligible_rollup_count "${eligible_rollup_count}" \
    --argjson rollup_chunk_max_items "${rollup_chunk_max_items}" \
    --arg rollup_scope_workspace "${rollup_scope_workspace}" \
    --arg rollup_scope_project "${rollup_scope_project}" \
    --arg rollup_scope_kind "${rollup_scope_kind}" \
    --argjson applied_link_add "${applied_link_add}" \
    --argjson applied_connection_links "${applied_connection_links}" \
    --argjson applied_link_update "${applied_link_update}" \
    --argjson applied_rollup "${applied_rollup}" \
    --argjson rollup_enabled "${rollup_enabled}" \
    --slurpfile health_before "${health_before_path}" \
    --slurpfile health_after "${health_after_path}" \
    --slurpfile results "${results_json}" \
    '{
        generated_at_utc: $generated_at_utc,
        db_path: $db_path,
        plan_path: $plan_path,
        session_id: $session_id,
        apply: $apply,
        status: (if $had_error > 0 then "failed" else "ok" end),
        budgets: {
            max_links_added: $max_links_added,
            max_links_updated: $max_links_updated,
            max_rollups: $max_rollups,
            max_nodes_affected: $max_nodes_affected
        },
        planned_operations: {
            link_additions: $planned_link_add_count,
            connection_links: $planned_connection_links,
            link_updates: $planned_link_update_count,
            rollup_planned_runs: $planned_rollups,
            rollup_enabled: $rollup_enabled,
            rollup_eligible_count: $eligible_rollup_count,
            rollup_chunk_max_items: $rollup_chunk_max_items,
            rollup_scope: {
                workspace: $rollup_scope_workspace,
                project: $rollup_scope_project,
                kind: $rollup_scope_kind
            }
        },
        applied_operations: {
            link_additions: $applied_link_add,
            connection_links: $applied_connection_links,
            link_updates: $applied_link_update,
            rollups: $applied_rollup
        },
        health_before: $health_before[0],
        health_after: $health_after[0],
        operation_results: $results[0]
    }' > "${report_path}"

echo "pruner complete:"
echo "  report: ${report_path}"
if (( had_error > 0 )); then
    exit 1
fi
