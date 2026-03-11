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
  --suggestion-project <key>   Default: memory_console_codex_suggestions
  --suggestion-kind <value>    Default: decision
  --max-suggestion-links <n>   Default: 3
  --always-write-suggestion-md Always write suggestion markdown even for small suggestions
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
suggestion_project="memory_console_codex_suggestions"
suggestion_kind="decision"
max_suggestion_links=3
always_write_suggestion_md=false
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
        --suggestion-project)
            suggestion_project="${2:-}"
            shift 2
            ;;
        --suggestion-kind)
            suggestion_kind="${2:-}"
            shift 2
            ;;
        --max-suggestion-links)
            max_suggestion_links="${2:-}"
            shift 2
            ;;
        --always-write-suggestion-md)
            always_write_suggestion_md=true
            shift
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

jq_read_default() {
    local file_path="$1"
    local filter="$2"
    local fallback="$3"
    local out
    set +e
    out="$(jq -r "${filter}" "${file_path}" 2>/dev/null)"
    local status=$?
    set -e
    if (( status != 0 )) || [[ -z "${out}" || "${out}" == "null" ]]; then
        echo "${fallback}"
        return 0
    fi
    echo "${out}"
}

first_nonempty_line() {
    local file_path="$1"
    if [[ ! -f "${file_path}" ]]; then
        return 0
    fi
    awk 'NF { print; exit }' "${file_path}"
}

if [[ -z "${db_path}" ]]; then
    usage >&2
    exit 1
fi
if ! [[ "${max_suggestion_links}" =~ ^[0-9]+$ ]]; then
    echo "invalid --max-suggestion-links value: ${max_suggestion_links}" >&2
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

plan_path="${run_dir}/pruner_plan.json"
pruner_report_path="${run_dir}/pruner_apply_report.json"
reader_last_message_path="${run_dir}/codex_reader_last_message.md"
pruner_last_message_path="${run_dir}/codex_pruner_last_message.md"
suggestion_md_path="${run_dir}/codex_rollup_improvement.md"
suggestion_md_written=false
suggestion_priority="small"
suggestion_item_id=""
suggestion_status=0
suggestion_error=""
suggestion_links_attempted=0
suggestion_links_added=0
linked_previous_ids_file="$(mktemp)"
touch "${linked_previous_ids_file}"

rollup_recommended="$(jq_read_default "${plan_path}" '.proposals.rollup.recommended // false' 'false')"
rollup_reason="$(jq_read_default "${plan_path}" '.proposals.rollup.reason // ""' '')"
eligible_count="$(jq_read_default "${plan_path}" '.proposals.rollup.eligible_count // 0' '0')"
gate_active_nodes_met="$(jq_read_default "${plan_path}" '.policy.rollup_gate.gate_active_nodes_met // false' 'false')"
gate_stale_count_met="$(jq_read_default "${plan_path}" '.policy.rollup_gate.gate_stale_count_met // false' 'false')"
applied_rollups="$(jq_read_default "${pruner_report_path}" '.applied_operations.rollups // 0' '0')"
applied_connection_links="$(jq_read_default "${pruner_report_path}" '.applied_operations.connection_links // 0' '0')"
planned_rollups="$(jq_read_default "${pruner_report_path}" '.planned_operations.rollup_planned_runs // 0' '0')"

first_error_output="$(jq_read_default "${pruner_report_path}" 'first(.operation_results[]? | select(.status=="error") | .output) // ""' '')"
reader_hint="$(first_nonempty_line "${reader_last_message_path}")"
pruner_hint="$(first_nonempty_line "${pruner_last_message_path}")"

suggestion_idea="Track nightly rollup runs and continue tuning policy gates for stable compression behavior."
if (( reader_status != 0 )); then
    suggestion_priority="major"
    suggestion_idea="Reader phase failed. Improve reader fallback/retry behavior and validate required snapshot artifact completeness before Codex refinement."
elif (( pruner_status != 0 )); then
    suggestion_priority="major"
    suggestion_idea="Pruner phase failed. Strengthen pre-apply validation so plan counts and executable operation scope cannot diverge."
elif (( health_status != 0 )); then
    suggestion_priority="major"
    suggestion_idea="Post-run health check failed. Add health retry/diagnostic capture and block apply completion when integrity checks fail."
elif [[ -n "${first_error_output}" ]]; then
    suggestion_priority="major"
    suggestion_idea="Operation errors occurred during apply. Add targeted remediation for failing operation class and emit bounded diagnostics in run report."
elif [[ "${rollup_recommended}" == "true" && "${apply_mode}" == "true" && "${applied_rollups}" -eq 0 ]]; then
    suggestion_priority="major"
    suggestion_idea="Rollup was recommended but no rollups applied. Tighten approval guards and budget sizing to avoid false-positive apply paths."
elif [[ "${rollup_recommended}" == "false" && "${gate_active_nodes_met}" == "false" ]]; then
    suggestion_idea="Rollup deferred due active-node pressure gate. Keep current memory density and periodically re-evaluate gate threshold by project growth."
elif [[ "${rollup_recommended}" == "false" && "${gate_stale_count_met}" == "false" ]]; then
    suggestion_idea="Rollup deferred due stale-candidate gate. Keep data uncompressed until stale signal strengthens."
elif [[ "${apply_mode}" == "true" && "${applied_connection_links}" -lt "${applied_rollups}" ]]; then
    suggestion_idea="Increase connection-pass resilience so each new rollup block guarantees at least one stable graph attachment."
else
    suggestion_idea="Evaluate whether token usage can be reduced by lowering stale scan bounds when gate signals are consistently below threshold."
fi

run_stamp="$(basename "${run_dir}")"
suggestion_title="Codex Rollup Suggestion (${run_stamp})"
suggestion_body="$(cat <<EOF
Codex nightly suggestion

run_dir: ${run_dir}
generated_at_utc: ${generated_at_utc}
mode: apply=${apply_mode}, locked_apply=${locked_apply}
overall_status: $(jq_read_default "${run_report_path}" '.status' 'unknown')
priority: ${suggestion_priority}

signals:
- rollup_recommended: ${rollup_recommended}
- eligible_count: ${eligible_count}
- gate_active_nodes_met: ${gate_active_nodes_met}
- gate_stale_count_met: ${gate_stale_count_met}
- planned_rollups: ${planned_rollups}
- applied_rollups: ${applied_rollups}
- applied_connection_links: ${applied_connection_links}

suggestion:
${suggestion_idea}

rollup_reason:
${rollup_reason}

pruner_error_hint:
${first_error_output}

reader_hint:
${reader_hint}

pruner_hint:
${pruner_hint}
EOF
)"

prior_ids=()
if [[ "${max_suggestion_links}" -gt 0 ]]; then
    while IFS= read -r prior_id; do
        if [[ -n "${prior_id}" ]]; then
            prior_ids+=( "${prior_id}" )
        fi
    done < <("${MEM_CLI}" query --db "${db_path}" --workspace "${workspace_key}" --project "${suggestion_project}" --kind "${suggestion_kind}" --limit "$((max_suggestion_links + 6))" --format json 2>/dev/null | jq -r '.[].id')
fi

set +e
suggestion_add_output="$("${MEM_CLI}" add \
    --db "${db_path}" \
    --title "${suggestion_title}" \
    --body "${suggestion_body}" \
    --workspace "${workspace_key}" \
    --project "${suggestion_project}" \
    --kind "${suggestion_kind}" \
    --session-id "${session_id}" 2>&1)"
suggestion_add_status=$?
set -e

if (( suggestion_add_status != 0 )); then
    suggestion_status=1
    suggestion_error="${suggestion_add_output}"
else
    if [[ "${suggestion_add_output}" =~ id=([0-9]+) ]]; then
        suggestion_item_id="${BASH_REMATCH[1]}"
    else
        suggestion_status=1
        suggestion_error="suggestion add succeeded but item id could not be parsed"
    fi
fi

if (( suggestion_status == 0 )) && [[ -n "${suggestion_item_id}" && "${max_suggestion_links}" -gt 0 ]]; then
    for prior_id in "${prior_ids[@]}"; do
        if (( suggestion_links_attempted >= max_suggestion_links )); then
            break
        fi
        if [[ "${prior_id}" == "${suggestion_item_id}" ]]; then
            continue
        fi
        suggestion_links_attempted=$(( suggestion_links_attempted + 1 ))
        set +e
        link_output="$("${MEM_CLI}" link-add \
            --db "${db_path}" \
            --from "${suggestion_item_id}" \
            --to "${prior_id}" \
            --kind related \
            --note "nightly codex suggestion chain" \
            --session-id "${session_id}" 2>&1)"
        link_status=$?
        set -e
        if (( link_status == 0 )); then
            suggestion_links_added=$(( suggestion_links_added + 1 ))
            printf '%s\n' "${prior_id}" >> "${linked_previous_ids_file}"
        elif [[ "${link_output}" == *"UNIQUE constraint failed"* || "${link_output}" == *"duplicate"* ]]; then
            printf '%s\n' "${prior_id}" >> "${linked_previous_ids_file}"
        fi
    done
fi

if [[ "${always_write_suggestion_md}" == "true" || "${suggestion_priority}" == "major" ]]; then
    {
        echo "# Codex Rollup Improvement Note"
        echo
        echo "- generated_at_utc: ${generated_at_utc}"
        echo "- run_dir: ${run_dir}"
        echo "- priority: ${suggestion_priority}"
        echo "- status: $(jq_read_default "${run_report_path}" '.status' 'unknown')"
        echo "- suggestion_item_id: ${suggestion_item_id}"
        echo
        echo "## Key Signals"
        echo "- rollup_recommended: ${rollup_recommended}"
        echo "- eligible_count: ${eligible_count}"
        echo "- gate_active_nodes_met: ${gate_active_nodes_met}"
        echo "- gate_stale_count_met: ${gate_stale_count_met}"
        echo "- planned_rollups: ${planned_rollups}"
        echo "- applied_rollups: ${applied_rollups}"
        echo "- applied_connection_links: ${applied_connection_links}"
        echo
        echo "## Suggested Improvement"
        echo "${suggestion_idea}"
        if [[ -n "${rollup_reason}" ]]; then
            echo
            echo "## Rollup Reason"
            echo "${rollup_reason}"
        fi
        if [[ -n "${first_error_output}" ]]; then
            echo
            echo "## First Error"
            echo "${first_error_output}"
        fi
    } > "${suggestion_md_path}"
    suggestion_md_written=true
fi

linked_previous_ids_json="$(jq -Rsc 'split("\n") | map(select(length>0) | tonumber)' "${linked_previous_ids_file}")"
rm -f "${linked_previous_ids_file}"

tmp_run_report="$(mktemp)"
jq \
    --arg suggestion_project "${suggestion_project}" \
    --arg suggestion_kind "${suggestion_kind}" \
    --arg suggestion_priority "${suggestion_priority}" \
    --arg suggestion_idea "${suggestion_idea}" \
    --arg suggestion_item_id "${suggestion_item_id}" \
    --arg suggestion_error "${suggestion_error}" \
    --arg suggestion_md_path "${suggestion_md_path}" \
    --argjson suggestion_status "${suggestion_status}" \
    --argjson suggestion_md_written "${suggestion_md_written}" \
    --argjson suggestion_links_attempted "${suggestion_links_attempted}" \
    --argjson suggestion_links_added "${suggestion_links_added}" \
    --argjson linked_previous_ids "${linked_previous_ids_json}" \
    '
    .artifacts.codex_suggestion_memory_item_id = $suggestion_item_id
    | .artifacts.codex_rollup_improvement_md = (if $suggestion_md_written then $suggestion_md_path else "" end)
    | .suggestion = {
        status: (if $suggestion_status == 0 then "ok" else "failed" end),
        project: $suggestion_project,
        kind: $suggestion_kind,
        priority: $suggestion_priority,
        idea: $suggestion_idea,
        item_id: $suggestion_item_id,
        links_attempted: $suggestion_links_attempted,
        links_added: $suggestion_links_added,
        linked_previous_ids: $linked_previous_ids,
        md_written: $suggestion_md_written,
        md_path: (if $suggestion_md_written then $suggestion_md_path else "" end),
        error: $suggestion_error
    }
    | .status = (
        if .status == "ok" and $suggestion_status != 0 then
            "suggestion_failed"
        else
            .status
        end
    )
    ' "${run_report_path}" > "${tmp_run_report}"
mv "${tmp_run_report}" "${run_report_path}"

echo "nightly codex run complete:"
echo "  run_dir=${run_dir}"
echo "  plan=${run_dir}/pruner_plan.json"
echo "  report=${run_dir}/pruner_apply_report.json"
echo "  health_after=${health_after_path}"
echo "  run_report=${run_report_path}"
echo "  suggestion_project=${suggestion_project}"
echo "  suggestion_item_id=${suggestion_item_id}"
if [[ "${suggestion_md_written}" == "true" ]]; then
    echo "  suggestion_md=${suggestion_md_path}"
fi
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
if (( suggestion_status != 0 )); then
    echo "suggestion logging failed: ${suggestion_error}" >&2
    exit 1
fi
if (( reader_status != 0 )); then
    exit "${reader_status}"
fi
if (( pruner_status != 0 )); then
    exit "${pruner_status}"
fi
