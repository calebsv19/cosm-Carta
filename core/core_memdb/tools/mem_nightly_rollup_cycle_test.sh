#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
RUN_CODEX="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_run_codex.sh"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'EOF'
usage: mem_nightly_rollup_cycle_test.sh [options]

Purpose:
  Run a full nightly Codex test cycle on an isolated TEST DB only:
  1) copy MAIN DB -> TEST DB
  2) run full Codex dry pass with stale-days override
  3) auto-approve rollup in generated plan (test-only)
  4) locked apply on same run dir
  5) verify reports and main-db checksum unchanged

options:
  --main-db <path>             Default: /Users/calebsv/Desktop/CodeWork/data/codework_mem_console.sqlite
  --test-db <path>             Default: /Users/calebsv/Desktop/CodeWork/data/codework_mem_console_rollup_cycle_test.sqlite
  --runs-root <dir>            Default: docs/private_program_docs/memory_console/nightly_runs
  --run-dir <dir>              Optional explicit run dir
  --workspace <key>            Default: codework
  --project <key>              Default: memory_console
  --stale-days <n>             Default: 0 (forces stale candidates for test)
  --min-active-nodes-before-rollup <n>
                               Default: 40
  --min-stale-candidates-before-rollup <n>
                               Default: 4
  --chunk-max-items <n>        Default: 12 (smaller rollup blocks per apply cycle)
  --model <name>               Optional codex model override
  --skip-reader-codex          Pass-through to codex run wrapper
  --skip-pruner-codex          Pass-through to codex run wrapper
  --allow-non-test-filename    Allow test DB basename without "test" marker
  -h, --help                   Show this help
EOF
}

main_db="/Users/calebsv/Desktop/CodeWork/data/codework_mem_console.sqlite"
test_db="/Users/calebsv/Desktop/CodeWork/data/codework_mem_console_rollup_cycle_test.sqlite"
runs_root="${ROOT_DIR}/docs/private_program_docs/memory_console/nightly_runs"
run_dir=""
workspace_key="codework"
project_key="memory_console"
stale_days=0
min_active_nodes_before_rollup=40
min_stale_candidates_before_rollup=4
chunk_max_items=12
model_name=""
skip_reader_codex=false
skip_pruner_codex=false
allow_non_test_filename=false

abs_path() {
    local p="$1"
    local d
    d="$(dirname "$p")"
    mkdir -p "$d"
    echo "$(cd "$d" && pwd -P)/$(basename "$p")"
}

sha256_file() {
    local p="$1"
    shasum -a 256 "$p" | awk '{print $1}'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --main-db)
            main_db="${2:-}"
            shift 2
            ;;
        --test-db)
            test_db="${2:-}"
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
        --allow-non-test-filename)
            allow_non_test_filename=true
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

if [[ ! -x "${RUN_CODEX}" || ! -x "${MEM_CLI}" ]]; then
    echo "missing required executables:" >&2
    echo "  ${RUN_CODEX}" >&2
    echo "  ${MEM_CLI}" >&2
    exit 1
fi
if [[ ! -f "${main_db}" ]]; then
    echo "main db not found: ${main_db}" >&2
    exit 1
fi
if [[ "${chunk_max_items}" -le 0 ]]; then
    echo "chunk-max-items must be > 0, got: ${chunk_max_items}" >&2
    exit 1
fi

main_db="$(abs_path "${main_db}")"
test_db="$(abs_path "${test_db}")"
runs_root="$(abs_path "${runs_root}")"

if [[ "${main_db}" == "${test_db}" ]]; then
    echo "refusing to run: test db path resolves to main db path." >&2
    exit 1
fi

lower_test_name="$(basename "${test_db}" | tr '[:upper:]' '[:lower:]')"
if [[ "${allow_non_test_filename}" != "true" && "${lower_test_name}" != *test* ]]; then
    echo "refusing to run: test db filename must include 'test' marker." >&2
    echo "set --allow-non-test-filename if this is intentional." >&2
    exit 1
fi

if [[ -z "${run_dir}" ]]; then
    run_dir="${runs_root}/test_rollup_cycle_$(date +%Y-%m-%d_%H%M%S)"
fi
run_dir="$(abs_path "${run_dir}")"
mkdir -p "$(dirname "${test_db}")"
mkdir -p "${runs_root}"
mkdir -p "${run_dir}"

main_hash_before="$(sha256_file "${main_db}")"

cp "${main_db}" "${test_db}"
"${MEM_CLI}" health --db "${test_db}" --format json > "${run_dir}/test_db_health_before.json"

run_args=(
    --db "${test_db}"
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
    echo "rollup policy gate not satisfied; cannot test apply path." >&2
    echo "eligible_count=${eligible_count} recommended=${rollup_recommended} active_gate_met=${active_gate_met} stale_gate_met=${stale_gate_met}" >&2
    echo "rerun with broader stale criteria or inspect: ${plan_path}" >&2
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
    .mode = "manual_test_rollup_apply" |
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
    .notes = ((.notes // []) + ["Automated test-cycle approval: rollup enabled on isolated test DB with scoped chunking and post-rollup connection pass."])
    ' "${plan_path}" > "${run_dir}/pruner_plan.approved.json"
mv "${run_dir}/pruner_plan.approved.json" "${plan_path}"

apply_args=(
    --db "${test_db}"
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

main_hash_after="$(sha256_file "${main_db}")"
if [[ "${main_hash_before}" != "${main_hash_after}" ]]; then
    echo "safety check failed: main db checksum changed during test cycle." >&2
    echo "main_db=${main_db}" >&2
    exit 3
fi

echo "rollup cycle test complete:"
echo "  run_dir=${run_dir}"
echo "  main_db_sha256_unchanged=true"
jq '{
    status,
    rolled_up_item_ids: [ .operation_results[]? | select(.op_type=="rollup" and .status=="ok" and (.rollup_item_id != null)) | .rollup_item_id ],
    planned_operations,
    applied_operations
}' "${run_dir}/pruner_apply_report.json"
jq '.status, .mode, .phases' "${run_dir}/run_report.json"
