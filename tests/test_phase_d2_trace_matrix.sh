#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stress_script="$repo_root/tests/test_phase_b_continuity_stress.sh"
trace_root="$repo_root/build/traces"
d2_trace_dir="$trace_root/d2"
d2_max_load_clamp_total="${MAPFORGE_PHASE_D2_MATRIX_MAX_LOAD_CLAMP_TOTAL:-96}"
d2_max_load_ex_total="${MAPFORGE_PHASE_D2_MATRIX_MAX_LOAD_EX_TOTAL:-72}"
d2_max_lane_l2_hits="${MAPFORGE_PHASE_D2_MATRIX_MAX_L2_HITS_TOTAL:-24}"
d2_max_lane_l3_hits="${MAPFORGE_PHASE_D2_MATRIX_MAX_L3_HITS_TOTAL:-12}"
d2_max_integ_clamp_total="${MAPFORGE_PHASE_D2_MATRIX_MAX_INTEG_CLAMP_TOTAL:-16}"
d2_max_integ_ex_total="${MAPFORGE_PHASE_D2_MATRIX_MAX_INTEG_EX_TOTAL:-32}"
d2_max_vk_asset_sat_total="${MAPFORGE_PHASE_D2_MATRIX_MAX_VK_ASSET_SAT_TOTAL:-8}"
d2_max_vk_poly_hit_total="${MAPFORGE_PHASE_D2_MATRIX_MAX_VK_POLY_HIT_TOTAL:-8}"
binary="${MAPFORGE_BINARY:-$repo_root/build/toolchains/clang/bin/mapforge}"

if [[ ! -x "$stress_script" ]]; then
    echo "missing executable stress script: $stress_script" >&2
    exit 1
fi

if [[ ! -x "$binary" ]]; then
    make -C "$repo_root" BUILD_TOOLCHAIN="${MAPFORGE_SCRIPT_BUILD_TOOLCHAIN:-clang}" app >/dev/null
fi

mkdir -p "$d2_trace_dir"

run_stamp="$(date -u +%Y%m%d_%H%M%S)"
report_path="$d2_trace_dir/phase_d2_trace_matrix_${run_stamp}.tsv"
latest_path="$d2_trace_dir/phase_d2_trace_matrix_latest.tsv"

printf "region\ttrace_pack\ttrace_bytes\tmin_cov\tl0_peak_ms\tband_fb_peak\tchurn_band_peak\tchurn_queue_peak\tfb_ratio_peak\tload_clamp\tload_ex\tinteg_clamp\tinteg_ex\tvk_sat\tvk_poly_hit\n" >"$report_path"

extract_metric() {
    local line="$1"
    local pattern="$2"
    printf "%s\n" "$line" | sed -n "s/.*$pattern\\([0-9.][0-9.]*\\).*/\\1/p"
}

extract_churn_band_peak() {
    local line="$1"
    printf "%s\n" "$line" | sed -n 's/.*churn_peak(b=\([0-9][0-9]*\) q=[0-9][0-9]*).*/\1/p'
}

extract_churn_queue_peak() {
    local line="$1"
    printf "%s\n" "$line" | sed -n 's/.*churn_peak(b=[0-9][0-9]* q=\([0-9][0-9]*\)).*/\1/p'
}

run_case() {
    local region="$1"
    local max_l0_ms="$2"
    local min_cov="$3"
    local max_consec="$4"

    local before_latest=""
    before_latest="$(ls -1t "$trace_root"/mapforge_trace_*.pack 2>/dev/null | head -n 1 || true)"

    local output=""
    if ! output="$(
        cd "$repo_root" && \
        MAPFORGE_START_REGION="$region" \
        MAPFORGE_PHASE_B_MAX_L0_LATENCY_MS="$max_l0_ms" \
        MAPFORGE_PHASE_B_COVERAGE_MIN="$min_cov" \
        MAPFORGE_PHASE_B_MAX_CONSEC_BELOW="$max_consec" \
        MAPFORGE_PHASE_D1_MAX_LOAD_CLAMP_TOTAL="$d2_max_load_clamp_total" \
        MAPFORGE_PHASE_D1_MAX_LOAD_EX_TOTAL="$d2_max_load_ex_total" \
        MAPFORGE_PHASE_D1_MAX_L2_HITS_TOTAL="$d2_max_lane_l2_hits" \
        MAPFORGE_PHASE_D1_MAX_L3_HITS_TOTAL="$d2_max_lane_l3_hits" \
        MAPFORGE_PHASE_D1_MAX_INTEG_CLAMP_TOTAL="$d2_max_integ_clamp_total" \
        MAPFORGE_PHASE_D1_MAX_INTEG_EX_TOTAL="$d2_max_integ_ex_total" \
        MAPFORGE_PHASE_D1_MAX_VK_ASSET_SAT_TOTAL="$d2_max_vk_asset_sat_total" \
        MAPFORGE_PHASE_D1_MAX_VK_POLY_HIT_TOTAL="$d2_max_vk_poly_hit_total" \
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

    local after_latest=""
    after_latest="$(ls -1t "$trace_root"/mapforge_trace_*.pack 2>/dev/null | head -n 1 || true)"
    if [[ -z "$after_latest" ]]; then
        echo "no trace pack found after region run: $region" >&2
        return 1
    fi
    if [[ -n "$before_latest" && "$after_latest" == "$before_latest" ]]; then
        echo "expected a new trace pack for region run: $region" >&2
        return 1
    fi

    local copied_trace="$d2_trace_dir/${region}_${run_stamp}.pack"
    cp "$after_latest" "$copied_trace"

    local trace_bytes=""
    trace_bytes="$(stat -f "%z" "$copied_trace")"
    local min_cov_v=""
    local l0_peak_v=""
    local band_fb_peak_v=""
    local fb_ratio_peak_v=""
    local churn_band_peak_v=""
    local churn_queue_peak_v=""
    local load_clamp_v=""
    local load_ex_v=""
    local integ_clamp_v=""
    local integ_ex_v=""
    local vk_sat_v=""
    local vk_poly_hit_v=""

    min_cov_v="$(extract_metric "$summary_line" "min_cov=")"
    l0_peak_v="$(extract_metric "$summary_line" "l0_peak=")"
    band_fb_peak_v="$(extract_metric "$summary_line" "band_fb_peak=")"
    fb_ratio_peak_v="$(extract_metric "$summary_line" "fb_ratio_peak=")"
    churn_band_peak_v="$(extract_churn_band_peak "$summary_line")"
    churn_queue_peak_v="$(extract_churn_queue_peak "$summary_line")"
    load_clamp_v="$(extract_metric "$summary_line" "load_clamp=")"
    load_ex_v="$(extract_metric "$summary_line" "load_ex=")"
    integ_clamp_v="$(extract_metric "$summary_line" "integ_clamp=")"
    integ_ex_v="$(extract_metric "$summary_line" "integ_ex=")"
    vk_sat_v="$(extract_metric "$summary_line" "vk_sat=")"
    vk_poly_hit_v="$(extract_metric "$summary_line" "vk_poly_hit=")"

    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
        "$region" \
        "$copied_trace" \
        "$trace_bytes" \
        "$min_cov_v" \
        "$l0_peak_v" \
        "$band_fb_peak_v" \
        "$churn_band_peak_v" \
        "$churn_queue_peak_v" \
        "$fb_ratio_peak_v" \
        "$load_clamp_v" \
        "$load_ex_v" \
        "$integ_clamp_v" \
        "$integ_ex_v" \
        "$vk_sat_v" \
        "$vk_poly_hit_v" >>"$report_path"

    printf "d2_trace region=%s trace=%s bytes=%s min_cov=%s l0_peak=%s churn(b=%s q=%s) load_ex=%s integ_ex=%s\n" \
        "$region" "$copied_trace" "$trace_bytes" "$min_cov_v" "$l0_peak_v" "$churn_band_peak_v" "$churn_queue_peak_v" "$load_ex_v" "$integ_ex_v"
}

run_case "seattle_downtown" "${MAPFORGE_PHASE_D2_MATRIX_DOWNTOWN_MAX_L0_MS:-2200}" "${MAPFORGE_PHASE_D2_MATRIX_DOWNTOWN_MIN_COV:-0.55}" "${MAPFORGE_PHASE_D2_MATRIX_DOWNTOWN_MAX_CONSEC_BELOW:-6}"
run_case "seattle" "${MAPFORGE_PHASE_D2_MATRIX_SEATTLE_MAX_L0_MS:-4500}" "${MAPFORGE_PHASE_D2_MATRIX_SEATTLE_MIN_COV:-0.55}" "${MAPFORGE_PHASE_D2_MATRIX_SEATTLE_MAX_CONSEC_BELOW:-6}"

cp "$report_path" "$latest_path"
echo "phase_d2_trace_matrix: PASS report=$report_path latest=$latest_path"
