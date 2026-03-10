#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'EOF'
usage: mem_nightly_reader.sh --db <path> --run-dir <dir> [options]

required:
  --db <path>                  SQLite DB path
  --run-dir <dir>              Output directory for this run

options:
  --workspace <key>            Default: codework
  --project <key>              Default: memory_console
  --stale-days <n>             Default: 30
  --min-active-nodes-before-rollup <n>
                               Default: 40 (rollup recommended only when active nodes exceed this)
  --min-stale-candidates-before-rollup <n>
                               Default: 4
  --before-ns <timestamp_ns>   Explicit stale cutoff override
  --now-ns <timestamp_ns>      Override current time for deterministic tests
  --scan-limit <n>             Max items scanned for stale detection (default: 800)
  --page-size <n>              Query page size while scanning (default: 200)
  --candidate-limit <n>        Max stale candidates in plan (default: 80)
  --rollup-chunk-max-items <n> Max items per rollup block when apply is enabled (default: 12)
  --rollup-max-groups <n>      Max planned rollup blocks per run (default: 8)
  --canonical-limit <n>        Canonical retrieval limit (default: 24)
  --pinned-limit <n>           Pinned retrieval limit (default: 24)
  --recent-limit <n>           Recent retrieval limit (default: 48)
  --events-limit <n>           Recent event rows (default: 200)
  --audits-limit <n>           Recent audit rows (default: 200)
  -h, --help                   Show this help
EOF
}

db_path=""
run_dir=""
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
canonical_limit=24
pinned_limit=24
recent_limit=48
events_limit=200
audits_limit=200
before_ns=""
now_ns=""

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
        --before-ns)
            before_ns="${2:-}"
            shift 2
            ;;
        --now-ns)
            now_ns="${2:-}"
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
        --canonical-limit)
            canonical_limit="${2:-}"
            shift 2
            ;;
        --pinned-limit)
            pinned_limit="${2:-}"
            shift 2
            ;;
        --recent-limit)
            recent_limit="${2:-}"
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

if [[ -z "${now_ns}" ]]; then
    now_ns="$(( $(date +%s) * 1000000000 ))"
fi
if [[ -z "${before_ns}" ]]; then
    before_ns="$(( now_ns - stale_days * 86400 * 1000000000 ))"
fi

generated_at_utc="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
run_date="$(date +"%Y-%m-%d")"

health_path="${run_dir}/health_before.json"
canonical_path="${run_dir}/canonical_items.json"
pinned_path="${run_dir}/pinned_items.json"
recent_path="${run_dir}/recent_items.json"
events_path="${run_dir}/recent_events.json"
audits_path="${run_dir}/recent_audits.json"
rollup_kind_path="${run_dir}/rollup_kind_items.json"
scan_path="${run_dir}/all_items_scan.json"
stale_path="${run_dir}/stale_candidates.json"
snapshot_path="${run_dir}/input_snapshot.json"
summary_path="${run_dir}/reader_daily_summary.md"
plan_path="${run_dir}/pruner_plan.json"

tmp_all="$(mktemp)"
tmp_new="$(mktemp)"
trap 'rm -f "${tmp_all}" "${tmp_new}"' EXIT
printf '[]\n' > "${tmp_all}"

"${MEM_CLI}" health --db "${db_path}" --format json > "${health_path}"
"${MEM_CLI}" query --db "${db_path}" --workspace "${workspace_key}" --project "${project_key}" --canonical-only --limit "${canonical_limit}" --format json > "${canonical_path}"
"${MEM_CLI}" query --db "${db_path}" --workspace "${workspace_key}" --project "${project_key}" --pinned-only --limit "${pinned_limit}" --format json > "${pinned_path}"
"${MEM_CLI}" query --db "${db_path}" --workspace "${workspace_key}" --project "${project_key}" --limit "${recent_limit}" --format json > "${recent_path}"
"${MEM_CLI}" event-list --db "${db_path}" --limit "${events_limit}" --format json > "${events_path}"
"${MEM_CLI}" audit-list --db "${db_path}" --limit "${audits_limit}" --format json > "${audits_path}"
"${MEM_CLI}" query --db "${db_path}" --workspace "${workspace_key}" --project "${project_key}" --kind rollup --limit "${scan_limit}" --format json > "${rollup_kind_path}"

offset=0
scanned=0
while (( scanned < scan_limit )); do
    remaining=$(( scan_limit - scanned ))
    chunk_limit="${page_size}"
    if (( remaining < chunk_limit )); then
        chunk_limit="${remaining}"
    fi
    chunk_json="$("${MEM_CLI}" query --db "${db_path}" --workspace "${workspace_key}" --project "${project_key}" --limit "${chunk_limit}" --offset "${offset}" --format json)"
    chunk_count="$(printf '%s\n' "${chunk_json}" | jq 'length')"
    if (( chunk_count == 0 )); then
        break
    fi
    jq -n --slurpfile a "${tmp_all}" --argjson b "${chunk_json}" '$a[0] + $b' > "${tmp_new}"
    mv "${tmp_new}" "${tmp_all}"
    scanned=$(( scanned + chunk_count ))
    offset=$(( offset + chunk_count ))
    if (( chunk_count < chunk_limit )); then
        break
    fi
done

cp "${tmp_all}" "${scan_path}"

jq --argjson before "${before_ns}" --argjson max "${candidate_limit}" --slurpfile rollup_items "${rollup_kind_path}" '
    ($rollup_items[0] | map(.id)) as $rollup_ids
    | map(select((.id) as $item_id | (.pinned|tonumber)==0 and (.canonical|tonumber)==0 and (.updated_ns|tonumber) < $before and (($rollup_ids | index($item_id)) | not)))
    | sort_by(.updated_ns|tonumber)
    | .[:$max]
' "${scan_path}" > "${stale_path}"

jq -n \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg run_date "${run_date}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --argjson now_ns "${now_ns}" \
    --argjson before_ns "${before_ns}" \
    --argjson stale_days "${stale_days}" \
    --argjson min_active_nodes_before_rollup "${min_active_nodes_before_rollup}" \
    --argjson min_stale_candidates_before_rollup "${min_stale_candidates_before_rollup}" \
    --argjson scan_limit "${scan_limit}" \
    --slurpfile health "${health_path}" \
    --slurpfile canonical "${canonical_path}" \
    --slurpfile pinned "${pinned_path}" \
    --slurpfile recent "${recent_path}" \
    --slurpfile events "${events_path}" \
    --slurpfile audits "${audits_path}" \
    --slurpfile rollup_items "${rollup_kind_path}" \
    --slurpfile all_items "${scan_path}" \
    --slurpfile stale "${stale_path}" \
    '{
        metadata: {
            generated_at_utc: $generated_at_utc,
            run_date: $run_date,
            db_path: $db_path,
            workspace: $workspace,
            project: $project,
            now_ns: $now_ns,
            stale_cutoff_before_ns: $before_ns,
            stale_days: $stale_days,
            min_active_nodes_before_rollup: $min_active_nodes_before_rollup,
            min_stale_candidates_before_rollup: $min_stale_candidates_before_rollup,
            scan_limit: $scan_limit
        },
        health_before: $health[0],
        canonical_items: $canonical[0],
        pinned_items: $pinned[0],
        recent_items: $recent[0],
        recent_events: $events[0],
        recent_audits: $audits[0],
        rollup_kind_items: $rollup_items[0],
        scanned_items: $all_items[0],
        stale_candidates: $stale[0]
    }' > "${snapshot_path}"

stale_count="$(jq 'length' "${stale_path}")"
stale_ids="$(jq '[.[].id]' "${stale_path}")"
all_count="$(jq 'length' "${scan_path}")"
recent_count="$(jq 'length' "${recent_path}")"
canonical_count="$(jq 'length' "${canonical_path}")"
pinned_count="$(jq 'length' "${pinned_path}")"
event_count="$(jq 'length' "${events_path}")"
audit_count="$(jq 'length' "${audits_path}")"
rollup_kind_count="$(jq 'length' "${rollup_kind_path}")"

gate_active_nodes_met=false
if (( all_count > min_active_nodes_before_rollup )); then
    gate_active_nodes_met=true
fi
gate_stale_count_met=false
if (( stale_count >= min_stale_candidates_before_rollup )); then
    gate_stale_count_met=true
fi

rollup_recommended=false
if (( stale_count > 0 )) && [[ "${gate_active_nodes_met}" == "true" && "${gate_stale_count_met}" == "true" ]]; then
    rollup_recommended=true
fi

suggested_rollups=0
if [[ "${rollup_recommended}" == "true" ]]; then
    if (( rollup_chunk_max_items > 0 )); then
        suggested_rollups=$(( (stale_count + rollup_chunk_max_items - 1) / rollup_chunk_max_items ))
    else
        suggested_rollups=1
    fi
    if (( suggested_rollups > rollup_max_groups )); then
        suggested_rollups="${rollup_max_groups}"
    fi
fi

connection_max_neighbor_links_per_rollup=6
connection_neighbor_scan_max_edges=48
connection_neighbor_scan_max_nodes=24

budget_max_links_added=0
budget_max_rollups=0
budget_max_nodes_affected=0
if [[ "${rollup_recommended}" == "true" ]]; then
    budget_max_rollups="${suggested_rollups}"
    budget_max_nodes_affected="${stale_count}"
    budget_max_links_added=$(( (suggested_rollups * (connection_max_neighbor_links_per_rollup + 1)) + 2 ))
fi

jq -n \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --argjson before_ns "${before_ns}" \
    --argjson stale_count "${stale_count}" \
    --argjson stale_ids "${stale_ids}" \
    --argjson all_count "${all_count}" \
    --argjson min_active_nodes_before_rollup "${min_active_nodes_before_rollup}" \
    --argjson min_stale_candidates_before_rollup "${min_stale_candidates_before_rollup}" \
    --argjson gate_active_nodes_met "${gate_active_nodes_met}" \
    --argjson gate_stale_count_met "${gate_stale_count_met}" \
    --argjson rollup_recommended "${rollup_recommended}" \
    --argjson suggested_rollups "${suggested_rollups}" \
    --argjson rollup_chunk_max_items "${rollup_chunk_max_items}" \
    --argjson budget_max_links_added "${budget_max_links_added}" \
    --argjson budget_max_rollups "${budget_max_rollups}" \
    --argjson budget_max_nodes_affected "${budget_max_nodes_affected}" \
    --argjson connection_max_neighbor_links_per_rollup "${connection_max_neighbor_links_per_rollup}" \
    --argjson connection_neighbor_scan_max_edges "${connection_neighbor_scan_max_edges}" \
    --argjson connection_neighbor_scan_max_nodes "${connection_neighbor_scan_max_nodes}" \
    '{
        version: 1,
        generated_at_utc: $generated_at_utc,
        db_path: $db_path,
        workspace: $workspace,
        project: $project,
        mode: "manual_dry_run_default",
        policy: {
            rollup_gate: {
                min_active_nodes_before_rollup: $min_active_nodes_before_rollup,
                min_stale_candidates_before_rollup: $min_stale_candidates_before_rollup,
                active_nodes_in_scope: $all_count,
                stale_candidates_in_scope: $stale_count,
                gate_active_nodes_met: $gate_active_nodes_met,
                gate_stale_count_met: $gate_stale_count_met
            }
        },
        budgets: {
            max_links_added: $budget_max_links_added,
            max_links_updated: 0,
            max_rollups: $budget_max_rollups,
            max_nodes_affected: $budget_max_nodes_affected,
            max_deletions: 0
        },
        proposals: {
            rollup: {
                recommended: $rollup_recommended,
                before_ns: $before_ns,
                eligible_count: $stale_count,
                eligible_item_ids: $stale_ids,
                suggested_rollup_groups: $suggested_rollups,
                chunk_max_items: $rollup_chunk_max_items,
                reason: (
                    if $stale_count == 0 then
                        "No stale candidates found."
                    elif ($gate_active_nodes_met | not) then
                        ("Rollup deferred: active nodes in scope (" + ($all_count|tostring) + ") do not exceed min gate (" + ($min_active_nodes_before_rollup|tostring) + ").")
                    elif ($gate_stale_count_met | not) then
                        ("Rollup deferred: stale candidates (" + ($stale_count|tostring) + ") below min gate (" + ($min_stale_candidates_before_rollup|tostring) + ").")
                    else
                        ("Rollup recommended: stale candidates (" + ($stale_count|tostring) + "), active nodes (" + ($all_count|tostring) + "), chunk_max_items=" + ($rollup_chunk_max_items|tostring) + ".")
                    end
                )
            },
            link_additions: [],
            link_updates: []
        },
        operations: {
            rollup: {
                enabled: false,
                before_ns: $before_ns,
                chunk_max_items: $rollup_chunk_max_items,
                scope: {
                    workspace: $workspace,
                    project: $project,
                    kind: ""
                }
            },
            connection_pass: {
                enabled: $rollup_recommended,
                link_kind: "related",
                anchor_kind: "summarizes",
                anchor_item_id: 0,
                ensure_min_degree: true,
                propagate_neighbor_links: true,
                max_neighbor_links_per_rollup: $connection_max_neighbor_links_per_rollup,
                neighbor_scan_max_edges: $connection_neighbor_scan_max_edges,
                neighbor_scan_max_nodes: $connection_neighbor_scan_max_nodes
            },
            link_additions: {
                enabled: false,
                items: []
            },
            link_updates: {
                enabled: false,
                items: []
            }
        },
        notes: [
            "Reader is read-only; no DB mutations were performed.",
            "Rollup recommendation is policy-gated by active-node pressure and stale-candidate minimum thresholds.",
            "Set operations.rollup.enabled=true and run pruner with --apply only when policy gates and human review agree.",
            "Rollup supports chunked groups via operations.rollup.chunk_max_items and scope filters in operations.rollup.scope.",
            "Connection pass links rollup blocks after apply so new rollup nodes remain graph-connected, with optional neighbor-link propagation.",
            "Add explicit operations.link_additions.items / operations.link_updates.items before apply if needed."
        ]
    }' > "${plan_path}"

{
    echo "# Nightly Reader Summary (${run_date})"
    echo
    echo "- generated_at_utc: ${generated_at_utc}"
    echo "- db_path: ${db_path}"
    echo "- workspace/project: ${workspace_key}/${project_key}"
    echo "- stale_cutoff_before_ns: ${before_ns}"
    echo "- min_active_nodes_before_rollup: ${min_active_nodes_before_rollup}"
    echo "- min_stale_candidates_before_rollup: ${min_stale_candidates_before_rollup}"
    echo "- rollup_chunk_max_items: ${rollup_chunk_max_items}"
    echo "- suggested_rollup_groups: ${suggested_rollups}"
    echo "- scanned_items: ${all_count}"
    echo "- stale_candidates: ${stale_count}"
    echo "- recent_items: ${recent_count}"
    echo "- canonical_items: ${canonical_count}"
    echo "- pinned_items: ${pinned_count}"
    echo "- recent_events: ${event_count}"
    echo "- recent_audits: ${audit_count}"
    echo "- rollup_kind_items: ${rollup_kind_count}"
    echo "- rollup_gate_active_nodes_met: ${gate_active_nodes_met}"
    echo "- rollup_gate_stale_count_met: ${gate_stale_count_met}"
    echo "- rollup_recommended: ${rollup_recommended}"
    echo
    echo "## Stale Candidate Preview (oldest first, max 20)"
    if (( stale_count == 0 )); then
        echo "- none"
    else
        jq -r '.[] | "- id=\(.id) updated_ns=\(.updated_ns) title=\(.title)"' "${stale_path}" | head -n 20
    fi
    echo
    echo "## Output Artifacts"
    echo "- ${health_path}"
    echo "- ${snapshot_path}"
    echo "- ${summary_path}"
    echo "- ${plan_path}"
} > "${summary_path}"

echo "reader complete:"
echo "  snapshot: ${snapshot_path}"
echo "  summary:  ${summary_path}"
echo "  plan:     ${plan_path}"
