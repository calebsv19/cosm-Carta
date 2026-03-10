#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
PRUNER_BASE="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_pruner.sh"
CODEX_BIN="${CODEX_BIN:-codex}"
SKILL_FILE="${ROOT_DIR}/skills/memory-nightly-pruner/SKILL.md"

usage() {
    cat <<'EOF'
usage: mem_nightly_pruner_codex.sh --db <path> --run-dir <dir> [options]

required:
  --db <path>                  SQLite DB path
  --run-dir <dir>              Run directory containing pruner_plan.json

options:
  --plan <path>                Default: <run-dir>/pruner_plan.json
  --session-id <id>            Default: mem-nightly-<yyyymmdd>
  --model <name>               Optional codex model override
  --locked-plan                Apply existing reviewed plan without Codex review mutation
  --skip-codex                 Skip Codex review/refinement and run base pruner directly
  --apply                      Apply writes (default: dry-run)
  --allow-empty-apply          Allow apply mode when zero operations are approved
  -h, --help                   Show this help
EOF
}

db_path=""
run_dir=""
plan_path=""
session_id=""
model_name=""
locked_plan=false
skip_codex=false
apply_mode=false
allow_empty_apply=false

normalize_plan_basic_fields() {
    local plan_file="$1"
    local run_db_path="$2"
    local generated_at
    local tmp
    generated_at="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
    tmp="$(mktemp)"
    jq \
        --arg db_path "${run_db_path}" \
        --arg generated_at_utc "${generated_at}" \
        '
        .version = (if ((.version | type) == "number") then .version else 1 end) |
        .generated_at_utc = (
            if ((.generated_at_utc | type) == "string" and (.generated_at_utc | length) > 0) then
                .generated_at_utc
            else
                $generated_at_utc
            end
        ) |
        .db_path = (
            if ((.db_path | type) == "string" and (.db_path | length) > 0) then
                .db_path
            else
                $db_path
            end
        ) |
        .workspace = (
            if ((.workspace | type) == "string" and (.workspace | length) > 0) then
                .workspace
            elif ((.operations.rollup.scope.workspace | type) == "string" and (.operations.rollup.scope.workspace | length) > 0) then
                .operations.rollup.scope.workspace
            elif ((.proposals.rollup.scope.workspace | type) == "string" and (.proposals.rollup.scope.workspace | length) > 0) then
                .proposals.rollup.scope.workspace
            else
                "codework"
            end
        ) |
        .project = (
            if ((.project | type) == "string" and (.project | length) > 0) then
                .project
            elif ((.operations.rollup.scope.project | type) == "string" and (.operations.rollup.scope.project | length) > 0) then
                .operations.rollup.scope.project
            elif ((.proposals.rollup.scope.project | type) == "string" and (.proposals.rollup.scope.project | length) > 0) then
                .proposals.rollup.scope.project
            else
                "memory_console"
            end
        ) |
        .mode = (
            if ((.mode | type) == "string" and (.mode | length) > 0) then
                .mode
            else
                "reader_read_only"
            end
        ) |
        .notes = (if ((.notes | type) == "array") then .notes else [] end)
        ' "${plan_file}" > "${tmp}"
    mv "${tmp}" "${plan_file}"
}

validate_plan_basic() {
    local plan_file="$1"
    jq -e '
        ((.version | type) == "number" and .version >= 1 and .version <= 2)
        and ((.generated_at_utc | type) == "string")
        and ((.db_path | type) == "string" and (.db_path | length) > 0)
        and ((.workspace | type) == "string" and (.workspace | length) > 0)
        and ((.project | type) == "string" and (.project | length) > 0)
        and ((.budgets | type) == "object")
        and ((.operations | type) == "object")
        and ((.operations.rollup | type) == "object")
        and ((.operations.rollup.enabled | type) == "boolean")
    ' "${plan_file}" > /dev/null
}

validate_plan_apply_strict() {
    local plan_file="$1"
    jq -e '
        def rollup_runs:
            if (.operations.rollup.enabled | not) then 0
            else
                ((.proposals.rollup.eligible_count // 0) as $eligible |
                 (.operations.rollup.chunk_max_items // 0) as $chunk |
                 if $eligible <= 0 then 0
                 elif $chunk > 0 then (($eligible + $chunk - 1) / $chunk | floor)
                 else 1
                 end)
            end;
        def neighbor_links_per_rollup:
            (.operations.connection_pass.max_neighbor_links_per_rollup // 0);
        def connection_links:
            if ((.operations.connection_pass.enabled // false) and (.operations.rollup.enabled // false)) then
                (
                    rollup_runs
                    + (if (.operations.connection_pass.propagate_neighbor_links // false) then (rollup_runs * neighbor_links_per_rollup) else 0 end)
                )
            else
                0
            end;
        ((.budgets.max_links_added | type) == "number" and .budgets.max_links_added >= 0)
        and ((.budgets.max_links_updated | type) == "number" and .budgets.max_links_updated >= 0)
        and ((.budgets.max_rollups | type) == "number" and .budgets.max_rollups >= 0)
        and ((.budgets.max_nodes_affected | type) == "number" and .budgets.max_nodes_affected >= 0)
        and ((.operations.connection_pass // {} | type) == "object")
        and ((.operations.connection_pass.enabled // false | type) == "boolean")
        and ((.operations.connection_pass.link_kind // "related" | type) == "string")
        and ((.operations.connection_pass.anchor_kind // "summarizes" | type) == "string")
        and ((.operations.connection_pass.anchor_item_id // 0 | type) == "number")
        and ((.operations.connection_pass.ensure_min_degree // true | type) == "boolean")
        and ((.operations.connection_pass.propagate_neighbor_links // false | type) == "boolean")
        and ((.operations.connection_pass.max_neighbor_links_per_rollup // 0 | type) == "number")
        and ((.operations.connection_pass.max_neighbor_links_per_rollup // 0) >= 0)
        and ((.operations.connection_pass.neighbor_scan_max_edges // 24 | type) == "number")
        and ((.operations.connection_pass.neighbor_scan_max_edges // 24) >= 1)
        and ((.operations.connection_pass.neighbor_scan_max_nodes // 16 | type) == "number")
        and ((.operations.connection_pass.neighbor_scan_max_nodes // 16) >= 1)
        and ((.operations.link_additions | type) == "object")
        and ((.operations.link_updates | type) == "object")
        and ((.operations.link_additions.enabled | type) == "boolean")
        and ((.operations.link_updates.enabled | type) == "boolean")
        and ((.operations.link_additions.items | type) == "array")
        and ((.operations.link_updates.items | type) == "array")
        and (
            ((.operations.rollup.chunk_max_items // 0) | type) == "number"
            and (.operations.rollup.chunk_max_items // 0) >= 0
        )
        and (
            (.operations.rollup.scope // {} | type) == "object"
        )
        and (.budgets.max_links_added >= ((if .operations.link_additions.enabled then (.operations.link_additions.items | length) else 0 end) + connection_links))
        and (.budgets.max_links_updated >= (if .operations.link_updates.enabled then (.operations.link_updates.items | length) else 0 end))
        and (.budgets.max_rollups >= rollup_runs)
        and (.budgets.max_nodes_affected >= (if .operations.rollup.enabled then (.proposals.rollup.eligible_count // 0) else 0 end))
    ' "${plan_file}" > /dev/null
}

planned_total_ops() {
    local plan_file="$1"
    jq -r '
        def rollup_runs:
            if (.operations.rollup.enabled | not) then 0
            else
                ((.proposals.rollup.eligible_count // 0) as $eligible |
                 (.operations.rollup.chunk_max_items // 0) as $chunk |
                 if $eligible <= 0 then 0
                 elif $chunk > 0 then (($eligible + $chunk - 1) / $chunk | floor)
                 else 1
                 end)
            end;
        def neighbor_links_per_rollup:
            (.operations.connection_pass.max_neighbor_links_per_rollup // 0);
        def connection_links:
            if ((.operations.connection_pass.enabled // false) and (.operations.rollup.enabled // false)) then
                (
                    rollup_runs
                    + (if (.operations.connection_pass.propagate_neighbor_links // false) then (rollup_runs * neighbor_links_per_rollup) else 0 end)
                )
            else
                0
            end;
        (if .operations.link_additions.enabled then (.operations.link_additions.items | length) else 0 end)
        + (if .operations.link_updates.enabled then (.operations.link_updates.items | length) else 0 end)
        + rollup_runs
        + connection_links
    ' "${plan_file}"
}

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
        --model)
            model_name="${2:-}"
            shift 2
            ;;
        --locked-plan)
            locked_plan=true
            shift
            ;;
        --skip-codex)
            skip_codex=true
            shift
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

if [[ -z "${db_path}" || -z "${run_dir}" ]]; then
    usage >&2
    exit 1
fi
if [[ -z "${plan_path}" ]]; then
    plan_path="${run_dir}/pruner_plan.json"
fi
if [[ -z "${session_id}" ]]; then
    session_id="mem-nightly-$(date +%Y%m%d)"
fi
if [[ ! -f "${plan_path}" ]]; then
    echo "plan not found: ${plan_path}" >&2
    exit 1
fi

normalize_plan_basic_fields "${plan_path}" "${db_path}"
if [[ ! -x "${PRUNER_BASE}" ]]; then
    echo "missing executable: ${PRUNER_BASE}" >&2
    exit 1
fi

if [[ "${locked_plan}" == "true" ]]; then
    if [[ "${apply_mode}" != "true" ]]; then
        echo "--locked-plan requires --apply." >&2
        exit 1
    fi
    skip_codex=true
fi

if ! validate_plan_basic "${plan_path}"; then
    echo "plan validation failed: missing required top-level schema fields or unsupported version (expected [1,2])." >&2
    exit 1
fi

if [[ "${skip_codex}" != "true" ]]; then
    if ! command -v "${CODEX_BIN}" > /dev/null 2>&1; then
        echo "codex binary not found: ${CODEX_BIN}" >&2
        exit 1
    fi

    prompt_file="$(mktemp)"
    trap 'rm -f "${prompt_file}"' EXIT

    cat > "${prompt_file}" <<EOF
You are executing the Nightly Pruner review pass for CodeWork Memory DB.

Follow this skill contract first:
- ${SKILL_FILE}

Run context:
- db_path: ${db_path}
- run_dir: ${run_dir}
- plan_path: ${plan_path}
- apply_mode: ${apply_mode}
- session_id: ${session_id}

Input artifacts to read:
- ${run_dir}/input_snapshot.json
- ${run_dir}/reader_daily_summary.md
- ${plan_path}

Task:
1. Review and refine ${plan_path} if needed.
2. Keep budgets bounded and conservative.
3. Ensure JSON remains valid and schema fields remain present.
4. Write a concise review note to:
   - ${run_dir}/codex_pruner_review.md

Hard constraints:
- Do not execute DB write commands in this review pass.
- Keep \`operations.rollup.enabled\` false unless explicitly justified by plan evidence.
- Do not print full JSON artifacts. Use bounded \`jq\` extracts (counts, selected keys, first <= 20 rows).
- Keep canonical operations lane schema:
  - \`operations.link_additions = { enabled: <bool>, items: [] }\`
  - \`operations.link_updates = { enabled: <bool>, items: [] }\`
- For rollup operations, preserve/apply optional scoped chunk controls when present:
  - \`operations.rollup.scope = { workspace, project, kind }\`
  - \`operations.rollup.chunk_max_items = <n>\`
- Preserve connection-pass fields when present:
  - \`operations.connection_pass = { enabled, link_kind, anchor_kind, anchor_item_id, ensure_min_degree, propagate_neighbor_links, max_neighbor_links_per_rollup, neighbor_scan_max_edges, neighbor_scan_max_nodes }\`
- If \`apply_mode=true\` and plan contains approved operations, do not force budgets to zero below planned operation counts.
- If \`apply_mode=false\` and no operations are proposed, conservative zero budgets are acceptable.

When done:
- Run \`jq '.' ${plan_path}\`.
- Return a concise summary of adjustments.
EOF

    codex_args=( exec --full-auto --sandbox workspace-write --skip-git-repo-check --cd "${ROOT_DIR}" --output-last-message "${run_dir}/codex_pruner_last_message.md" - )
    if [[ -n "${model_name}" ]]; then
        codex_args=( exec --full-auto --sandbox workspace-write --skip-git-repo-check --cd "${ROOT_DIR}" --model "${model_name}" --output-last-message "${run_dir}/codex_pruner_last_message.md" - )
    fi
    set +e
    OTEL_SDK_DISABLED=true "${CODEX_BIN}" "${codex_args[@]}" < "${prompt_file}"
    codex_status=$?
    set -e
    if (( codex_status != 0 )); then
        echo "codex pruner review pass failed with exit code ${codex_status}" >&2
        echo "hint: verify codex auth/network, or rerun with --skip-codex to execute base pruner only." >&2
        echo "run_dir=${run_dir}" >&2
        exit "${codex_status}"
    fi
fi

normalize_plan_basic_fields "${plan_path}" "${db_path}"
if ! validate_plan_basic "${plan_path}"; then
    echo "plan validation failed after review: missing required top-level schema fields or unsupported version (expected [1,2])." >&2
    exit 1
fi

if [[ "${apply_mode}" == "true" ]]; then
    if ! validate_plan_apply_strict "${plan_path}"; then
        echo "plan validation failed for apply mode: expected canonical operations schema with budgets >= approved operation counts." >&2
        exit 1
    fi
    planned_total="$(planned_total_ops "${plan_path}")"
    if (( planned_total == 0 )); then
        if [[ "${allow_empty_apply}" != "true" ]]; then
            echo "apply mode requested but plan has zero approved operations; refusing apply. pass --allow-empty-apply to permit intentional no-op apply." >&2
            exit 2
        fi
    fi
fi

pruner_args=( --db "${db_path}" --run-dir "${run_dir}" --plan "${plan_path}" --session-id "${session_id}" )
if [[ "${apply_mode}" == "true" ]]; then
    pruner_args+=( --apply )
fi
if [[ "${allow_empty_apply}" == "true" ]]; then
    pruner_args+=( --allow-empty-apply )
fi
"${PRUNER_BASE}" "${pruner_args[@]}"

echo "pruner codex pass complete:"
echo "  run_dir=${run_dir}"
echo "  report=${run_dir}/pruner_apply_report.json"
