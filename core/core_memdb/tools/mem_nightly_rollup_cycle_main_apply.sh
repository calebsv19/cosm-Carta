#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
RUN_CODEX="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_run_codex.sh"

usage() {
    cat <<'EOF'
usage: mem_nightly_rollup_cycle_main_apply.sh [options]

Purpose:
  Run an official Codex-guided rollup cycle directly on MAIN DB:
  1) run nightly Codex dry pass on main DB
  2) auto-approve reviewed plan with scoped/chunked rollup + connection pass
  3) locked apply on same run dir and same main DB
  4) print compact apply summary (including rollup item IDs)

options:
  --db <path>                    Default: /Users/calebsv/Desktop/CodeWork/data/codework_mem_console.sqlite
  --runs-root <dir>              Default: docs/private_program_docs/memory_console/nightly_runs
  --run-dir <dir>                Optional explicit run dir
  --workspace <key>              Default: codework
  --project <key>                Default: memory_console
  --stale-days <n>               Default: 30
  --min-active-nodes-before-rollup <n>
                                 Default: 40
  --min-stale-candidates-before-rollup <n>
                                 Default: 4
  --chunk-max-items <n>          Default: 12
  --model <name>                 Optional codex model override
  --skip-reader-codex            Pass-through to codex run wrapper
  --skip-pruner-codex            Pass-through to codex run wrapper
  --allow-non-main-filename      Allow DB filename other than codework_mem_console.sqlite
  --confirm-main-apply           Required acknowledgement flag for real DB mutations
  -h, --help                     Show this help
EOF
}

db_path="/Users/calebsv/Desktop/CodeWork/data/codework_mem_console.sqlite"
runs_root="${ROOT_DIR}/docs/private_program_docs/memory_console/nightly_runs"
run_dir=""
workspace_key="codework"
project_key="memory_console"
stale_days=30
min_active_nodes_before_rollup=40
min_stale_candidates_before_rollup=4
chunk_max_items=12
model_name=""
skip_reader_codex=false
skip_pruner_codex=false
allow_non_main_filename=false
confirm_main_apply=false

abs_path() {
    local p="$1"
    local d
    d="$(dirname "$p")"
    mkdir -p "$d"
    echo "$(cd "$d" && pwd -P)/$(basename "$p")"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --db)
            db_path="${2:-}"
            shift 2
            ;;
        --runs-root)
            runs_root="${2:-}"
            shift 2
            ;;
        --run-dir)
            run_dir="${2:-}"
            shift 2
            ;;
        --workspace)
            workspace_key="${2:-}"
            shift 2
            ;;
        --project)
            project_key="${2:-}"
            shift 2
            ;;
        --stale-days)
            stale_days="${2:-}"
            shift 2
            ;;
        --min-active-nodes-before-rollup)
            min_active_nodes_before_rollup="${2:-}"
            shift 2
            ;;
        --min-stale-candidates-before-rollup)
            min_stale_candidates_before_rollup="${2:-}"
            shift 2
            ;;
        --chunk-max-items)
            chunk_max_items="${2:-}"
            shift 2
            ;;
        --model)
            model_name="${2:-}"
            shift 2
            ;;
        --skip-reader-codex)
            skip_reader_codex=true
            shift
            ;;
        --skip-pruner-codex)
            skip_pruner_codex=true
            shift
            ;;
        --allow-non-main-filename)
            allow_non_main_filename=true
            shift
            ;;
        --confirm-main-apply)
            confirm_main_apply=true
            shift
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

if [[ ! -x "${RUN_CODEX}" ]]; then
    echo "missing executable: ${RUN_CODEX}" >&2
    exit 1
fi
if [[ ! -f "${db_path}" ]]; then
    echo "main db not found: ${db_path}" >&2
    exit 1
fi
if [[ "${chunk_max_items}" -le 0 ]]; then
    echo "chunk-max-items must be > 0, got: ${chunk_max_items}" >&2
    exit 1
fi
if [[ "${confirm_main_apply}" != "true" ]]; then
    echo "refusing to mutate main DB without explicit confirmation." >&2
    echo "rerun with: --confirm-main-apply" >&2
    exit 1
fi

db_path="$(abs_path "${db_path}")"
runs_root="$(abs_path "${runs_root}")"

if [[ "${allow_non_main_filename}" != "true" ]]; then
    db_basename="$(basename "${db_path}")"
    if [[ "${db_basename}" != "codework_mem_console.sqlite" ]]; then
        echo "refusing to run: db filename must be codework_mem_console.sqlite for official main apply." >&2
        echo "set --allow-non-main-filename if intentional." >&2
        exit 1
    fi
fi

if [[ -z "${run_dir}" ]]; then
    run_dir="${runs_root}/main_rollup_cycle_$(date +%Y-%m-%d_%H%M%S)"
fi
run_dir="$(abs_path "${run_dir}")"
mkdir -p "${runs_root}"
mkdir -p "${run_dir}"

run_args=(
    --db "${db_path}"
    --run-dir "${run_dir}"
    --workspace "${workspace_key}"
    --project "${project_key}"
    --stale-days "${stale_days}"
    --min-active-nodes-before-rollup "${min_active_nodes_before_rollup}"
    --min-stale-candidates-before-rollup "${min_stale_candidates_before_rollup}"
    --rollup-chunk-max-items "${chunk_max_items}"
)
if [[ -n "${model_name}" ]]; then
    run_args+=( --model "${model_name}" )
fi
if [[ "${skip_reader_codex}" == "true" ]]; then
    run_args+=( --skip-reader-codex )
fi
if [[ "${skip_pruner_codex}" == "true" ]]; then
    run_args+=( --skip-pruner-codex )
fi

"${RUN_CODEX}" "${run_args[@]}"

plan_path="${run_dir}/pruner_plan.json"
if [[ ! -f "${plan_path}" ]]; then
    echo "expected plan missing: ${plan_path}" >&2
    exit 1
fi

eligible_count="$(jq -r '.proposals.rollup.eligible_count // 0' "${plan_path}")"
rollup_recommended="$(jq -r '.proposals.rollup.recommended // false' "${plan_path}")"
active_gate_met="$(jq -r '.policy.rollup_gate.gate_active_nodes_met // false' "${plan_path}")"
stale_gate_met="$(jq -r '.policy.rollup_gate.gate_stale_count_met // false' "${plan_path}")"
if [[ "${eligible_count}" -le 0 || "${rollup_recommended}" != "true" || "${active_gate_met}" != "true" || "${stale_gate_met}" != "true" ]]; then
    echo "rollup policy gate not satisfied; refusing main apply." >&2
    echo "eligible_count=${eligible_count} recommended=${rollup_recommended} active_gate_met=${active_gate_met} stale_gate_met=${stale_gate_met}" >&2
    echo "run_dir=${run_dir}" >&2
    exit 2
fi

planned_rollups=1
if [[ "${chunk_max_items}" -gt 0 ]]; then
    planned_rollups=$(( (eligible_count + chunk_max_items - 1) / chunk_max_items ))
fi
if [[ "${planned_rollups}" -le 0 ]]; then
    planned_rollups=1
fi

jq \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --argjson chunk_max_items "${chunk_max_items}" \
    --argjson planned_rollups "${planned_rollups}" \
    '
    .mode = "manual_main_rollup_apply" |
    .budgets.max_rollups = $planned_rollups |
    .budgets.max_nodes_affected = (.proposals.rollup.eligible_count // 0) |
    .operations.rollup.enabled = ((.proposals.rollup.recommended // false) and ((.proposals.rollup.eligible_count // 0) > 0)) |
    .operations.rollup.before_ns = (.proposals.rollup.before_ns // .operations.rollup.before_ns) |
    .operations.rollup.chunk_max_items = $chunk_max_items |
    .operations.rollup.scope = { workspace: $workspace, project: $project, kind: "" } |
    .operations.connection_pass = {
        enabled: ((.proposals.rollup.recommended // false) and ((.proposals.rollup.eligible_count // 0) > 0)),
        link_kind: "related",
        anchor_kind: "summarizes",
        anchor_item_id: 0,
        ensure_min_degree: true,
        propagate_neighbor_links: true,
        max_neighbor_links_per_rollup: (.operations.connection_pass.max_neighbor_links_per_rollup // 6),
        neighbor_scan_max_edges: (.operations.connection_pass.neighbor_scan_max_edges // 48),
        neighbor_scan_max_nodes: (.operations.connection_pass.neighbor_scan_max_nodes // 24)
    } |
    .budgets.max_links_added = (
        ($planned_rollups * ((.operations.connection_pass.max_neighbor_links_per_rollup // 6) + 1)) + 2
    ) |
    .operations.link_additions = { enabled: false, items: [] } |
    .operations.link_updates = { enabled: false, items: [] } |
    .notes = ((.notes // []) + ["Automated main-cycle approval: rollup enabled on main DB with scoped chunking and post-rollup connection pass."])
    ' "${plan_path}" > "${run_dir}/pruner_plan.approved.json"
mv "${run_dir}/pruner_plan.approved.json" "${plan_path}"

apply_args=(
    --db "${db_path}"
    --run-dir "${run_dir}"
    --workspace "${workspace_key}"
    --project "${project_key}"
    --apply
    --locked-apply
)
if [[ "${skip_reader_codex}" == "true" ]]; then
    apply_args+=( --skip-reader-codex )
fi
if [[ "${skip_pruner_codex}" == "true" ]]; then
    apply_args+=( --skip-pruner-codex )
fi

"${RUN_CODEX}" "${apply_args[@]}"

echo "main rollup cycle complete:"
echo "  run_dir=${run_dir}"
jq '{
    status,
    rolled_up_item_ids: [ .operation_results[]? | select(.op_type=="rollup" and .status=="ok" and (.rollup_item_id != null)) | .rollup_item_id ],
    planned_operations,
    applied_operations
}' "${run_dir}/pruner_apply_report.json"
jq '.status, .mode, .phases' "${run_dir}/run_report.json"
