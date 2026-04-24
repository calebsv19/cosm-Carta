#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'EOF'
usage: mem_hierarchy_migrate.sh --db <path> [options]

Purpose:
  Seed hierarchy pillar anchors and generate/apply a bounded recategorization link plan.

Modes:
  --dry-run (default)  Build reports only (no DB writes).
  --apply              Apply missing anchors and planned link additions.

Options:
  --db <path>                    Target DB path (required)
  --workspace <key>              Default: codework
  --project <key>                Project bucket (repeatable). Default: memory_console
  --use-doc-project-buckets      Use every directory under docs/private_program_docs as project set
  --scan-limit <n>               Query limit per project for recategorization planning (default: 1000)
  --max-link-adds <n>            Max link adds in apply mode (default: 250)
  --link-kind <kind>             Canonical link kind for recategorization links (default: related)
  --run-dir <dir>                Optional explicit run dir
  --runs-root <dir>              Default: docs/private_program_docs/memory_console/hierarchy_migrations
  --session-id <id>              Default: mem-hierarchy-migrate-<yyyymmdd>
  --session-max-writes <n>       Default: 500
  --seed-only                    Only create missing anchors; skip recategorization apply
  --dry-run                      Force dry-run mode
  --apply                        Force apply mode
  -h, --help                     Show this help
EOF
}

require_cmd() {
    local cmd="$1"
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "required command not found: ${cmd}" >&2
        exit 1
    fi
}

abs_path() {
    local p="$1"
    local d
    d="$(dirname "${p}")"
    mkdir -p "${d}"
    echo "$(cd "${d}" && pwd -P)/$(basename "${p}")"
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

append_jsonl() {
    local file="$1"
    local line="$2"
    printf '%s\n' "${line}" >> "${file}"
}

db_path=""
workspace_key="codework"
scan_limit=1000
max_link_adds=250
link_kind="related"
runs_root="${ROOT_DIR}/docs/private_program_docs/memory_console/hierarchy_migrations"
run_dir=""
session_id="mem-hierarchy-migrate-$(date +%Y%m%d)"
session_max_writes=500
apply_mode=false
seed_only=false
use_doc_project_buckets=false
projects=()

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
            projects+=("${2:-}")
            shift 2
            ;;
        --use-doc-project-buckets)
            use_doc_project_buckets=true
            shift
            ;;
        --scan-limit)
            scan_limit="${2:-}"
            shift 2
            ;;
        --max-link-adds)
            max_link_adds="${2:-}"
            shift 2
            ;;
        --link-kind)
            link_kind="${2:-}"
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
        --session-id)
            session_id="${2:-}"
            shift 2
            ;;
        --session-max-writes)
            session_max_writes="${2:-}"
            shift 2
            ;;
        --seed-only)
            seed_only=true
            shift
            ;;
        --dry-run)
            apply_mode=false
            shift
            ;;
        --apply)
            apply_mode=true
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
    echo "--db <path> is required" >&2
    usage >&2
    exit 1
fi
if [[ ! -x "${MEM_CLI}" ]]; then
    echo "mem_cli not found at ${MEM_CLI}" >&2
    echo "build with: make -C shared/core/core_memdb all" >&2
    exit 1
fi
require_cmd jq

if [[ "${scan_limit}" == *[!0-9]* || "${scan_limit}" -lt 1 || "${scan_limit}" -gt 1000 ]]; then
    echo "--scan-limit must be an integer in [1,1000]" >&2
    exit 1
fi
if [[ "${max_link_adds}" == *[!0-9]* || "${max_link_adds}" -lt 0 ]]; then
    echo "--max-link-adds must be a non-negative integer" >&2
    exit 1
fi
if [[ "${session_max_writes}" == *[!0-9]* || "${session_max_writes}" -lt 1 ]]; then
    echo "--session-max-writes must be a positive integer" >&2
    exit 1
fi
if ! is_allowed_link_kind "${link_kind}"; then
    echo "--link-kind must be one of: supports depends_on references summarizes implements blocks contradicts related" >&2
    exit 1
fi

db_path="$(abs_path "${db_path}")"
runs_root="$(abs_path "${runs_root}")"
if [[ -z "${run_dir}" ]]; then
    run_dir="${runs_root}/hierarchy_migrate_$(date +%Y-%m-%d_%H%M%S)"
fi
run_dir="$(abs_path "${run_dir}")"

if [[ ! -f "${db_path}" ]]; then
    echo "db not found: ${db_path}" >&2
    exit 1
fi

mkdir -p "${runs_root}"
mkdir -p "${run_dir}"

if [[ "${use_doc_project_buckets}" == "true" ]]; then
    while IFS= read -r p; do
        if [[ -n "${p}" ]]; then
            projects+=("${p}")
        fi
    done < <(find "${ROOT_DIR}/docs/private_program_docs" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort)
fi

if (( ${#projects[@]} == 0 )); then
    projects=("memory_console")
fi

# De-duplicate projects while preserving order.
uniq_projects=()
seen_projects="|"
for p in "${projects[@]}"; do
    if [[ -z "${p}" ]]; then
        continue
    fi
    if [[ "${seen_projects}" == *"|${p}|"* ]]; then
        continue
    fi
    seen_projects+="${p}|"
    uniq_projects+=("${p}")
done
projects=("${uniq_projects[@]}")

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

anchors_jsonl="${tmp_dir}/anchors.jsonl"
recat_jsonl="${tmp_dir}/recategorize_plan.jsonl"
apply_jsonl="${tmp_dir}/apply_results.jsonl"
touch "${anchors_jsonl}" "${recat_jsonl}" "${apply_jsonl}"

config_json="${run_dir}/config.json"
health_before_json="${run_dir}/health_before.json"
anchors_json="${run_dir}/anchors.json"
recat_plan_json="${run_dir}/recategorize_plan.json"
apply_results_json="${run_dir}/apply_results.json"
summary_json="${run_dir}/summary.json"
all_items_list_json="${tmp_dir}/all_items_list.json"

jq -n \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg mode "$(if [[ "${apply_mode}" == "true" ]]; then echo "apply"; else echo "dry-run"; fi)" \
    --argjson seed_only "$(if [[ "${seed_only}" == "true" ]]; then echo true; else echo false; fi)" \
    --arg session_id "${session_id}" \
    --arg link_kind "${link_kind}" \
    --arg run_dir "${run_dir}" \
    --arg runs_root "${runs_root}" \
    --argjson scan_limit "${scan_limit}" \
    --argjson max_link_adds "${max_link_adds}" \
    --argjson session_max_writes "${session_max_writes}" \
    --argjson projects "$(printf '%s\n' "${projects[@]}" | jq -R . | jq -s .)" \
    '{
      db_path: $db_path,
      workspace: $workspace,
      mode: $mode,
      seed_only: $seed_only,
      session_id: $session_id,
      session_max_writes: $session_max_writes,
      scan_limit: $scan_limit,
      max_link_adds: $max_link_adds,
      link_kind: $link_kind,
      run_dir: $run_dir,
      runs_root: $runs_root,
      projects: $projects
    }' > "${config_json}"

"${MEM_CLI}" health --db "${db_path}" --format json > "${health_before_json}"
"${MEM_CLI}" list --db "${db_path}" --format json > "${all_items_list_json}"

find_anchor_id() {
    local _project="$1"
    local stable_id="$2"
    local found_id=""
    found_id="$(jq -r --arg sid "${stable_id}" '[.[] | select(.stable_id == $sid)][0].id // ""' "${all_items_list_json}")"
    if [[ -n "${found_id}" ]]; then
        printf '%s\n' "${found_id}"
        return
    fi
    find_anchor_id_include_archived "${stable_id}"
}

find_anchor_id_include_archived() {
    local stable_id="$1"
    "${MEM_CLI}" query \
        --db "${db_path}" \
        --include-archived \
        --limit 1000 \
        --format json \
        | jq -r --arg sid "${stable_id}" '[.[] | select(.stable_id == $sid)][0].id // ""'
}

safe_title() {
    local project="$1"
    local lane_label="$2"
    printf '%s %s Directory' "${project}" "${lane_label}"
}

ensure_anchor() {
    local project="$1"
    local lane="$2"
    local stable_id="$3"
    local kind="$4"
    local lane_label="$5"
    local found_id=""
    local status="existing"
    local note=""
    local add_output=""
    local new_id=""
    local title=""
    local body=""

    title="$(safe_title "${project}" "${lane_label}")"
    found_id="$(find_anchor_id "${project}" "${stable_id}")"
    if [[ -z "${found_id}" ]]; then
        if [[ "${apply_mode}" == "true" ]]; then
            body="Hierarchy pillar anchor for project=${project}, lane=${lane}. Seeded by mem_hierarchy_migrate.sh."
            add_err_file="${tmp_dir}/add_anchor_${project}_${lane}.err"
            if add_output="$("${MEM_CLI}" add \
                --db "${db_path}" \
                --title "${title}" \
                --body "${body}" \
                --stable-id "${stable_id}" \
                --workspace "${workspace_key}" \
                --project "${project}" \
                --kind "${kind}" \
                --session-id "${session_id}" \
                --session-max-writes "${session_max_writes}" 2>"${add_err_file}")"; then
                new_id="$(printf '%s\n' "${add_output}" | sed -n 's/^added id=\([0-9][0-9]*\).*/\1/p' | head -n 1)"
            else
                if grep -q "UNIQUE constraint failed: mem_item.stable_id" "${add_err_file}"; then
                    new_id="$(find_anchor_id_include_archived "${stable_id}")"
                    note="stable_id_conflict_existing_row"
                else
                    note="add_failed:$(tr '\n' ' ' < "${add_err_file}" | sed 's/[[:space:]]\\+/ /g' | sed 's/^ //; s/ $//')"
                fi
            fi
            if [[ -z "${new_id}" ]]; then
                new_id="$(find_anchor_id "${project}" "${stable_id}")"
            fi
            if [[ -n "${new_id}" ]]; then
                # Keep stable-id lookup cache warm for anchors created in this run.
                jq \
                    --arg sid "${stable_id}" \
                    --argjson id "${new_id}" \
                    --arg title "${title}" \
                    '. + [{id:$id,stable_id:$sid,title:$title}]' "${all_items_list_json}" > "${all_items_list_json}.tmp"
                mv "${all_items_list_json}.tmp" "${all_items_list_json}"
            fi
            if [[ -n "${new_id}" ]]; then
                found_id="${new_id}"
                status="added"
            else
                status="error"
                note="failed_to_add"
            fi
        else
            status="planned_add"
        fi
    fi

    append_jsonl "${anchors_jsonl}" "$(jq -nc \
        --arg project "${project}" \
        --arg lane "${lane}" \
        --arg stable_id "${stable_id}" \
        --arg kind "${kind}" \
        --arg title "${title}" \
        --arg status "${status}" \
        --arg item_id "${found_id}" \
        --arg note "${note}" \
        '{
          project: $project,
          lane: $lane,
          stable_id: $stable_id,
          kind: $kind,
          title: $title,
          status: $status,
          item_id: (if ($item_id|length)>0 then ($item_id|tonumber) else null end),
          note: (if ($note|length)>0 then $note else null end)
        }')"
}

for project in "${projects[@]}"; do
    ensure_anchor "${project}" "scope" "scope-${project}" "scope" "Scope"
    ensure_anchor "${project}" "plans" "plans-${project}" "plan" "Plans"
    ensure_anchor "${project}" "decisions" "decisions-${project}" "decision" "Decisions"
    ensure_anchor "${project}" "issues" "issues-${project}" "issue" "Issues"
    ensure_anchor "${project}" "misc" "misc-${project}" "summary" "Misc"
done

jq -s '.' "${anchors_jsonl}" > "${anchors_json}"

for project in "${projects[@]}"; do
    project_items_json="${tmp_dir}/items_${project}.json"
    "${MEM_CLI}" query \
        --db "${db_path}" \
        --project "${project}" \
        --limit "${scan_limit}" \
        --format json > "${project_items_json}"

    jq -c \
        --arg project "${project}" \
        '
        def anchor_sids: [
          ("scope-" + $project),
          ("plans-" + $project),
          ("decisions-" + $project),
          ("issues-" + $project),
          ("misc-" + $project)
        ];
        def classify($t; $sid):
          (($t + " " + $sid) | ascii_downcase) as $text
          | if ($text | test("(^|[^a-z])(issue|bug|fix|error|failure|failed|crash|regression|blocker)([^a-z]|$)")) then
              {target: ("issues-" + $project), reason: "keyword_issue"}
            elif ($text | test("(^|[^a-z])(decision|decide|chose|choose|policy|boundary|tradeoff|trade-off)([^a-z]|$)")) then
              {target: ("decisions-" + $project), reason: "keyword_decision"}
            elif ($text | test("(^|[^a-z])(plan|phase|roadmap|milestone|strategy|migration)([^a-z]|$)")) then
              {target: ("plans-" + $project), reason: "keyword_plan"}
            else
              {target: ("misc-" + $project), reason: "fallback_misc"}
            end;
        map(select(((.stable_id // "") as $sid | (anchor_sids | index($sid) | not))))
        | map(
            classify((.title // ""); (.stable_id // "")) as $c
            | {
                project: $project,
                item_id: .id,
                item_stable_id: (.stable_id // ""),
                item_title: (.title // ""),
                target_stable_id: $c.target,
                reason: $c.reason
              }
          )
        | .[]
        ' "${project_items_json}" >> "${recat_jsonl}"
done

if [[ -s "${recat_jsonl}" ]]; then
    jq -s '.' "${recat_jsonl}" > "${recat_plan_json}"
else
    echo "[]" > "${recat_plan_json}"
fi

link_adds_done=0
link_skips_already=0
link_skips_missing_anchor=0
link_skips_budget=0

if [[ "${apply_mode}" == "true" && "${seed_only}" != "true" ]]; then
    while IFS= read -r row; do
        if [[ -z "${row}" ]]; then
            continue
        fi
        project="$(printf '%s\n' "${row}" | jq -r '.project')"
        item_id="$(printf '%s\n' "${row}" | jq -r '.item_id')"
        target_stable_id="$(printf '%s\n' "${row}" | jq -r '.target_stable_id')"
        reason="$(printf '%s\n' "${row}" | jq -r '.reason')"
        item_stable_id="$(printf '%s\n' "${row}" | jq -r '.item_stable_id')"
        item_title="$(printf '%s\n' "${row}" | jq -r '.item_title')"

        target_id="$(jq -r \
            --arg project "${project}" \
            --arg sid "${target_stable_id}" \
            '[.[] | select(.project == $project and .stable_id == $sid)][0].item_id // ""' "${anchors_json}")"

        if [[ -z "${target_id}" || "${target_id}" == "null" ]]; then
            link_skips_missing_anchor=$((link_skips_missing_anchor + 1))
            append_jsonl "${apply_jsonl}" "$(jq -nc \
                --arg project "${project}" \
                --argjson item_id "${item_id}" \
                --arg item_stable_id "${item_stable_id}" \
                --arg item_title "${item_title}" \
                --arg target_stable_id "${target_stable_id}" \
                --arg reason "${reason}" \
                '{
                  project: $project,
                  item_id: $item_id,
                  item_stable_id: $item_stable_id,
                  item_title: $item_title,
                  target_stable_id: $target_stable_id,
                  target_item_id: null,
                  reason: $reason,
                  status: "skipped_missing_target_anchor"
                }')"
            continue
        fi

        if [[ "${item_id}" == "${target_id}" ]]; then
            append_jsonl "${apply_jsonl}" "$(jq -nc \
                --arg project "${project}" \
                --argjson item_id "${item_id}" \
                --arg item_stable_id "${item_stable_id}" \
                --arg item_title "${item_title}" \
                --arg target_stable_id "${target_stable_id}" \
                --argjson target_item_id "${target_id}" \
                --arg reason "${reason}" \
                '{
                  project: $project,
                  item_id: $item_id,
                  item_stable_id: $item_stable_id,
                  item_title: $item_title,
                  target_stable_id: $target_stable_id,
                  target_item_id: $target_item_id,
                  reason: $reason,
                  status: "skipped_self_target"
                }')"
            continue
        fi

        if (( link_adds_done >= max_link_adds )); then
            link_skips_budget=$((link_skips_budget + 1))
            append_jsonl "${apply_jsonl}" "$(jq -nc \
                --arg project "${project}" \
                --argjson item_id "${item_id}" \
                --arg item_stable_id "${item_stable_id}" \
                --arg item_title "${item_title}" \
                --arg target_stable_id "${target_stable_id}" \
                --argjson target_item_id "${target_id}" \
                --arg reason "${reason}" \
                '{
                  project: $project,
                  item_id: $item_id,
                  item_stable_id: $item_stable_id,
                  item_title: $item_title,
                  target_stable_id: $target_stable_id,
                  target_item_id: $target_item_id,
                  reason: $reason,
                  status: "skipped_budget"
                }')"
            continue
        fi

        link_list_text="$("${MEM_CLI}" link-list --db "${db_path}" --item-id "${item_id}" || true)"
        if printf '%s\n' "${link_list_text}" | grep -Eq "^[[:space:]]*[0-9]+[[:space:]]*\\|[[:space:]]*${item_id}[[:space:]]*->[[:space:]]*${target_id}[[:space:]]*\\|"; then
            link_skips_already=$((link_skips_already + 1))
            append_jsonl "${apply_jsonl}" "$(jq -nc \
                --arg project "${project}" \
                --argjson item_id "${item_id}" \
                --arg item_stable_id "${item_stable_id}" \
                --arg item_title "${item_title}" \
                --arg target_stable_id "${target_stable_id}" \
                --argjson target_item_id "${target_id}" \
                --arg reason "${reason}" \
                '{
                  project: $project,
                  item_id: $item_id,
                  item_stable_id: $item_stable_id,
                  item_title: $item_title,
                  target_stable_id: $target_stable_id,
                  target_item_id: $target_item_id,
                  reason: $reason,
                  status: "skipped_already_linked"
                }')"
            continue
        fi

        "${MEM_CLI}" link-add \
            --db "${db_path}" \
            --from "${item_id}" \
            --to "${target_id}" \
            --kind "${link_kind}" \
            --session-id "${session_id}" >/dev/null

        link_adds_done=$((link_adds_done + 1))
        append_jsonl "${apply_jsonl}" "$(jq -nc \
            --arg project "${project}" \
            --argjson item_id "${item_id}" \
            --arg item_stable_id "${item_stable_id}" \
            --arg item_title "${item_title}" \
            --arg target_stable_id "${target_stable_id}" \
            --argjson target_item_id "${target_id}" \
            --arg reason "${reason}" \
            --arg link_kind "${link_kind}" \
            '{
              project: $project,
              item_id: $item_id,
              item_stable_id: $item_stable_id,
              item_title: $item_title,
              target_stable_id: $target_stable_id,
              target_item_id: $target_item_id,
              reason: $reason,
              link_kind: $link_kind,
              status: "added"
            }')"
    done < <(jq -c '.[]' "${recat_plan_json}")
fi

if [[ -s "${apply_jsonl}" ]]; then
    jq -s '.' "${apply_jsonl}" > "${apply_results_json}"
else
    echo "[]" > "${apply_results_json}"
fi

jq -n \
    --arg mode "$(if [[ "${apply_mode}" == "true" ]]; then echo "apply"; else echo "dry-run"; fi)" \
    --argjson seed_only "$(if [[ "${seed_only}" == "true" ]]; then echo true; else echo false; fi)" \
    --arg run_dir "${run_dir}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg session_id "${session_id}" \
    --argjson projects "$(printf '%s\n' "${projects[@]}" | jq -R . | jq -s .)" \
    --argjson anchors "$(cat "${anchors_json}")" \
    --argjson recat "$(cat "${recat_plan_json}")" \
    --argjson apply_results "$(cat "${apply_results_json}")" \
    --argjson link_adds_done "${link_adds_done}" \
    --argjson link_skips_already "${link_skips_already}" \
    --argjson link_skips_missing_anchor "${link_skips_missing_anchor}" \
    --argjson link_skips_budget "${link_skips_budget}" \
    '{
      mode: $mode,
      seed_only: $seed_only,
      run_dir: $run_dir,
      db_path: $db_path,
      workspace: $workspace,
      session_id: $session_id,
      projects: $projects,
      anchor_summary: {
        existing: ($anchors | map(select(.status == "existing")) | length),
        planned_add: ($anchors | map(select(.status == "planned_add")) | length),
        added: ($anchors | map(select(.status == "added")) | length),
        errors: ($anchors | map(select(.status == "error")) | length)
      },
      recategorization_summary: {
        candidates: ($recat | length),
        by_reason: ($recat | group_by(.reason) | map({reason: .[0].reason, count: length})),
        by_project: ($recat | group_by(.project) | map({project: .[0].project, count: length}))
      },
      apply_summary: {
        link_adds_done: $link_adds_done,
        link_skips_already: $link_skips_already,
        link_skips_missing_anchor: $link_skips_missing_anchor,
        link_skips_budget: $link_skips_budget,
        result_rows: ($apply_results | length)
      }
    }' > "${summary_json}"

echo "hierarchy migrate complete:"
echo "  mode=$(if [[ "${apply_mode}" == "true" ]]; then echo "apply"; else echo "dry-run"; fi)"
echo "  seed_only=${seed_only}"
echo "  db=${db_path}"
echo "  run_dir=${run_dir}"
echo "  summary=${summary_json}"
jq '.anchor_summary, .recategorization_summary, .apply_summary' "${summary_json}"
