#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
READER="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_reader.sh"
PRUNER="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_pruner.sh"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'EOF'
usage: mem_nightly_run.sh --db <path> [options]

required:
  --db <path>                  SQLite DB path

options:
  --workspace <key>            Default: codework
  --project <key>              Default: memory_console
  --runs-root <dir>            Default: docs/private_program_docs/memory_console/nightly_runs
  --run-date <YYYY-MM-DD>      Default: local current date
  --stale-days <n>             Default: 30
  --min-active-nodes-before-rollup <n>
                               Default: 40
  --min-stale-candidates-before-rollup <n>
                               Default: 4
  --scan-limit <n>             Default: 800
  --page-size <n>              Default: 200
  --events-limit <n>           Default: 200
  --audits-limit <n>           Default: 200
  --candidate-limit <n>        Default: 80
  --rollup-chunk-max-items <n> Default: 12
  --rollup-max-groups <n>      Default: 8
  --session-id <id>            Default: mem-nightly-<yyyymmdd>
  --apply                      Execute pruner writes (default: dry-run)
  --allow-empty-apply          Allow apply mode when zero operations are approved
  -h, --help                   Show this help
EOF
}

db_path=""
workspace_key="codework"
project_key="memory_console"
runs_root="${ROOT_DIR}/docs/private_program_docs/memory_console/nightly_runs"
run_date="$(date +"%Y-%m-%d")"
stale_days=30
min_active_nodes_before_rollup=40
min_stale_candidates_before_rollup=4
scan_limit=800
page_size=200
events_limit=200
audits_limit=200
candidate_limit=80
rollup_chunk_max_items=12
rollup_max_groups=8
session_id=""
apply_mode=false
allow_empty_apply=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --db)
            db_path="${2:-}"
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
        --runs-root)
            runs_root="${2:-}"
            shift 2
            ;;
        --run-date)
            run_date="${2:-}"
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
        --scan-limit)
            scan_limit="${2:-}"
            shift 2
            ;;
        --page-size)
            page_size="${2:-}"
            shift 2
            ;;
        --events-limit)
            events_limit="${2:-}"
            shift 2
            ;;
        --audits-limit)
            audits_limit="${2:-}"
            shift 2
            ;;
        --candidate-limit)
            candidate_limit="${2:-}"
            shift 2
            ;;
        --rollup-chunk-max-items)
            rollup_chunk_max_items="${2:-}"
            shift 2
            ;;
        --rollup-max-groups)
            rollup_max_groups="${2:-}"
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

if [[ -z "${db_path}" ]]; then
    usage >&2
    exit 1
fi

if [[ ! -x "${MEM_CLI}" || ! -x "${READER}" || ! -x "${PRUNER}" ]]; then
    echo "missing executable tool(s). expected:" >&2
    echo "  ${MEM_CLI}" >&2
    echo "  ${READER}" >&2
    echo "  ${PRUNER}" >&2
    echo "run chmod +x on nightly scripts and build mem_cli if needed." >&2
    exit 1
fi

mkdir -p "${runs_root}"
run_stamp="${run_date}_$(date +"%H%M%S")"
run_dir="${runs_root}/${run_stamp}"
mkdir -p "${run_dir}"

if [[ -z "${session_id}" ]]; then
    session_id="mem-nightly-${run_date//-/}"
fi

generated_at_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
health_after_path="${run_dir}/health_after.json"
run_report_path="${run_dir}/run_report.json"

"${READER}" \
    --db "${db_path}" \
    --run-dir "${run_dir}" \
    --workspace "${workspace_key}" \
    --project "${project_key}" \
    --stale-days "${stale_days}" \
    --min-active-nodes-before-rollup "${min_active_nodes_before_rollup}" \
    --min-stale-candidates-before-rollup "${min_stale_candidates_before_rollup}" \
    --scan-limit "${scan_limit}" \
    --page-size "${page_size}" \
    --candidate-limit "${candidate_limit}" \
    --rollup-chunk-max-items "${rollup_chunk_max_items}" \
    --rollup-max-groups "${rollup_max_groups}" \
    --events-limit "${events_limit}" \
    --audits-limit "${audits_limit}"

pruner_status=0
plan_path="${run_dir}/pruner_plan.json"
if [[ "${apply_mode}" == "true" ]]; then
    planned_ops_count="$(jq '
        def lane_count($lane):
            if ($lane | type) == "array" then
                ($lane | length)
            elif ($lane | type) == "object" then
                if ($lane.enabled // true) then
                    (($lane.items // $lane.entries // []) | length)
                else
                    0
                end
            else
                0
            end;
        def rollup_runs:
            if (.operations.rollup.enabled // false) then
                ((.proposals.rollup.eligible_count // 0) as $eligible |
                 (.operations.rollup.chunk_max_items // 0) as $chunk |
                 if $eligible <= 0 then 0
                 elif $chunk > 0 then (($eligible + $chunk - 1) / $chunk | floor)
                 else 1
                 end)
            else
                0
            end;
        lane_count(.operations.link_additions)
        + lane_count(.operations.link_updates)
        + rollup_runs
    ' "${plan_path}")"
    if (( planned_ops_count == 0 )); then
        if [[ "${allow_empty_apply}" == "true" ]]; then
            echo "warning: apply mode requested but plan has zero enabled operations; continuing because --allow-empty-apply was set." >&2
        else
            echo "apply mode requested but plan has zero enabled operations; refusing apply. pass --allow-empty-apply to permit intentional no-op apply." >&2
            exit 2
        fi
    fi
fi
set +e
if [[ "${apply_mode}" == "true" ]]; then
    pruner_apply_args=( --db "${db_path}" --run-dir "${run_dir}" --plan "${plan_path}" --session-id "${session_id}" --apply )
    if [[ "${allow_empty_apply}" == "true" ]]; then
        pruner_apply_args+=( --allow-empty-apply )
    fi
    "${PRUNER}" "${pruner_apply_args[@]}"
else
    "${PRUNER}" --db "${db_path}" --run-dir "${run_dir}" --plan "${plan_path}" --session-id "${session_id}"
fi
pruner_status=$?
set -e

"${MEM_CLI}" health --db "${db_path}" --format json > "${health_after_path}"

jq -n \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --arg run_dir "${run_dir}" \
    --arg session_id "${session_id}" \
    --argjson apply "${apply_mode}" \
    --argjson pruner_status "${pruner_status}" \
    --slurpfile health_after "${health_after_path}" \
    '{
        generated_at_utc: $generated_at_utc,
        db_path: $db_path,
        workspace: $workspace,
        project: $project,
        run_dir: $run_dir,
        session_id: $session_id,
        apply: $apply,
        status: (if $pruner_status == 0 then "ok" else "failed" end),
        artifacts: {
            health_before: ($run_dir + "/health_before.json"),
            input_snapshot: ($run_dir + "/input_snapshot.json"),
            reader_summary: ($run_dir + "/reader_daily_summary.md"),
            pruner_plan: ($run_dir + "/pruner_plan.json"),
            pruner_report: ($run_dir + "/pruner_apply_report.json"),
            health_after: ($run_dir + "/health_after.json")
        },
        health_after: $health_after[0]
    }' > "${run_report_path}"

echo "nightly run complete:"
echo "  run_dir: ${run_dir}"
echo "  report:  ${run_report_path}"
if (( pruner_status != 0 )); then
    exit "${pruner_status}"
fi
