#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
READER_CODEX="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_reader_codex.sh"
PRUNER_CODEX="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_pruner_codex.sh"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'EOF'
usage: mem_nightly_run_codex.sh --db <path> [options]

required:
  --db <path>                  SQLite DB path

options:
  --run-dir <dir>              Existing run dir. If unset, auto-create under nightly_runs.
  --runs-root <dir>            Default: docs/private_program_docs/memory_console/nightly_runs
  --workspace <key>            Default: codework
  --project <key>              Default: memory_console
  --stale-days <n>             Default: 30
  --min-active-nodes-before-rollup <n>
                               Default: 40
  --min-stale-candidates-before-rollup <n>
                               Default: 4
  --scan-limit <n>             Default: 800
  --page-size <n>              Default: 200
  --candidate-limit <n>        Default: 80
  --rollup-chunk-max-items <n> Default: 12
  --rollup-max-groups <n>      Default: 8
  --events-limit <n>           Default: 200
  --audits-limit <n>           Default: 200
  --session-id <id>            Default: mem-nightly-<yyyymmdd>
  --model <name>               Optional codex model override
  --apply                      Apply writes in pruner phase (default: dry-run)
  --allow-empty-apply          Allow apply mode when zero operations are approved
  --locked-apply               Apply existing reviewed run-dir without running Reader/Codex review
  --skip-reader-codex          Skip codex intelligence in reader phase
  --skip-pruner-codex          Skip codex intelligence in pruner phase
  -h, --help                   Show this help
EOF
}

db_path=""
run_dir=""
runs_root="${ROOT_DIR}/docs/private_program_docs/memory_console/nightly_runs"
workspace_key="codework"
project_key="memory_console"
stale_days=30
min_active_nodes_before_rollup=40
min_stale_candidates_before_rollup=4
scan_limit=800
page_size=200
candidate_limit=80
rollup_chunk_max_items=12
rollup_max_groups=8
events_limit=200
audits_limit=200
session_id=""
model_name=""
apply_mode=false
allow_empty_apply=false
locked_apply=false
skip_reader_codex=false
skip_pruner_codex=false

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
        --runs-root)
            runs_root="${2:-}"
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
        --scan-limit)
            scan_limit="${2:-}"
            shift 2
            ;;
        --page-size)
            page_size="${2:-}"
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
        --events-limit)
            events_limit="${2:-}"
            shift 2
            ;;
        --audits-limit)
            audits_limit="${2:-}"
            shift 2
            ;;
        --session-id)
            session_id="${2:-}"
            shift 2
            ;;
        --model)
            model_name="${2:-}"
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
        --locked-apply)
            locked_apply=true
            shift
            ;;
        --skip-reader-codex)
            skip_reader_codex=true
            shift
            ;;
        --skip-pruner-codex)
            skip_pruner_codex=true
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
if [[ ! -x "${READER_CODEX}" || ! -x "${PRUNER_CODEX}" || ! -x "${MEM_CLI}" ]]; then
    echo "missing executable codex wrappers." >&2
    echo "expected:" >&2
    echo "  ${READER_CODEX}" >&2
    echo "  ${PRUNER_CODEX}" >&2
    echo "  ${MEM_CLI}" >&2
    exit 1
fi

if [[ -z "${session_id}" ]]; then
    session_id="mem-nightly-$(date +%Y%m%d)"
fi

if [[ "${locked_apply}" == "true" ]]; then
    if [[ "${apply_mode}" != "true" ]]; then
        echo "--locked-apply requires --apply." >&2
        exit 1
    fi
    if [[ -z "${run_dir}" ]]; then
        echo "--locked-apply requires --run-dir pointing to an existing reviewed run directory." >&2
        exit 1
    fi
    if [[ ! -d "${run_dir}" ]]; then
        echo "run dir not found for locked apply: ${run_dir}" >&2
        exit 1
    fi
    if [[ ! -f "${run_dir}/pruner_plan.json" ]]; then
        echo "locked apply requires existing plan: ${run_dir}/pruner_plan.json" >&2
        exit 1
    fi
else
    if [[ -z "${run_dir}" ]]; then
        mkdir -p "${runs_root}"
        run_dir="${runs_root}/$(date +%Y-%m-%d_%H%M%S)"
    fi
    mkdir -p "${run_dir}"
fi

generated_at_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
health_after_path="${run_dir}/health_after.json"
run_report_path="${run_dir}/run_report.json"
reader_status=0
pruner_status=0
health_status=0
reader_phase="executed"

if [[ "${locked_apply}" != "true" ]]; then
    reader_args=(
        --db "${db_path}"
        --run-dir "${run_dir}"
        --workspace "${workspace_key}"
        --project "${project_key}"
        --stale-days "${stale_days}"
        --min-active-nodes-before-rollup "${min_active_nodes_before_rollup}"
        --min-stale-candidates-before-rollup "${min_stale_candidates_before_rollup}"
        --scan-limit "${scan_limit}"
        --page-size "${page_size}"
        --candidate-limit "${candidate_limit}"
        --rollup-chunk-max-items "${rollup_chunk_max_items}"
        --rollup-max-groups "${rollup_max_groups}"
        --events-limit "${events_limit}"
        --audits-limit "${audits_limit}"
    )
    if [[ -n "${model_name}" ]]; then
        reader_args+=( --model "${model_name}" )
    fi
    if [[ "${skip_reader_codex}" == "true" ]]; then
        reader_args+=( --skip-codex )
    fi

    set +e
    "${READER_CODEX}" "${reader_args[@]}"
    reader_status=$?
    set -e
else
    reader_phase="skipped_locked_apply"
fi

if (( reader_status == 0 )); then
    pruner_args=(
        --db "${db_path}"
        --run-dir "${run_dir}"
        --plan "${run_dir}/pruner_plan.json"
        --session-id "${session_id}"
    )
    if [[ -n "${model_name}" ]]; then
        pruner_args+=( --model "${model_name}" )
    fi
    if [[ "${skip_pruner_codex}" == "true" ]]; then
        pruner_args+=( --skip-codex )
    fi
    if [[ "${apply_mode}" == "true" ]]; then
        pruner_args+=( --apply )
    fi
    if [[ "${allow_empty_apply}" == "true" ]]; then
        pruner_args+=( --allow-empty-apply )
    fi
    if [[ "${locked_apply}" == "true" ]]; then
        pruner_args+=( --locked-plan )
    fi

    set +e
    "${PRUNER_CODEX}" "${pruner_args[@]}"
    pruner_status=$?
    set -e
else
    pruner_status="${reader_status}"
fi

set +e
"${MEM_CLI}" health --db "${db_path}" --format json > "${health_after_path}"
health_status=$?
set -e
if (( health_status != 0 )); then
    echo "warning: failed to collect health_after using mem_cli." >&2
    printf '{"ok":0,"error":"health command failed after codex nightly run"}\n' > "${health_after_path}"
fi

jq -n \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --arg run_dir "${run_dir}" \
    --arg session_id "${session_id}" \
    --arg reader_phase "${reader_phase}" \
    --argjson apply "${apply_mode}" \
    --argjson locked_apply "${locked_apply}" \
    --argjson allow_empty_apply "${allow_empty_apply}" \
    --argjson reader_status "${reader_status}" \
    --argjson pruner_status "${pruner_status}" \
    --argjson health_status "${health_status}" \
    --slurpfile health_after "${health_after_path}" \
    '{
        generated_at_utc: $generated_at_utc,
        db_path: $db_path,
        workspace: $workspace,
        project: $project,
        run_dir: $run_dir,
        session_id: $session_id,
        mode: {
            apply: $apply,
            locked_apply: $locked_apply,
            allow_empty_apply: $allow_empty_apply
        },
        phases: {
            reader: {
                phase: $reader_phase,
                status_code: $reader_status
            },
            pruner: {
                status_code: $pruner_status
            }
        },
        status: (
            if $reader_status != 0 then "reader_failed"
            elif $pruner_status != 0 then "pruner_failed"
            elif $health_status != 0 then "health_failed"
            else "ok"
            end
        ),
        artifacts: {
            health_before: ($run_dir + "/health_before.json"),
            input_snapshot: ($run_dir + "/input_snapshot.json"),
            reader_summary: ($run_dir + "/reader_daily_summary.md"),
            pruner_plan: ($run_dir + "/pruner_plan.json"),
            pruner_report: ($run_dir + "/pruner_apply_report.json"),
            pruner_health_before: ($run_dir + "/pruner_health_before.json"),
            pruner_health_after: ($run_dir + "/pruner_health_after.json"),
            health_after: ($run_dir + "/health_after.json"),
            run_report: ($run_dir + "/run_report.json"),
            codex_reader_last_message: ($run_dir + "/codex_reader_last_message.md"),
            codex_pruner_last_message: ($run_dir + "/codex_pruner_last_message.md")
        },
        health_after: $health_after[0]
    }' > "${run_report_path}"

echo "nightly codex run complete:"
echo "  run_dir=${run_dir}"
echo "  plan=${run_dir}/pruner_plan.json"
echo "  report=${run_dir}/pruner_apply_report.json"
echo "  health_after=${health_after_path}"
echo "  run_report=${run_report_path}"
if [[ -f "${run_dir}/pruner_apply_report.json" ]]; then
    echo "  apply_summary:"
    jq '{
        rollup_item_ids: [ .operation_results[]? | select(.op_type=="rollup" and .status=="ok" and (.rollup_item_id != null)) | .rollup_item_id ],
        rollup_runs: (.applied_operations.rollups // 0),
        connection_links: (.applied_operations.connection_links // 0),
        link_additions: (.applied_operations.link_additions // 0),
        link_updates: (.applied_operations.link_updates // 0)
    }' "${run_dir}/pruner_apply_report.json"
fi
if (( reader_status != 0 )); then
    exit "${reader_status}"
fi
if (( pruner_status != 0 )); then
    exit "${pruner_status}"
fi
