#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
READER_BASE="${ROOT_DIR}/shared/core/core_memdb/tools/mem_nightly_reader.sh"
CODEX_BIN="${CODEX_BIN:-codex}"
SKILL_FILE="${ROOT_DIR}/skills/memory-nightly-reader/SKILL.md"

usage() {
    cat <<'EOF'
usage: mem_nightly_reader_codex.sh --db <path> [options]

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
  --model <name>               Optional codex model override
  --skip-codex                 Only run base reader artifact generation
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
model_name=""
skip_codex=false

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
        --model)
            model_name="${2:-}"
            shift 2
            ;;
        --skip-codex)
            skip_codex=true
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

if [[ ! -x "${READER_BASE}" ]]; then
    echo "missing executable: ${READER_BASE}" >&2
    exit 1
fi

if [[ -z "${run_dir}" ]]; then
    mkdir -p "${runs_root}"
    run_dir="${runs_root}/$(date +%Y-%m-%d_%H%M%S)"
fi
mkdir -p "${run_dir}"

"${READER_BASE}" \
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

if [[ "${skip_codex}" == "true" ]]; then
    echo "reader codex refinement skipped (--skip-codex)."
    echo "run_dir=${run_dir}"
    exit 0
fi

if ! command -v "${CODEX_BIN}" > /dev/null 2>&1; then
    echo "codex binary not found: ${CODEX_BIN}" >&2
    echo "run_dir=${run_dir}"
    exit 1
fi

prompt_file="$(mktemp)"
trap 'rm -f "${prompt_file}"' EXIT

cat > "${prompt_file}" <<EOF
You are executing the Nightly Reader pass for CodeWork Memory DB.

Follow this skill contract first:
- ${SKILL_FILE}

Run context:
- db_path: ${db_path}
- workspace: ${workspace_key}
- project: ${project_key}
- run_dir: ${run_dir}

Input artifacts to read:
- ${run_dir}/input_snapshot.json
- ${run_dir}/all_items_scan.json
- ${run_dir}/stale_candidates.json
- ${run_dir}/health_before.json

Required outputs to write/update:
- ${run_dir}/reader_daily_summary.md
- ${run_dir}/pruner_plan.json

Hard constraints:
1. Do not run DB write commands.
2. Keep retrieval bounded if any additional reads are needed.
3. Keep plan budgets conservative.
4. Ensure \`operations.rollup.enabled\` remains false by default.
5. Ensure \`${run_dir}/pruner_plan.json\` is valid JSON.
6. Do not print full JSON artifacts; use bounded \`jq\` extracts (counts, selected keys, first <= 20 rows).
7. Emit canonical operations lanes in \`pruner_plan.json\`:
   - \`operations.connection_pass = { enabled: <bool>, link_kind: <kind>, anchor_kind: <kind>, anchor_item_id: <id>, ensure_min_degree: <bool>, propagate_neighbor_links: <bool>, max_neighbor_links_per_rollup: <n>, neighbor_scan_max_edges: <n>, neighbor_scan_max_nodes: <n> }\`
   - \`operations.link_additions = { enabled: <bool>, items: [] }\`
   - \`operations.link_updates = { enabled: <bool>, items: [] }\`
8. Keep plan schema version stable and compatible (\`version\` in [1,2]); do not emit incompatible version values.

When done:
- Run \`jq '.' ${run_dir}/pruner_plan.json\` to validate JSON.
- Return a concise completion message with key changes.
EOF

codex_args=( exec --full-auto --sandbox workspace-write --skip-git-repo-check --cd "${ROOT_DIR}" --output-last-message "${run_dir}/codex_reader_last_message.md" - )
if [[ -n "${model_name}" ]]; then
    codex_args=( exec --full-auto --sandbox workspace-write --skip-git-repo-check --cd "${ROOT_DIR}" --model "${model_name}" --output-last-message "${run_dir}/codex_reader_last_message.md" - )
fi

set +e
OTEL_SDK_DISABLED=true "${CODEX_BIN}" "${codex_args[@]}" < "${prompt_file}"
codex_status=$?
set -e
if (( codex_status != 0 )); then
    echo "codex reader pass failed with exit code ${codex_status}" >&2
    echo "hint: verify codex auth/network, or rerun with --skip-codex for offline baseline artifacts." >&2
    echo "run_dir=${run_dir}" >&2
    exit "${codex_status}"
fi

echo "reader codex pass complete:"
echo "  run_dir=${run_dir}"
echo "  codex_last_message=${run_dir}/codex_reader_last_message.md"
