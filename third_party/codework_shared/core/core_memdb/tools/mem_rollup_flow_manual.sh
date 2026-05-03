#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
MEM_CLI="${ROOT_DIR}/shared/core/core_memdb/build/mem_cli"

usage() {
    cat <<'USAGE'
usage: mem_rollup_flow_manual.sh --db <path> [options]

Purpose:
  Manual rollup flow driver for the new memory-rollup skills.
  Defaults to read-only dry run; optional constrained single-group apply is supported.

required:
  --db <path>                  SQLite DB path

options:
  --run-dir <dir>              Output directory (default: docs/private_program_docs/memory_console/manual_rollup_runs/<timestamp>)
  --workspace <key>            Default: codework
  --project <key>              Default: mem_console (alias memory_console auto-normalized)
  --lane <name>                Default: plan (only plan is supported in v1)
  --stale-days <n>             Default: 30
  --before-ns <timestamp_ns>   Explicit stale cutoff override
  --now-ns <timestamp_ns>      Override current time for deterministic tests
  --scan-limit <n>             Max items scanned (default: 1200)
  --page-size <n>              Page size per query (default: 250, max 1000)
  --candidate-limit <n>        Max non-keep candidates retained (default: 120)
  --group-max-items <n>        Max source items per proposed rollup group (default: 6)
  --neighbor-scan-max-edges <n>
                               neighbors() max-edges per candidate scan (default: 48)
  --neighbor-scan-max-nodes <n>
                               neighbors() max-nodes per candidate scan (default: 24)
  --external-protect-min <n>   Keep candidate if external neighbor links >= n (default: 6)
  --link-rewire-topk <n>       Max proposed external links per rollup group (default: 8)
  --id-proximity-max-gap <n>   Max id distance for proximity fallback edges (default: 42)
  --similarity-max-id-gap <n>  Max id distance for title/phase fallback edges (default: 240)
  --title-overlap-min <n>      Min shared tokens for title fallback (default: 2)
  --title-jaccard-min-milli <n>
                               Min jaccard*1000 for title fallback (default: 340)
  --phase-minor-gap-max <n>    Max step/phase minor-number gap for sequence fallback (default: 2)
  --phase-overlap-min <n>      Min lexical overlap required for phase-sequence fallback (default: 1)
  --apply-group <n>            Apply exactly one proposed group index (1-based, constrained mode)
  --apply-max-source-items <n> Max source items allowed for constrained apply (default: 6)
  --apply-allow-singleton      Allow constrained apply for 1-item groups (default: off)
  --archive-db <path>          Optional archive SQLite DB path to copy source snapshots before source archive
  --no-archive-source-items    Keep source items active after apply (default: source items are archived)
  -h, --help                   Show this help
USAGE
}

normalize_project_key() {
    local value="${1:-}"
    case "${value}" in
        memory_console)
            echo "mem_console"
            ;;
        *)
            echo "${value}"
            ;;
    esac
}

require_int_ge() {
    local flag="$1"
    local value="$2"
    local min="$3"
    if ! [[ "${value}" =~ ^[0-9]+$ ]]; then
        echo "${flag} must be an integer (got: ${value})" >&2
        exit 1
    fi
    if (( value < min )); then
        echo "${flag} must be >= ${min} (got: ${value})" >&2
        exit 1
    fi
}

require_int_range() {
    local flag="$1"
    local value="$2"
    local min="$3"
    local max="$4"
    require_int_ge "${flag}" "${value}" "${min}"
    if (( value > max )); then
        echo "${flag} must be <= ${max} (got: ${value})" >&2
        exit 1
    fi
}

add_adj() {
    local a="$1"
    local b="$2"
    local cur="${ADJ_MAP[$a]:-}"
    if [[ -z "${cur}" ]]; then
        ADJ_MAP["$a"]="${b}"
        return
    fi
    if [[ ",${cur}," != *",${b},"* ]]; then
        ADJ_MAP["$a"]="${cur},${b}"
    fi
}

normalize_link_kind() {
    local kind="${1:-related}"
    case "${kind}" in
        supports|depends_on|references|summarizes|implements|blocks|contradicts|related)
            echo "${kind}"
            ;;
        *)
            echo "related"
            ;;
    esac
}

db_path=""
run_dir=""
workspace_key="codework"
project_key="mem_console"
lane_type="plan"
stale_days=30
before_ns=""
now_ns=""
scan_limit=1200
page_size=250
candidate_limit=120
group_max_items=6
neighbor_scan_max_edges=48
neighbor_scan_max_nodes=24
external_protect_min=6
link_rewire_topk=8
id_proximity_max_gap=42
similarity_max_id_gap=240
title_overlap_min=2
title_jaccard_min_milli=340
phase_minor_gap_max=2
phase_overlap_min=1
apply_group=0
apply_max_source_items=6
apply_allow_singleton=0
archive_db_path=""
archive_source_items=1

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
        --lane)
            lane_type="${2:-}"
            shift 2
            ;;
        --stale-days)
            stale_days="${2:-}"
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
        --group-max-items)
            group_max_items="${2:-}"
            shift 2
            ;;
        --neighbor-scan-max-edges)
            neighbor_scan_max_edges="${2:-}"
            shift 2
            ;;
        --neighbor-scan-max-nodes)
            neighbor_scan_max_nodes="${2:-}"
            shift 2
            ;;
        --external-protect-min)
            external_protect_min="${2:-}"
            shift 2
            ;;
        --link-rewire-topk)
            link_rewire_topk="${2:-}"
            shift 2
            ;;
        --id-proximity-max-gap)
            id_proximity_max_gap="${2:-}"
            shift 2
            ;;
        --similarity-max-id-gap)
            similarity_max_id_gap="${2:-}"
            shift 2
            ;;
        --title-overlap-min)
            title_overlap_min="${2:-}"
            shift 2
            ;;
        --title-jaccard-min-milli)
            title_jaccard_min_milli="${2:-}"
            shift 2
            ;;
        --phase-minor-gap-max)
            phase_minor_gap_max="${2:-}"
            shift 2
            ;;
        --phase-overlap-min)
            phase_overlap_min="${2:-}"
            shift 2
            ;;
        --apply-group)
            apply_group="${2:-}"
            shift 2
            ;;
        --apply-max-source-items)
            apply_max_source_items="${2:-}"
            shift 2
            ;;
        --apply-allow-singleton)
            apply_allow_singleton=1
            shift 1
            ;;
        --archive-db)
            archive_db_path="${2:-}"
            shift 2
            ;;
        --no-archive-source-items)
            archive_source_items=0
            shift 1
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
if [[ ! -x "${MEM_CLI}" ]]; then
    echo "mem_cli not found at ${MEM_CLI}" >&2
    echo "build it with: make -C shared/core/core_memdb all" >&2
    exit 1
fi

project_key="$(normalize_project_key "${project_key}")"
if [[ "${lane_type}" != "plan" ]]; then
    echo "--lane currently supports only 'plan' in v1 (got: ${lane_type})" >&2
    exit 1
fi

require_int_ge "--stale-days" "${stale_days}" 0
require_int_ge "--scan-limit" "${scan_limit}" 1
require_int_range "--page-size" "${page_size}" 1 1000
require_int_ge "--candidate-limit" "${candidate_limit}" 1
require_int_ge "--group-max-items" "${group_max_items}" 1
require_int_ge "--neighbor-scan-max-edges" "${neighbor_scan_max_edges}" 1
require_int_ge "--neighbor-scan-max-nodes" "${neighbor_scan_max_nodes}" 1
require_int_ge "--external-protect-min" "${external_protect_min}" 0
require_int_ge "--link-rewire-topk" "${link_rewire_topk}" 1
require_int_ge "--id-proximity-max-gap" "${id_proximity_max_gap}" 0
require_int_ge "--similarity-max-id-gap" "${similarity_max_id_gap}" 0
require_int_ge "--title-overlap-min" "${title_overlap_min}" 1
require_int_range "--title-jaccard-min-milli" "${title_jaccard_min_milli}" 0 1000
require_int_ge "--phase-minor-gap-max" "${phase_minor_gap_max}" 0
require_int_ge "--phase-overlap-min" "${phase_overlap_min}" 0
require_int_ge "--apply-group" "${apply_group}" 0
require_int_ge "--apply-max-source-items" "${apply_max_source_items}" 1
if [[ -n "${now_ns}" ]]; then
    require_int_ge "--now-ns" "${now_ns}" 1
fi
if [[ -n "${before_ns}" ]]; then
    require_int_ge "--before-ns" "${before_ns}" 1
fi

if [[ -z "${run_dir}" ]]; then
    run_dir="${ROOT_DIR}/docs/private_program_docs/memory_console/manual_rollup_runs/$(date +%Y-%m-%d_%H%M%S)"
fi
if [[ -n "${archive_db_path}" && "${archive_db_path}" == "${db_path}" ]]; then
    echo "--archive-db must be different from --db" >&2
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

scan_path="${run_dir}/all_items_scan.json"
candidate_set_path="${run_dir}/candidate_set.json"
candidate_summary_path="${run_dir}/candidate_summary.md"
preview_path="${run_dir}/plan_rollup_preview.md"
apply_plan_path="${run_dir}/plan_rollup_apply_plan.json"
report_path="${run_dir}/rollup_flow_report.md"
neighbors_path="${run_dir}/candidate_neighbors.json"
grouping_path="${run_dir}/graph_grouping.json"
similarity_edges_path="${run_dir}/similarity_edges.json"
apply_result_path="${run_dir}/apply_result.json"

tmp_all="$(mktemp)"
tmp_new="$(mktemp)"
trap 'rm -f "${tmp_all}" "${tmp_new}" "${tmp_neighbor_tsv:-}" "${tmp_neighbor_json:-}" "${tmp_graph_stats_json:-}" "${tmp_groups_json:-}" "${tmp_group_lines:-}" "${tmp_similarity_edges_json:-}" "${tmp_ranked:-}" "${tmp_apply_source_links_tsv:-}" "${tmp_apply_external_links_tsv:-}" "${tmp_apply_archived_source_tsv:-}" "${tmp_archive_db_add_tsv:-}" "${tmp_apply_sources_json:-}" "${tmp_apply_sources_new:-}"' EXIT
printf '[]\n' > "${tmp_all}"

offset=0
scanned=0
while (( scanned < scan_limit )); do
    remaining=$(( scan_limit - scanned ))
    chunk_limit="${page_size}"
    if (( remaining < chunk_limit )); then
        chunk_limit="${remaining}"
    fi
    if (( chunk_limit > 1000 )); then
        chunk_limit=1000
    fi

    chunk_json="$(${MEM_CLI} query --db "${db_path}" --workspace "${workspace_key}" --project "${project_key}" --kind plan --limit "${chunk_limit}" --offset "${offset}" --format json)"
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

pillars=(
    "scope-${project_key}"
    "plans-${project_key}"
    "decisions-${project_key}"
    "issues-${project_key}"
    "misc-${project_key}"
)
if [[ "${project_key}" == "mem_console" ]]; then
    pillars+=(
        "scope-memory_console"
        "plans-memory_console"
        "decisions-memory_console"
        "issues-memory_console"
        "misc-memory_console"
    )
fi

protected_stable_ids_json="$(printf '%s\n' "${pillars[@]}" | jq -Rsc 'split("\n") | map(select(length>0)) | unique')"

jq \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --arg lane_type "${lane_type}" \
    --arg generated_at_utc "${generated_at_utc}" \
    --argjson stale_days "${stale_days}" \
    --argjson before_ns "${before_ns}" \
    --argjson scan_limit "${scan_limit}" \
    --argjson page_size "${page_size}" \
    --argjson candidate_limit "${candidate_limit}" \
    --argjson protected_stable_ids "${protected_stable_ids_json}" \
    '
    def txt: (((.title // "") + "\n" + (.body // "")) | ascii_downcase);
    def stale($before): ((.updated_ns|tonumber) < $before);
    def protected($stable_ids): (((.stable_id // "") as $sid | (($stable_ids | index($sid)) != null)) or ((.pinned|tonumber)==1) or ((.canonical|tonumber)==1));
    def classify($before; $stable_ids):
        if protected($stable_ids) then
            {class:"keep", reason:"protected_or_pinned_or_canonical"}
        elif stale($before) and (txt | test("\\b(superseded|replaced|deprecated|obsolete|abandoned)\\b")) then
            {class:"superseded", reason:"stale_with_superseded_keyword"}
        elif stale($before) and (txt | test("\\b(complete|completed|done|closed|shipped|resolved)\\b")) and ((txt | test("\\b(active|in progress|todo|open)\\b")) | not) then
            {class:"archive", reason:"stale_with_completed_signal"}
        elif stale($before) then
            {class:"merge", reason:"stale_no_completion_or_superseded_signal"}
        else
            {class:"keep", reason:"recent_or_active"}
        end;

    . as $items
    | [ $items[]
        | (classify($before_ns; $protected_stable_ids)) as $c
        | {
            id,
            title: (.title // ""),
            updated_ns: (.updated_ns|tonumber),
            class: $c.class,
            reason: $c.reason
          }
      ] as $classified
    | ($classified | map(select(.reason == "protected_or_pinned_or_canonical") | .id) | unique) as $protected_item_ids
    | ($classified | map(select(.class != "keep")) | sort_by(.updated_ns) | .[:$candidate_limit]) as $picked
    | ($picked | map(.id)) as $picked_ids
    | ($picked | map(select(.class=="merge") | .id)) as $merge_ids
    | ($picked | map(select(.class=="archive") | .id)) as $archive_ids
    | ($picked | map(select(.class=="superseded") | .id)) as $superseded_ids
    | {
        workspace: $workspace,
        project: $project,
        lane_type: $lane_type,
        generated_at_utc: $generated_at_utc,
        filters: {
            stale_days: $stale_days,
            before_ns: $before_ns,
            scan_limit: $scan_limit,
            page_size: $page_size,
            candidate_limit: $candidate_limit
        },
        counts: {
            scanned: ($items | length),
            keep: (($items | map(.id)) - $picked_ids | length),
            merge: ($merge_ids | length),
            archive: ($archive_ids | length),
            superseded: ($superseded_ids | length),
            protected: ($protected_item_ids | length)
        },
        keep_ids: (($items | map(.id)) - $picked_ids),
        merge_ids: $merge_ids,
        archive_ids: $archive_ids,
        superseded_ids: $superseded_ids,
        protected_ids: $protected_item_ids,
        candidate_rows: ($picked | map({id, class, reason, updated_ns, title}))
      }
    ' "${scan_path}" > "${candidate_set_path}"

jq -n -r \
    --slurpfile cand "${candidate_set_path}" \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    '
    ($cand[0]) as $c
    | [
        "# Candidate Summary",
        "",
        "- generated_at_utc: " + $generated_at_utc,
        "- db_path: " + $db_path,
        "- workspace/project: " + $workspace + "/" + $project,
        "- lane_type: " + $c.lane_type,
        "- scanned: " + ($c.counts.scanned|tostring),
        "- keep: " + ($c.counts.keep|tostring),
        "- merge: " + ($c.counts.merge|tostring),
        "- archive: " + ($c.counts.archive|tostring),
        "- superseded: " + ($c.counts.superseded|tostring),
        "- protected: " + ($c.counts.protected|tostring),
        "",
        "## Candidate Examples",
        (
            if (($c.candidate_rows // []) | length) == 0 then
                "- none"
            else
                (($c.candidate_rows // [])
                 | .[:20]
                 | map("- id=" + (.id|tostring) + " class=" + .class + " reason=" + .reason + " title=" + .title)
                 | join("\n"))
            end
        )
      ] | join("\n")
    ' > "${candidate_summary_path}"

tmp_neighbor_tsv="$(mktemp)"
tmp_neighbor_json="$(mktemp)"
tmp_graph_stats_json="$(mktemp)"
tmp_groups_json="$(mktemp)"
tmp_group_lines="$(mktemp)"
tmp_similarity_edges_json="$(mktemp)"
: > "${tmp_neighbor_tsv}"
printf '[]\n' > "${tmp_neighbor_json}"
printf '{}\n' > "${tmp_graph_stats_json}"
printf '[]\n' > "${tmp_groups_json}"
: > "${tmp_group_lines}"
printf '[]\n' > "${tmp_similarity_edges_json}"

mapfile -t candidate_ids < <(jq -r '.candidate_rows[]?.id' "${candidate_set_path}")
if (( ${#candidate_ids[@]} > 0 )); then
    declare -A candidate_id_set=()
    declare -A candidate_link_counts=()
    declare -A external_link_counts=()
    for cid in "${candidate_ids[@]}"; do
        candidate_id_set["$cid"]=1
    done

    for source_id in "${candidate_ids[@]}"; do
        neighbors_json="$("${MEM_CLI}" neighbors --db "${db_path}" --item-id "${source_id}" --max-edges "${neighbor_scan_max_edges}" --max-nodes "${neighbor_scan_max_nodes}" --format json)"
        while IFS=$'\t' read -r neighbor_id link_kind neighbor_title; do
            if [[ -z "${neighbor_id}" || "${neighbor_id}" == "null" ]]; then
                continue
            fi
            printf '%s\t%s\t%s\t%s\n' "${source_id}" "${neighbor_id}" "${link_kind}" "${neighbor_title}" >> "${tmp_neighbor_tsv}"
            if [[ "${neighbor_id}" != "${source_id}" && -n "${candidate_id_set[${neighbor_id}]:-}" ]]; then
                candidate_link_counts["${source_id}"]=$(( ${candidate_link_counts["${source_id}"]:-0} + 1 ))
            else
                external_link_counts["${source_id}"]=$(( ${external_link_counts["${source_id}"]:-0} + 1 ))
            fi
        done < <(printf '%s\n' "${neighbors_json}" | jq -r '.[]? | [(.neighbor_id // empty), (.kind // "related"), (.neighbor_title // "")] | @tsv')
    done

    jq -Rsc '
        split("\n")
        | map(select(length > 0) | split("\t"))
        | map({
            source_id: (.[0] | tonumber),
            neighbor_id: (.[1] | tonumber),
            kind: (.[2] // "related"),
            neighbor_title: (.[3] // "")
          })
    ' "${tmp_neighbor_tsv}" > "${tmp_neighbor_json}"
    cp "${tmp_neighbor_json}" "${neighbors_path}"

    : > "${tmp_graph_stats_json}"
    for cid in "${candidate_ids[@]}"; do
        printf '{"id":%s,"candidate_links":%s,"external_links":%s}\n' \
            "${cid}" \
            "${candidate_link_counts["${cid}"]:-0}" \
            "${external_link_counts["${cid}"]:-0}" >> "${tmp_graph_stats_json}"
    done
    graph_stats_map="$(jq -sc 'map({key:(.id|tostring), value:{candidate_links:.candidate_links, external_links:.external_links}}) | from_entries' "${tmp_graph_stats_json}")"

    jq \
        --argjson graph_stats "${graph_stats_map}" \
        --argjson external_protect_min "${external_protect_min}" \
        '
        (.candidate_rows // []) as $rows
        | ($rows | map(
            . as $r
            | ($graph_stats[($r.id|tostring)] // {candidate_links:0, external_links:0}) as $gs
            | . + { graph_stats: $gs }
            | if ($gs.external_links >= $external_protect_min) then
                .class = "keep" | .reason = "graph_external_link_pressure"
              elif (.class == "archive" and $gs.external_links > 0) then
                .class = "merge" | .reason = "archive_downgraded_due_external_links"
              else
                .
              end
          )) as $rows2
        | ($rows2 | map(select(.class != "keep"))) as $picked
        | ($picked | map(.id)) as $picked_ids
        | .merge_ids = ($picked | map(select(.class == "merge") | .id))
        | .archive_ids = ($picked | map(select(.class == "archive") | .id))
        | .superseded_ids = ($picked | map(select(.class == "superseded") | .id))
        | .keep_ids = ((.keep_ids // []) + ($rows2 | map(select(.class == "keep") | .id)) | unique)
        | .candidate_rows = $picked
        | .counts.keep = (.keep_ids | length)
        | .counts.merge = (.merge_ids | length)
        | .counts.archive = (.archive_ids | length)
        | .counts.superseded = (.superseded_ids | length)
        | .graph = {
            external_protect_min: $external_protect_min,
            neighbor_scan_max_edges: '"${neighbor_scan_max_edges}"',
            neighbor_scan_max_nodes: '"${neighbor_scan_max_nodes}"',
            id_proximity_max_gap: '"${id_proximity_max_gap}"',
            similarity_max_id_gap: '"${similarity_max_id_gap}"',
            title_overlap_min: '"${title_overlap_min}"',
            title_jaccard_min_milli: '"${title_jaccard_min_milli}"',
            phase_minor_gap_max: '"${phase_minor_gap_max}"',
            phase_overlap_min: '"${phase_overlap_min}"'
          }
        ' "${candidate_set_path}" > "${tmp_new}"
    mv "${tmp_new}" "${candidate_set_path}"

    mapfile -t final_candidate_ids < <(jq -r '.candidate_rows[]?.id' "${candidate_set_path}")
    if (( ${#final_candidate_ids[@]} > 0 )); then
        declare -A final_candidate_set=()
        declare -A rank_by_id=()
        declare -A visited=()
        declare -A ADJ_MAP=()
        declare -A protected_id_set=()
        declare -A shared_sources_by_neighbor=()
        shared_neighbor_bridge_max_sources=8
        for idx in "${!final_candidate_ids[@]}"; do
            cid="${final_candidate_ids[$idx]}"
            final_candidate_set["$cid"]=1
            rank_by_id["$cid"]="$idx"
        done
        while IFS= read -r pid; do
            if [[ -n "${pid}" ]]; then
                protected_id_set["$pid"]=1
            fi
        done < <(jq -r '.protected_ids[]?' "${candidate_set_path}")

        while IFS=$'\t' read -r source_id neighbor_id _kind _title; do
            if [[ -z "${source_id}" || -z "${neighbor_id}" ]]; then
                continue
            fi
            if [[ "${source_id}" == "${neighbor_id}" ]]; then
                continue
            fi
            if [[ -n "${final_candidate_set[${source_id}]:-}" && -n "${final_candidate_set[${neighbor_id}]:-}" ]]; then
                add_adj "${source_id}" "${neighbor_id}"
                add_adj "${neighbor_id}" "${source_id}"
                continue
            fi
            if [[ -n "${final_candidate_set[${source_id}]:-}" && -z "${final_candidate_set[${neighbor_id}]:-}" && -z "${protected_id_set[${neighbor_id}]:-}" ]]; then
                src_csv="${shared_sources_by_neighbor[${neighbor_id}]:-}"
                if [[ -z "${src_csv}" ]]; then
                    shared_sources_by_neighbor["${neighbor_id}"]="${source_id}"
                elif [[ ",${src_csv}," != *",${source_id},"* ]]; then
                    shared_sources_by_neighbor["${neighbor_id}"]="${src_csv},${source_id}"
                fi
            fi
        done < "${tmp_neighbor_tsv}"

        for neighbor_id in "${!shared_sources_by_neighbor[@]}"; do
            src_csv="${shared_sources_by_neighbor[${neighbor_id}]}"
            IFS=',' read -r -a src_arr <<< "${src_csv}"
            src_len=${#src_arr[@]}
            if (( src_len < 2 || src_len > shared_neighbor_bridge_max_sources )); then
                continue
            fi
            for ((i=0; i<src_len; i++)); do
                for ((j=i+1; j<src_len; j++)); do
                    left="${src_arr[$i]}"
                    right="${src_arr[$j]}"
                    if [[ -n "${final_candidate_set[${left}]:-}" && -n "${final_candidate_set[${right}]:-}" && "${left}" != "${right}" ]]; then
                        add_adj "${left}" "${right}"
                        add_adj "${right}" "${left}"
                    fi
                done
            done
        done

        jq -n \
            --slurpfile cand "${candidate_set_path}" \
            --argjson id_proximity_max_gap "${id_proximity_max_gap}" \
            --argjson similarity_max_id_gap "${similarity_max_id_gap}" \
            --argjson title_overlap_min "${title_overlap_min}" \
            --argjson title_jaccard_min_milli "${title_jaccard_min_milli}" \
            --argjson phase_minor_gap_max "${phase_minor_gap_max}" \
            --argjson phase_overlap_min "${phase_overlap_min}" \
            '
            def absn: if . < 0 then -. else . end;
            def clean:
                ascii_downcase
                | gsub("[^a-z0-9. ]"; " ")
                | gsub("\\s+"; " ")
                | ltrimstr(" ")
                | rtrimstr(" ");
            def stopwords:
                ["plan","phase","step","part","slice","complete","completed","created","doc","docs","execution","rollup","summary","and","the","for","with","from","into","after","before","shared","ide","mem","console","memory","project","program","lane","task","work"];
            def phase_stopwords:
                ["and","the","for","with","from","into","after","before","created","plan","doc","docs","summary","rollup"];
            def tokens($t):
                ($t
                 | clean
                 | gsub("\\b(phase|step|part|slice)\\s*[0-9]+(\\.[0-9]+)?\\b"; " ")
                 | split(" ")
                 | map(select(length >= 3))
                 | map(. as $tok | select((stopwords | index($tok)) == null))
                 | unique);
            def phase_tokens($t):
                ($t
                 | clean
                 | gsub("\\b(phase|step|part|slice)\\s*[0-9]+(\\.[0-9]+)?\\b"; " ")
                 | split(" ")
                 | map(select(length >= 3))
                 | map(. as $tok | select((phase_stopwords | index($tok)) == null))
                 | unique);
            def phase($t):
                (($t | ascii_downcase | capture("(?<k>phase|step|part|slice)\\s*(?<maj>[0-9]+)(?:\\.(?<min>[0-9]+))?")?) // null);

            ($cand[0].candidate_rows // []) as $rows
            | [
                range(0; ($rows | length)) as $i
                | range(($i + 1); ($rows | length)) as $j
                | ($rows[$i]) as $a
                | ($rows[$j]) as $b
                | (($a.id - $b.id) | absn) as $id_gap
                | (tokens($a.title // "")) as $ta
                | (tokens($b.title // "")) as $tb
                | (($ta + $tb) | unique | length) as $union_len
                | (($ta + $tb) | group_by(.) | map(select(length > 1) | .[0]) | length) as $overlap
                | (phase_tokens($a.title // "")) as $pta
                | (phase_tokens($b.title // "")) as $ptb
                | (($pta + $ptb) | group_by(.) | map(select(length > 1) | .[0]) | length) as $phase_overlap
                | (if $union_len == 0 then 0 else ($overlap / $union_len) end) as $jaccard
                | (phase($a.title // "")) as $pa
                | (phase($b.title // "")) as $pb
                | ($pa != null and $pb != null and ($pa.k == $pb.k)) as $phase_kind_match
                | (if $phase_kind_match then (((($pa.maj | tonumber) - ($pb.maj | tonumber)) | absn)) else 99999 end) as $phase_major_gap
                | ($phase_kind_match and ($pa.maj == $pb.maj)) as $phase_root_match
                | ($phase_root_match and ($pa.min != null) and ($pb.min != null) and (((($pa.min | tonumber) - ($pb.min | tonumber)) | absn) <= $phase_minor_gap_max)) as $phase_minor_adjacent
                | ($phase_kind_match and (($phase_major_gap <= 1) or $phase_minor_adjacent)) as $phase_adjacent
                | ([
                    (if ($id_gap <= $id_proximity_max_gap and $overlap >= 1) then "id_proximity_token" else empty end),
                    (if ($overlap >= $title_overlap_min and (($jaccard * 1000) >= $title_jaccard_min_milli) and $id_gap <= $similarity_max_id_gap) then "title_similarity" else empty end),
                    (if ($phase_adjacent and $id_gap <= $similarity_max_id_gap and $phase_overlap >= $phase_overlap_min) then "phase_sequence" else empty end),
                    (if ($phase_root_match and $phase_overlap >= $phase_overlap_min and $id_gap <= $similarity_max_id_gap) then "phase_root_similarity" else empty end)
                  ] | unique) as $reasons
                | if (($reasons | length) == 0) then
                    empty
                  elif (($reasons | index("phase_sequence")) != null
                        or ($reasons | index("title_similarity")) != null
                        or $overlap >= 2) then
                    {
                        left_id: $a.id,
                        right_id: $b.id,
                        id_gap: $id_gap,
                        token_overlap: $overlap,
                        phase_token_overlap: $phase_overlap,
                        jaccard: (($jaccard * 1000 | floor) / 1000),
                        phase_root_match: $phase_root_match,
                        phase_adjacent: $phase_adjacent,
                        reasons: $reasons
                    }
                  else
                    empty
                  end
              ]
            | sort_by(.id_gap, -.token_overlap, -.jaccard, .left_id, .right_id)
            ' > "${tmp_similarity_edges_json}"
        cp "${tmp_similarity_edges_json}" "${similarity_edges_path}"

        while IFS=$'\t' read -r left_id right_id; do
            if [[ -z "${left_id}" || -z "${right_id}" || "${left_id}" == "${right_id}" ]]; then
                continue
            fi
            if [[ -n "${final_candidate_set[${left_id}]:-}" && -n "${final_candidate_set[${right_id}]:-}" ]]; then
                add_adj "${left_id}" "${right_id}"
                add_adj "${right_id}" "${left_id}"
            fi
        done < <(jq -r '.[]? | [(.left_id|tostring), (.right_id|tostring)] | @tsv' "${tmp_similarity_edges_json}")

        for seed_id in "${final_candidate_ids[@]}"; do
            if [[ -n "${visited[${seed_id}]:-}" ]]; then
                continue
            fi
            queue=( "${seed_id}" )
            visited["${seed_id}"]=1
            component_ids=()
            while (( ${#queue[@]} > 0 )); do
                current_id="${queue[0]}"
                queue=( "${queue[@]:1}" )
                component_ids+=( "${current_id}" )
                neighbor_csv="${ADJ_MAP[${current_id}]:-}"
                if [[ -z "${neighbor_csv}" ]]; then
                    continue
                fi
                IFS=',' read -r -a neighbor_arr <<< "${neighbor_csv}"
                for next_id in "${neighbor_arr[@]}"; do
                    if [[ -z "${next_id}" || -n "${visited[${next_id}]:-}" ]]; then
                        continue
                    fi
                    visited["${next_id}"]=1
                    queue+=( "${next_id}" )
                done
            done

            tmp_ranked="$(mktemp)"
            for cid in "${component_ids[@]}"; do
                printf '%s\t%s\n' "${rank_by_id[${cid}]:-999999}" "${cid}" >> "${tmp_ranked}"
            done
            mapfile -t component_sorted < <(sort -n "${tmp_ranked}" | cut -f2)

            comp_len=${#component_sorted[@]}
            comp_idx=0
            while (( comp_idx < comp_len )); do
                chunk=( "${component_sorted[@]:comp_idx:group_max_items}" )
                printf '%s\n' "$(IFS=','; echo "${chunk[*]}")" >> "${tmp_group_lines}"
                comp_idx=$(( comp_idx + group_max_items ))
            done
        done

        jq -Rsc 'split("\n") | map(select(length > 0) | split(",") | map(tonumber))' "${tmp_group_lines}" > "${tmp_groups_json}"
    fi
else
    printf '[]\n' > "${neighbors_path}"
    printf '[]\n' > "${similarity_edges_path}"
fi
[ -f "${similarity_edges_path}" ] || cp "${tmp_similarity_edges_json}" "${similarity_edges_path}"
cp "${tmp_groups_json}" "${grouping_path}"

# Refresh summary after graph-aware reclassification.
jq -n -r \
    --slurpfile cand "${candidate_set_path}" \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg db_path "${db_path}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    '
    ($cand[0]) as $c
    | [
        "# Candidate Summary",
        "",
        "- generated_at_utc: " + $generated_at_utc,
        "- db_path: " + $db_path,
        "- workspace/project: " + $workspace + "/" + $project,
        "- lane_type: " + $c.lane_type,
        "- scanned: " + ($c.counts.scanned|tostring),
        "- keep: " + ($c.counts.keep|tostring),
        "- merge: " + ($c.counts.merge|tostring),
        "- archive: " + ($c.counts.archive|tostring),
        "- superseded: " + ($c.counts.superseded|tostring),
        "- protected: " + ($c.counts.protected|tostring),
        "",
        "## Candidate Examples",
        (
            if (($c.candidate_rows // []) | length) == 0 then
                "- none"
            else
                (($c.candidate_rows // [])
                 | .[:20]
                 | map("- id=" + (.id|tostring) + " class=" + .class + " reason=" + .reason + " title=" + .title)
                 | join("\n"))
            end
        )
      ] | join("\n")
    ' > "${candidate_summary_path}"

jq -n \
    --slurpfile cand "${candidate_set_path}" \
    --slurpfile scan "${scan_path}" \
    --slurpfile groups "${tmp_groups_json}" \
    --slurpfile neighbors "${tmp_neighbor_json}" \
    --slurpfile similarity "${tmp_similarity_edges_json}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --arg generated_at_utc "${generated_at_utc}" \
    --argjson link_rewire_topk "${link_rewire_topk}" \
    '
    def capword:
        if length == 0 then . else ((.[0:1] | ascii_upcase) + .[1:]) end;
    def clean_title:
        gsub("\\s*\\([^)]*\\)"; "")
        | gsub("(?i)\\b(step|part|slice|phase)\\s*[0-9.]+\\b"; "")
        | gsub("[^A-Za-z0-9 ]"; " ")
        | gsub("\\s+"; " ")
        | ltrimstr(" ")
        | rtrimstr(" ");
    def stopwords:
        ["plan","phase","step","part","slice","complete","completed","created","doc","docs","execution","rollup","summary","and","the","for","with","from","into","after","before","shared","ide","mem","console","memory","project","program","lane","created"];
    def semantic_tokens($titles):
        ($titles
         | map(ascii_downcase | clean_title | split(" "))
         | add
         | map(select(length >= 3))
         | map(. as $tok | select((stopwords | index($tok)) == null)));
    def semantic_title($titles):
        (semantic_tokens($titles)) as $tokens
        | if ($tokens | length) == 0 then
            ""
          else
            ($tokens
             | group_by(.)
             | map({w: .[0], c: length})
             | sort_by(-.c, .w)
             | .[:4]
             | map(.w | capword)
             | join(" "))
          end;

    ($cand[0]) as $c
    | ($scan[0]) as $items
    | ($groups[0]) as $groups_arr
    | ($neighbors[0]) as $neighbors_arr
    | ($similarity[0]) as $similarity_edges
    | (($c.candidate_rows // []) | map(.id)) as $all_candidate_ids
    | (reduce $items[] as $it ({}; .[($it.id|tostring)] = $it)) as $by_id
    | ($groups_arr
       | to_entries
       | map(
            .key as $gidx
            | .value as $ids
            | [ $ids[] | ($by_id[(.|tostring)].title // "") ] as $titles
            | (semantic_title($titles)) as $semantic
            | (($titles[0] // "Plan Thread") | clean_title) as $seed
            | ($neighbors_arr
               | map(select((.source_id as $sid | ($ids | index($sid)) != null)
                            and (.neighbor_id as $nid | (($all_candidate_ids | index($nid)) == null))))) as $external_rows
            | ($neighbors_arr
               | map(select((.source_id as $sid | ($ids | index($sid)) != null)
                            and ((.neighbor_id as $nid | ($ids | index($nid)) != null)))) | length) as $internal_link_hits
            | ($similarity_edges
               | map(select((.left_id as $left | ($ids | index($left)) != null)
                            and (.right_id as $right | ($ids | index($right)) != null)))) as $group_similarity_edges
            | ($external_rows | length) as $external_link_hits
            | ($external_rows
               | group_by([.neighbor_id, .kind])
               | map({
                    to_item_id: .[0].neighbor_id,
                    kind: (.[0].kind // "related"),
                    evidence_count: length,
                    neighbor_title: (.[0].neighbor_title // ""),
                    reason: "observed source->external link during candidate scan"
                 })
               | sort_by(-.evidence_count, .to_item_id)
               | .[:$link_rewire_topk]) as $proposed_links
            | {
                title: (
                    if ($semantic | length) > 0 then
                        "Plan Rollup: " + $semantic
                    elif ($seed | length) > 0 then
                        "Plan Rollup: " + (($seed[0:1] | ascii_upcase) + $seed[1:])
                    else
                        "Plan Rollup Group " + (($gidx + 1) | tostring)
                    end
                ),
                source_item_ids: $ids,
                source_title_examples: ($titles | map(select(length > 0)) | .[:5]),
                protected_item_ids: ($c.protected_ids // []),
                scope: {
                    workspace: $workspace,
                    project: $project,
                    kind: "plan"
                },
                graph_context: {
                    component_size: ($ids | length),
                    internal_link_hits: $internal_link_hits,
                    external_link_hits: $external_link_hits,
                    fallback_similarity_edges: ($group_similarity_edges | length),
                    fallback_similarity_reasons: (
                        ($group_similarity_edges | map(.reasons[]?) | group_by(.) | map({reason: .[0], count: length}))
                    )
                },
                link_rewire_suggestions: $proposed_links,
                notes: [
                    "Read-only planning artifact; apply disabled.",
                    "Source set built from graph-aware candidate classification and grouping.",
                    "Fallback grouping may include ID/title/phase similarity edges."
                ]
            }
         )) as $rollups
    | {
        workspace: $workspace,
        project: $project,
        lane_type: "plan",
        generated_at_utc: $generated_at_utc,
        source: "mem_rollup_flow_manual.sh",
        proposed_rollups: $rollups,
        link_rewire_plan: {
            enabled: (($rollups | map(.link_rewire_suggestions | length) | add) > 0),
            topk_per_group: $link_rewire_topk,
            groups: (
                $rollups
                | to_entries
                | map({
                    group_index: (.key + 1),
                    source_item_ids: .value.source_item_ids,
                    proposed_links: .value.link_rewire_suggestions
                  })
            )
        },
        budgets: {
            max_rollups: ($rollups | length),
            max_nodes_affected: (($rollups | map(.source_item_ids | length) | add) // 0),
            max_links_added: (($rollups | map(.link_rewire_suggestions | length) | add) // 0),
            max_links_updated: 0
        },
        apply_enabled: false
      }
    ' > "${apply_plan_path}"

jq -n -r \
    --slurpfile plan "${apply_plan_path}" \
    --slurpfile scan "${scan_path}" \
    '
    ($plan[0]) as $p
    | (reduce ($scan[0][]) as $it ({}; .[(($it.id|tostring))] = $it.title)) as $titles
    | [
        "# Plan Rollup Preview",
        "",
        "- proposed_rollups: " + (($p.proposed_rollups|length)|tostring),
        "- apply_enabled: " + ($p.apply_enabled|tostring),
        "- link_rewire_enabled: " + ($p.link_rewire_plan.enabled|tostring),
        ""
      ]
      + (
          $p.proposed_rollups
          | to_entries
          | map(
              "## Group " + ((.key+1)|tostring) + "\n"
              + "- title: " + .value.title + "\n"
              + "- source_count: " + ((.value.source_item_ids|length)|tostring) + "\n"
              + "- graph: internal_link_hits=" + ((.value.graph_context.internal_link_hits|tostring) // "0")
              + " external_link_hits=" + ((.value.graph_context.external_link_hits|tostring) // "0")
              + " fallback_similarity_edges=" + ((.value.graph_context.fallback_similarity_edges|tostring) // "0") + "\n"
              + (
                    if ((.value.graph_context.fallback_similarity_reasons // []) | length) == 0 then
                        "- fallback_similarity_reasons: none\n"
                    else
                        "- fallback_similarity_reasons: "
                        + (
                            (.value.graph_context.fallback_similarity_reasons // [])
                            | map(.reason + "=" + (.count|tostring))
                            | join(", ")
                          )
                        + "\n"
                    end
                )
              + (
                    .value.source_item_ids
                    | map("- source_id=" + (tostring) + " title=" + ($titles[(tostring)] // ""))
                    | join("\n")
                )
              + (
                    if ((.value.link_rewire_suggestions // []) | length) == 0 then
                        "\n- suggested_links: none"
                    else
                        "\n- suggested_links:\n"
                        + (
                            (.value.link_rewire_suggestions // [])
                            | map("  - to=" + (.to_item_id|tostring) + " kind=" + .kind + " evidence_count=" + (.evidence_count|tostring) + " title=" + (.neighbor_title // ""))
                            | join("\n")
                          )
                    end
                )
            )
        )
      | join("\n\n")
    ' > "${preview_path}"

apply_attempted=false
apply_succeeded=false
mode_label="read-only dry run"
selected_rollup_json='null'
selected_rollup_title=""
selected_source_count=0
applied_rollup_id=0
added_source_links=0
added_external_links=0
archived_source_items=0
archive_db_added_items=0
apply_error=""
pre_health_json='null'
post_health_json='null'
post_neighbors_json='[]'
tmp_apply_source_links_tsv="$(mktemp)"
tmp_apply_external_links_tsv="$(mktemp)"
tmp_apply_archived_source_tsv="$(mktemp)"
tmp_archive_db_add_tsv="$(mktemp)"
: > "${tmp_apply_source_links_tsv}"
: > "${tmp_apply_external_links_tsv}"
: > "${tmp_apply_archived_source_tsv}"
: > "${tmp_archive_db_add_tsv}"

if (( apply_group > 0 )); then
    apply_attempted=true
    mode_label="constrained apply (single group)"
    total_rollup_groups="$(jq '.proposed_rollups | length' "${apply_plan_path}")"
    if (( apply_group > total_rollup_groups )); then
        echo "--apply-group ${apply_group} is out of range (groups=${total_rollup_groups})" >&2
        exit 1
    fi

    selected_rollup_json="$(jq --argjson idx "$((apply_group - 1))" '.proposed_rollups[$idx]' "${apply_plan_path}")"
    selected_rollup_title="$(printf '%s\n' "${selected_rollup_json}" | jq -r '.title // ""')"
    selected_source_count="$(printf '%s\n' "${selected_rollup_json}" | jq '.source_item_ids | length')"
    selected_source_ids_json="$(printf '%s\n' "${selected_rollup_json}" | jq '.source_item_ids')"
    selected_rewire_count="$(printf '%s\n' "${selected_rollup_json}" | jq '(.link_rewire_suggestions // []) | length')"
    if (( selected_source_count == 0 )); then
        echo "selected group has zero source items; refusing apply" >&2
        exit 1
    fi
    if (( selected_source_count > apply_max_source_items )); then
        echo "selected group source_count=${selected_source_count} exceeds --apply-max-source-items=${apply_max_source_items}" >&2
        exit 1
    fi
    if (( selected_source_count == 1 && apply_allow_singleton == 0 )); then
        echo "selected group is singleton; pass --apply-allow-singleton to permit this constrained apply" >&2
        exit 1
    fi

    tmp_apply_sources_json="$(mktemp)"
    tmp_apply_sources_new="$(mktemp)"
    printf '[]\n' > "${tmp_apply_sources_json}"
    while IFS= read -r source_id; do
        if [[ -z "${source_id}" ]]; then
            continue
        fi
        source_show_json="$("${MEM_CLI}" show --db "${db_path}" --id "${source_id}" --format json)"
        source_row_json="$(printf '%s\n' "${source_show_json}" | jq '{
            id: (.id | tonumber),
            title: (.title // ""),
            body: (.body // ""),
            stable_id: (.stable_id // ""),
            workspace_key: (.workspace_key // ""),
            project_key: (.project_key // ""),
            kind: (.kind // ""),
            created_ns: (.created_ns | tonumber),
            updated_ns: (.updated_ns | tonumber)
        }')"
        jq -n --slurpfile arr "${tmp_apply_sources_json}" --argjson row "${source_row_json}" '$arr[0] + [$row]' > "${tmp_apply_sources_new}"
        mv "${tmp_apply_sources_new}" "${tmp_apply_sources_json}"
    done < <(printf '%s\n' "${selected_source_ids_json}" | jq -r '.[]')
    selected_source_details_json="$(jq 'sort_by(.updated_ns, .id)' "${tmp_apply_sources_json}")"

    pre_health_json="$("${MEM_CLI}" health --db "${db_path}" --format json)"

    apply_stable_id="${project_key}-plan-rollup-g${apply_group}-$(date +%Y%m%d%H%M%S)"
    apply_body="$(jq -n -r \
        --arg generated_at_utc "${generated_at_utc}" \
        --arg run_dir "${run_dir}" \
        --arg title "${selected_rollup_title}" \
        --argjson source_ids "${selected_source_ids_json}" \
        --argjson source_details "${selected_source_details_json}" \
        --argjson rewire_count "${selected_rewire_count}" \
        --arg archive_source_items "$(if (( archive_source_items == 1 )); then echo "true"; else echo "false"; fi)" \
        --arg archive_db_path "${archive_db_path}" \
        '
        def clean:
            gsub("[\\r\\n\\t]+"; " ")
            | gsub("\\s+"; " ")
            | ltrimstr(" ")
            | rtrimstr(" ");
        def clip($n):
            if (length > $n) then .[0:$n] + "..." else . end;
        def stopwords:
            ["plan","phase","step","part","slice","complete","completed","created","doc","docs","execution","rollup","summary","and","the","for","with","from","into","after","before","shared","ide","mem","console","memory","project","program","lane","task","work","added","updated","created","done","closed"];
        def theme_tokens($rows):
            ($rows
             | map((((.title // "") + " " + (.body // "")) | ascii_downcase | gsub("[^a-z0-9 ]"; " ") | gsub("\\s+"; " ") | split(" ")))
             | add
             | map(select(length >= 4))
             | map(. as $tok | select((stopwords | index($tok)) == null)));
        def theme_phrase($rows):
            (theme_tokens($rows)
             | group_by(.)
             | map({w: .[0], c: length})
             | sort_by(-.c, .w)
             | .[:6]
             | map(.w)) as $top
            | if ($top | length) == 0 then "mixed planning and implementation updates"
              else ($top | join(", "))
              end;
        def source_line($row):
            (($row.body // "") | clean) as $sent
            | ($row.title // "" | clean | clip(110)) as $t
            | ($sent | clip(260)) as $b
            | "- [" + ($row.id|tostring) + "] " + $t + ": " + (if ($b|length)==0 then "No body content." else $b end);

        [
            "Rollup Summary",
            "- Consolidates " + (($source_details | length) | tostring) + " source nodes into one rollup node.",
            "- Primary themes: " + theme_phrase($source_details),
            "- Source ids: " + ($source_ids | map(tostring) | join(", ")),
            "- Rewired external links applied: " + ($rewire_count | tostring),
            "",
            "Source Node Summaries",
            (
                if ($source_details | length) == 0 then
                    "- none"
                else
                    ($source_details | map(source_line(.)) | join("\n"))
                end
            ),
            "",
            "Apply Metadata",
            "- generated_at_utc: " + $generated_at_utc,
            "- run_dir: " + $run_dir,
            "- mode: constrained_apply_single_group",
            "- rollup_title: " + $title,
            "",
            "Operational Notes",
            "- Constrained single-group apply with source retention policy controls.",
            "- Source nodes archived in active DB: " + (if $archive_source_items == "true" then "yes" else "no" end),
            "- Archive DB copy path: " + (if ($archive_db_path|length) > 0 then $archive_db_path else "(disabled)" end)
        ] | join("\n")
        ')"

    add_out="$("${MEM_CLI}" add \
        --db "${db_path}" \
        --workspace "${workspace_key}" \
        --project "${project_key}" \
        --kind rollup \
        --stable-id "${apply_stable_id}" \
        --title "${selected_rollup_title}" \
        --body "${apply_body}")"
    applied_rollup_id="$(printf '%s\n' "${add_out}" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p' | tail -n1)"
    if [[ -z "${applied_rollup_id}" ]]; then
        echo "failed to parse applied rollup id from add output: ${add_out}" >&2
        exit 1
    fi

    while IFS= read -r source_id; do
        if [[ -z "${source_id}" ]]; then
            continue
        fi
        link_out="$("${MEM_CLI}" link-add \
            --db "${db_path}" \
            --from "${applied_rollup_id}" \
            --to "${source_id}" \
            --kind summarizes \
            --note "constrained apply group ${apply_group} source coverage")"
        link_id="$(printf '%s\n' "${link_out}" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p' | tail -n1)"
        printf '%s\t%s\t%s\t%s\n' "${link_id:-0}" "${source_id}" "summarizes" "source" >> "${tmp_apply_source_links_tsv}"
        added_source_links=$(( added_source_links + 1 ))
    done < <(printf '%s\n' "${selected_rollup_json}" | jq -r '.source_item_ids[]')

    while IFS=$'\t' read -r to_item_id raw_kind neighbor_title evidence_count; do
        if [[ -z "${to_item_id}" ]]; then
            continue
        fi
        if [[ "${to_item_id}" == "${applied_rollup_id}" ]]; then
            continue
        fi
        link_kind="$(normalize_link_kind "${raw_kind}")"
        link_out="$("${MEM_CLI}" link-add \
            --db "${db_path}" \
            --from "${applied_rollup_id}" \
            --to "${to_item_id}" \
            --kind "${link_kind}" \
            --note "constrained apply group ${apply_group} rewired evidence_count=${evidence_count}")"
        link_id="$(printf '%s\n' "${link_out}" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p' | tail -n1)"
        printf '%s\t%s\t%s\t%s\t%s\n' "${link_id:-0}" "${to_item_id}" "${link_kind}" "${evidence_count}" "${neighbor_title}" >> "${tmp_apply_external_links_tsv}"
        added_external_links=$(( added_external_links + 1 ))
    done < <(printf '%s\n' "${selected_rollup_json}" | jq -r '.link_rewire_suggestions[]? | [(.to_item_id|tostring), (.kind // "related"), (.neighbor_title // ""), (.evidence_count|tostring)] | @tsv')

    if (( archive_source_items == 1 )); then
        while IFS= read -r source_id; do
            source_row_json="$(printf '%s\n' "${selected_source_details_json}" | jq --argjson sid "${source_id}" -c '.[] | select(.id == $sid)')"
            if [[ -z "${source_row_json}" ]]; then
                continue
            fi

            if [[ -n "${archive_db_path}" ]]; then
                archive_title="$(printf '%s\n' "${source_row_json}" | jq -r '.title // ""')"
                archive_workspace="$(printf '%s\n' "${source_row_json}" | jq -r '.workspace_key // ""')"
                archive_project="$(printf '%s\n' "${source_row_json}" | jq -r '.project_key // ""')"
                archive_kind="$(printf '%s\n' "${source_row_json}" | jq -r '.kind // ""')"
                archive_body="$(printf '%s\n' "${source_row_json}" | jq -r \
                    --arg source_db_path "${db_path}" \
                    --arg generated_at_utc "${generated_at_utc}" \
                    --argjson rollup_item_id "${applied_rollup_id}" \
                    '
                    [
                      "Archived Source Snapshot",
                      "- source_item_id: " + (.id|tostring),
                      "- source_stable_id: " + (.stable_id // ""),
                      "- source_created_ns: " + ((.created_ns // 0)|tostring),
                      "- source_updated_ns: " + ((.updated_ns // 0)|tostring),
                      "- source_workspace: " + (.workspace_key // ""),
                      "- source_project: " + (.project_key // ""),
                      "- source_kind: " + (.kind // ""),
                      "- archived_by_rollup_item_id: " + ($rollup_item_id|tostring),
                      "- archived_at_utc: " + $generated_at_utc,
                      "- source_db_path: " + $source_db_path,
                      "",
                      "Source Body",
                      (.body // "")
                    ] | join("\n")
                    ')"
                archive_add_out="$("${MEM_CLI}" add \
                    --db "${archive_db_path}" \
                    --workspace "${archive_workspace}" \
                    --project "${archive_project}" \
                    --kind "${archive_kind}" \
                    --title "${archive_title}" \
                    --body "${archive_body}")"
                archive_add_id="$(printf '%s\n' "${archive_add_out}" | sed -n 's/.*id=\([0-9][0-9]*\).*/\1/p' | tail -n1)"
                printf '%s\t%s\t%s\n' "${archive_add_id:-0}" "${source_id}" "${archive_title}" >> "${tmp_archive_db_add_tsv}"
                archive_db_added_items=$(( archive_db_added_items + 1 ))
            fi

            archive_out="$("${MEM_CLI}" item-archive --db "${db_path}" --id "${source_id}")"
            archive_status="archived"
            if [[ "${archive_out}" == *"already archived"* ]]; then
                archive_status="already_archived"
            fi
            printf '%s\t%s\t%s\n' "${source_id}" "${archive_status}" "${archive_out}" >> "${tmp_apply_archived_source_tsv}"
            archived_source_items=$(( archived_source_items + 1 ))
        done < <(printf '%s\n' "${selected_rollup_json}" | jq -r '.source_item_ids[]')
    fi

    post_neighbors_json="$("${MEM_CLI}" neighbors --db "${db_path}" --item-id "${applied_rollup_id}" --max-edges 64 --max-nodes 64 --format json)"
    post_health_json="$("${MEM_CLI}" health --db "${db_path}" --format json)"
    apply_succeeded=true
fi

source_links_json="$(jq -Rsc '
    split("\n")
    | map(select(length > 0) | split("\t"))
    | map({
        link_id: (.[0] | tonumber),
        to_item_id: (.[1] | tonumber),
        kind: (.[2] // "summarizes"),
        type: (.[3] // "source")
      })
' "${tmp_apply_source_links_tsv}")"
external_links_json="$(jq -Rsc '
    split("\n")
    | map(select(length > 0) | split("\t"))
    | map({
        link_id: (.[0] | tonumber),
        to_item_id: (.[1] | tonumber),
        kind: (.[2] // "related"),
        evidence_count: ((.[3] // "0") | tonumber),
        neighbor_title: (.[4] // "")
      })
' "${tmp_apply_external_links_tsv}")"
archived_source_rows_json="$(jq -Rsc '
    split("\n")
    | map(select(length > 0) | split("\t"))
    | map({
        source_item_id: (.[0] | tonumber),
        status: (.[1] // ""),
        output: (.[2] // "")
      })
' "${tmp_apply_archived_source_tsv}")"
archive_db_rows_json="$(jq -Rsc '
    split("\n")
    | map(select(length > 0) | split("\t"))
    | map({
        archive_item_id: (.[0] | tonumber),
        source_item_id: (.[1] | tonumber),
        source_title: (.[2] // "")
      })
' "${tmp_archive_db_add_tsv}")"

jq -n \
    --arg generated_at_utc "${generated_at_utc}" \
    --arg workspace "${workspace_key}" \
    --arg project "${project_key}" \
    --arg mode "${mode_label}" \
    --argjson apply_attempted "$(if [[ "${apply_attempted}" == true ]]; then echo true; else echo false; fi)" \
    --argjson apply_succeeded "$(if [[ "${apply_succeeded}" == true ]]; then echo true; else echo false; fi)" \
    --argjson group_index "${apply_group}" \
    --argjson source_count "${selected_source_count}" \
    --argjson applied_rollup_id "${applied_rollup_id}" \
    --argjson source_links "${source_links_json}" \
    --argjson external_links "${external_links_json}" \
    --argjson archived_source_rows "${archived_source_rows_json}" \
    --argjson archive_db_rows "${archive_db_rows_json}" \
    --argjson selected_rollup "${selected_rollup_json}" \
    --argjson pre_health "${pre_health_json}" \
    --argjson post_health "${post_health_json}" \
    --argjson post_neighbors "${post_neighbors_json}" \
    --arg archive_db_path "${archive_db_path}" \
    --argjson archive_source_items "$(if (( archive_source_items == 1 )); then echo true; else echo false; fi)" \
    --argjson archived_source_items "${archived_source_items}" \
    --argjson archive_db_added_items "${archive_db_added_items}" \
    --arg apply_error "${apply_error}" \
    '
    {
        generated_at_utc: $generated_at_utc,
        workspace: $workspace,
        project: $project,
        mode: $mode,
        apply_attempted: $apply_attempted,
        apply_succeeded: $apply_succeeded,
        group_index: $group_index,
        selected_source_count: $source_count,
        applied_rollup_id: $applied_rollup_id,
        selected_rollup: $selected_rollup,
        links_added: {
            source_links: ($source_links | length),
            external_links: ($external_links | length),
            source_link_rows: $source_links,
            external_link_rows: $external_links
        },
        source_archive: {
            enabled: $archive_source_items,
            archived_count: $archived_source_items,
            archived_rows: $archived_source_rows
        },
        archive_db_copy: {
            enabled: (($archive_db_path | length) > 0),
            archive_db_path: (if ($archive_db_path | length) > 0 then $archive_db_path else null end),
            added_count: $archive_db_added_items,
            rows: $archive_db_rows
        },
        pre_health: $pre_health,
        post_health: $post_health,
        post_neighbors: $post_neighbors,
        error: (if ($apply_error | length) == 0 then null else $apply_error end)
    }
    ' > "${apply_result_path}"

{
    echo "# Rollup Flow Report"
    echo
    echo "- generated_at_utc: ${generated_at_utc}"
    echo "- mode: ${mode_label}"
    echo "- workspace/project: ${workspace_key}/${project_key}"
    echo "- lane: ${lane_type}"
    echo "- db_path: ${db_path}"
    echo
    echo "## Artifacts"
    echo "- ${candidate_set_path}"
    echo "- ${candidate_summary_path}"
    echo "- ${preview_path}"
    echo "- ${apply_plan_path}"
    echo "- ${apply_result_path}"
    echo "- ${neighbors_path}"
    echo "- ${similarity_edges_path}"
    echo "- ${grouping_path}"
    echo
    echo "## Validation"
    jq '.' "${candidate_set_path}" > /dev/null
    jq '.' "${apply_plan_path}" > /dev/null
    jq '.' "${apply_result_path}" > /dev/null
    jq '.' "${neighbors_path}" > /dev/null
    jq '.' "${similarity_edges_path}" > /dev/null
    jq '.' "${grouping_path}" > /dev/null
    echo "- candidate_set.json: valid"
    echo "- plan_rollup_apply_plan.json: valid"
    echo "- apply_result.json: valid"
    echo "- candidate_neighbors.json: valid"
    echo "- similarity_edges.json: valid"
    echo "- graph_grouping.json: valid"
    echo "- apply_attempted: ${apply_attempted}"
    echo "- apply_succeeded: ${apply_succeeded}"
} > "${report_path}"

echo "manual rollup flow complete:"
echo "  run_dir=${run_dir}"
echo "  candidate_set=${candidate_set_path}"
echo "  preview=${preview_path}"
echo "  apply_plan=${apply_plan_path}"
echo "  apply_result=${apply_result_path}"
echo "  report=${report_path}"
