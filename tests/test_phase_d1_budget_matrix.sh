#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stress_script="$repo_root/tests/test_phase_b_continuity_stress.sh"

if [[ ! -x "$stress_script" ]]; then
    echo "missing executable stress script: $stress_script" >&2
    exit 1
fi

max_load_clamp_total="${MAPFORGE_PHASE_D1_MAX_LOAD_CLAMP_TOTAL:-48}"
max_load_ex_total="${MAPFORGE_PHASE_D1_MAX_LOAD_EX_TOTAL:-72}"
max_lane_l2_hits_total="${MAPFORGE_PHASE_D1_MAX_L2_HITS_TOTAL:-12}"
max_lane_l3_hits_total="${MAPFORGE_PHASE_D1_MAX_L3_HITS_TOTAL:-12}"
max_integrate_clamp_total="${MAPFORGE_PHASE_D1_MAX_INTEG_CLAMP_TOTAL:-16}"
max_integrate_ex_total="${MAPFORGE_PHASE_D1_MAX_INTEG_EX_TOTAL:-32}"
max_vk_asset_sat_total="${MAPFORGE_PHASE_D1_MAX_VK_ASSET_SAT_TOTAL:-8}"
max_vk_poly_hit_total="${MAPFORGE_PHASE_D1_MAX_VK_POLY_HIT_TOTAL:-8}"
seattle_max_l0_ms="${MAPFORGE_PHASE_D1_MATRIX_SEATTLE_MAX_L0_MS:-3500}"
downtown_max_l0_ms="${MAPFORGE_PHASE_D1_MATRIX_DOWNTOWN_MAX_L0_MS:-2200}"

run_region() {
    local region="$1"
    local max_l0_ms="$2"
    local output=""
    if ! output="$(
        cd "$repo_root" && \
        MAPFORGE_START_REGION="$region" \
        MAPFORGE_PHASE_B_MAX_L0_LATENCY_MS="$max_l0_ms" \
        MAPFORGE_PHASE_D1_MAX_LOAD_CLAMP_TOTAL="$max_load_clamp_total" \
        MAPFORGE_PHASE_D1_MAX_LOAD_EX_TOTAL="$max_load_ex_total" \
        MAPFORGE_PHASE_D1_MAX_L2_HITS_TOTAL="$max_lane_l2_hits_total" \
        MAPFORGE_PHASE_D1_MAX_L3_HITS_TOTAL="$max_lane_l3_hits_total" \
        MAPFORGE_PHASE_D1_MAX_INTEG_CLAMP_TOTAL="$max_integrate_clamp_total" \
        MAPFORGE_PHASE_D1_MAX_INTEG_EX_TOTAL="$max_integrate_ex_total" \
        MAPFORGE_PHASE_D1_MAX_VK_ASSET_SAT_TOTAL="$max_vk_asset_sat_total" \
        MAPFORGE_PHASE_D1_MAX_VK_POLY_HIT_TOTAL="$max_vk_poly_hit_total" \
        "$stress_script" 2>&1
    )"; then
        printf "%s\n" "$output" >&2
        return 1
    fi

    local summary_line=""
    summary_line="$(printf "%s\n" "$output" | rg "phase_b continuity stress gate passed:" | tail -n 1 || true)"
    if [[ -z "$summary_line" ]]; then
        printf "missing pass summary for region '%s'\n" "$region" >&2
        printf "%s\n" "$output" >&2
        return 1
    fi

    local min_cov=""
    local l0_peak=""
    local load_clamp=""
    local load_ex=""
    local lane_l2=""
    local lane_l3=""
    local integ_clamp=""
    local integ_ex=""
    local vk_sat=""
    local vk_poly_hit=""
    min_cov="$(printf "%s\n" "$summary_line" | sed -n 's/.*min_cov=\([0-9.][0-9.]*\).*/\1/p')"
    l0_peak="$(printf "%s\n" "$summary_line" | sed -n 's/.*l0_peak=\([0-9.][0-9.]*\).*/\1/p')"
    load_clamp="$(printf "%s\n" "$summary_line" | sed -n 's/.*load_clamp=\([0-9][0-9]*\).*/\1/p')"
    load_ex="$(printf "%s\n" "$summary_line" | sed -n 's/.*load_ex=\([0-9][0-9]*\).*/\1/p')"
    lane_l2="$(printf "%s\n" "$summary_line" | sed -n 's/.*lane_l2=\([0-9][0-9]*\).*/\1/p')"
    lane_l3="$(printf "%s\n" "$summary_line" | sed -n 's/.*lane_l3=\([0-9][0-9]*\).*/\1/p')"
    integ_clamp="$(printf "%s\n" "$summary_line" | sed -n 's/.*integ_clamp=\([0-9][0-9]*\).*/\1/p')"
    integ_ex="$(printf "%s\n" "$summary_line" | sed -n 's/.*integ_ex=\([0-9][0-9]*\).*/\1/p')"
    vk_sat="$(printf "%s\n" "$summary_line" | sed -n 's/.*vk_sat=\([0-9][0-9]*\).*/\1/p')"
    vk_poly_hit="$(printf "%s\n" "$summary_line" | sed -n 's/.*vk_poly_hit=\([0-9][0-9]*\).*/\1/p')"

    printf "region=%s min_cov=%s l0_peak=%s load_clamp=%s load_ex=%s lane_l2=%s lane_l3=%s integ_clamp=%s integ_ex=%s vk_sat=%s vk_poly_hit=%s\n" \
        "$region" "$min_cov" "$l0_peak" "$load_clamp" "$load_ex" "$lane_l2" "$lane_l3" "$integ_clamp" "$integ_ex" "$vk_sat" "$vk_poly_hit"
}

echo "phase_d1_budget_matrix thresholds: load_clamp=$max_load_clamp_total load_ex=$max_load_ex_total lane_l2=$max_lane_l2_hits_total lane_l3=$max_lane_l3_hits_total integ_clamp=$max_integrate_clamp_total integ_ex=$max_integrate_ex_total vk_sat=$max_vk_asset_sat_total vk_poly_hit=$max_vk_poly_hit_total"
run_region "seattle_downtown" "$downtown_max_l0_ms"
run_region "seattle" "$seattle_max_l0_ms"
echo "phase_d1_budget_matrix: PASS"
