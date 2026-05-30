#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
matrix_script="$repo_root/tests/test_phase_d2_trace_matrix.sh"
d2_trace_dir="$repo_root/build/traces/d2"

if [[ ! -x "$matrix_script" ]]; then
    echo "missing executable matrix script: $matrix_script" >&2
    exit 1
fi

mkdir -p "$d2_trace_dir"

run_stamp="$(date -u +%Y%m%d_%H%M%S)"
report_path="$d2_trace_dir/phase_d2_tuning_profiles_${run_stamp}.tsv"
latest_path="$d2_trace_dir/phase_d2_tuning_profiles_latest.tsv"
trend_report_path="$d2_trace_dir/phase_d2_tuning_trend_${run_stamp}.tsv"
trend_latest_path="$d2_trace_dir/phase_d2_tuning_trend_latest.tsv"

d2_baseline_profile_name="${MAPFORGE_D2_BASELINE_PROFILE_NAME:-baseline}"
d2_candidate_profile_name="${MAPFORGE_D2_CANDIDATE_PROFILE_NAME:-l0_relief_candidate}"
d2_baseline_preset="${MAPFORGE_D2_BASELINE_PRESET:-baseline}"
d2_candidate_preset="${MAPFORGE_D2_CANDIDATE_PRESET:-l0_relief_candidate}"
d2_trend_window="${MAPFORGE_PHASE_D2_TREND_WINDOW:-5}"
d2_profile_max_attempts="${MAPFORGE_PHASE_D2_PROFILE_MAX_ATTEMPTS:-3}"
d2_profile_min_cov_floor="${MAPFORGE_PHASE_D2_PROFILE_MIN_COV_FLOOR:-0.55}"
d2_guardrail_retry_attempt="${MAPFORGE_PHASE_D2_GUARDRAIL_RETRY_ATTEMPT:-1}"
d2_guardrail_max_attempts="${MAPFORGE_PHASE_D2_GUARDRAIL_MAX_ATTEMPTS:-3}"
d2_profile_min_seattle_load_ex="${MAPFORGE_PHASE_D2_PROFILE_MIN_SEATTLE_LOAD_EX:-0}"
skip_guardrails="${MAPFORGE_PHASE_D2_SKIP_GUARDRAILS:-0}"
phase_gate_mode="${MAPFORGE_PHASE_D_GATE_MODE:-d2}"

d2_max_load_ex_delta_seattle="${MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_SEATTLE:-4}"
d2_max_l0_peak_delta_seattle="${MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_SEATTLE:-1200}"
d2_max_load_ex_delta_downtown="${MAPFORGE_PHASE_D2_MAX_LOAD_EX_DELTA_DOWNTOWN:-3}"
d2_max_l0_peak_delta_downtown="${MAPFORGE_PHASE_D2_MAX_L0_PEAK_DELTA_MS_DOWNTOWN:-800}"
d2_max_cov_drop="${MAPFORGE_PHASE_D2_MAX_MIN_COV_DROP:-0.010}"
d2_max_fb_ratio_delta="${MAPFORGE_PHASE_D2_MAX_FB_RATIO_DELTA:-0.001}"
d2_max_churn_band_delta="${MAPFORGE_PHASE_D2_MAX_CHURN_BAND_DELTA:-0}"
d2_max_churn_queue_delta="${MAPFORGE_PHASE_D2_MAX_CHURN_QUEUE_DELTA:-0}"
d2_max_band_fb_peak_delta="${MAPFORGE_PHASE_D2_MAX_BAND_FB_PEAK_DELTA:-0}"

d3_alert_load_ex_delta_seattle="${MAPFORGE_PHASE_D3_ALERT_LOAD_EX_DELTA_SEATTLE:-8}"
d3_alert_l0_peak_delta_seattle="${MAPFORGE_PHASE_D3_ALERT_L0_PEAK_DELTA_MS_SEATTLE:-1200}"
d3_alert_load_ex_delta_downtown="${MAPFORGE_PHASE_D3_ALERT_LOAD_EX_DELTA_DOWNTOWN:-10}"
d3_alert_l0_peak_delta_downtown="${MAPFORGE_PHASE_D3_ALERT_L0_PEAK_DELTA_MS_DOWNTOWN:-800}"

guardrail_failure_count=0
guardrail_failure_entries=""
trend_alert_count=0
trend_alert_messages=""

if ! [[ "$d2_trend_window" =~ ^[0-9]+$ ]] || [[ "$d2_trend_window" -lt 1 ]]; then
    echo "invalid MAPFORGE_PHASE_D2_TREND_WINDOW='$d2_trend_window' (must be integer >=1)" >&2
    exit 1
fi

if ! [[ "$d2_profile_max_attempts" =~ ^[0-9]+$ ]] || [[ "$d2_profile_max_attempts" -lt 1 ]]; then
    echo "invalid MAPFORGE_PHASE_D2_PROFILE_MAX_ATTEMPTS='$d2_profile_max_attempts' (must be integer >=1)" >&2
    exit 1
fi

if ! [[ "$d2_profile_min_seattle_load_ex" =~ ^[0-9]+$ ]]; then
    echo "invalid MAPFORGE_PHASE_D2_PROFILE_MIN_SEATTLE_LOAD_EX='$d2_profile_min_seattle_load_ex' (must be integer >=0)" >&2
    exit 1
fi

if [[ "$phase_gate_mode" != "d2" && "$phase_gate_mode" != "d3" ]]; then
    echo "invalid MAPFORGE_PHASE_D_GATE_MODE='$phase_gate_mode' (must be d2 or d3)" >&2
    exit 1
fi

baseline_aliases=("$d2_baseline_profile_name" "baseline")
candidate_aliases=("$d2_candidate_profile_name" "l0_relief_candidate" "l0_relief")

printf "profile\tregion\ttrace_pack\ttrace_bytes\tmin_cov\tl0_peak_ms\tband_fb_peak\tchurn_band_peak\tchurn_queue_peak\tfb_ratio_peak\tload_clamp\tload_ex\tinteg_clamp\tinteg_ex\tvk_sat\tvk_poly_hit\n" >"$report_path"
printf "metric\tregion\tsamples\tmean_delta\tmin_delta\tmax_delta\n" >"$trend_report_path"

append_profile_rows() {
    local profile="$1"
    local matrix_report="$2"

    if [[ ! -f "$matrix_report" ]]; then
        echo "missing matrix report for profile '$profile': $matrix_report" >&2
        return 1
    fi

    while IFS= read -r row; do
        [[ -n "$row" ]] || continue
        printf "%s\t%s\n" "$profile" "$row" >>"$report_path"
    done < <(tail -n +2 "$matrix_report")
}

append_newline_message() {
    local current="$1"
    local next="$2"
    if [[ -z "$current" ]]; then
        printf "%s" "$next"
        return 0
    fi
    printf "%s\n%s" "$current" "$next"
}

record_guardrail_failure() {
    local failure_class="$1"
    local msg="$2"
    guardrail_failure_count=$((guardrail_failure_count + 1))
    guardrail_failure_entries="$(append_newline_message "$guardrail_failure_entries" "$failure_class|$msg")"
}

emit_guardrail_failures() {
    local header=""
    if [[ "$phase_gate_mode" == "d3" ]]; then
        header="phase_d3_regression_gate failed: continuity_contract breach"
    else
        header="phase_d2_tuning_profiles guardrail failed"
    fi
    printf "%s failures=%d\n" "$header" "$guardrail_failure_count" >&2
    local item=""
    local failure_class=""
    local failure_message=""
    while IFS= read -r item; do
        [[ -n "$item" ]] || continue
        failure_class="${item%%|*}"
        failure_message="${item#*|}"
        printf "  - [%s] %s\n" "$failure_class" "$failure_message" >&2
    done <<<"$guardrail_failure_entries"
    if [[ "$phase_gate_mode" == "d3" ]]; then
        cat >&2 <<'EOF'
triage:
  1. continuity breach is a hard block (do not promote candidate)
  2. rerun: make -C /Users/calebsv/Desktop/CodeWork/map_forge test-phase-d3-regression-gate
  3. inspect latest reports:
     - /Users/calebsv/Desktop/CodeWork/map_forge/build/traces/d2/phase_d2_tuning_profiles_latest.tsv
     - /Users/calebsv/Desktop/CodeWork/map_forge/build/traces/d2/phase_d2_tuning_trend_latest.tsv
EOF
    fi
}

record_trend_alert() {
    local msg="$1"
    trend_alert_count=$((trend_alert_count + 1))
    trend_alert_messages="$(append_newline_message "$trend_alert_messages" "$msg")"
}

emit_trend_alerts() {
    [[ "$trend_alert_count" -gt 0 ]] || return 0
    printf "phase_d3 trend alerts: count=%d (non-blocking)\n" "$trend_alert_count" >&2
    while IFS= read -r item; do
        [[ -n "$item" ]] || continue
        printf "  - %s\n" "$item" >&2
    done <<<"$trend_alert_messages"
}

assert_trend_alert_delta_f64() {
    local label="$1"
    local baseline="$2"
    local relief="$3"
    local max_delta="$4"
    local delta=""
    delta="$(delta_f64 "$baseline" "$relief")"
    if awk -v d="$delta" -v m="$max_delta" 'BEGIN { exit !(d > m) }'; then
        record_trend_alert "$label delta=$delta exceeds alert=$max_delta (baseline=$baseline relief=$relief)"
    fi
}

metric_value_from_report() {
    local report="$1"
    local profile="$2"
    local region="$3"
    local column="$4"
    awk -F '\t' -v p="$profile" -v r="$region" -v c="$column" '
        $1 == p && $2 == r {
            print $c;
            found = 1;
            exit
        }
        END {
            if (!found) {
                exit 1
            }
        }' "$report"
}

metric_value_from_report_profiles() {
    local report="$1"
    local region="$2"
    local column="$3"
    shift 3 || true
    local profile=""
    local value=""
    for profile in "$@"; do
        value="$(metric_value_from_report "$report" "$profile" "$region" "$column" 2>/dev/null || true)"
        if [[ -n "$value" ]]; then
            printf "%s\n" "$value"
            return 0
        fi
    done
    return 1
}

delta_f64() {
    local baseline="$1"
    local relief="$2"
    awk -v base="$baseline" -v val="$relief" 'BEGIN { printf "%.6f", (val - base) }'
}

assert_delta_le_f64() {
    local guardrail_class="$1"
    local label="$2"
    local baseline="$3"
    local relief="$4"
    local max_delta="$5"
    local delta=""
    delta="$(delta_f64 "$baseline" "$relief")"
    if awk -v d="$delta" -v m="$max_delta" 'BEGIN { exit !(d > m) }'; then
        record_guardrail_failure "$guardrail_class" "$label delta=$delta exceeds max=$max_delta (baseline=$baseline relief=$relief)"
    fi
    return 0
}

run_profile() {
    local profile="$1"
    local preset="$2"

    local output=""
    local attempt=1
    local matrix_report=""
    local seattle_cov=""
    local downtown_cov=""
    local seattle_load_ex=""
    local stable_report=0
    local -a preset_env=()
    case "$preset" in
        baseline)
            ;;
        l0_relief|l0_relief_candidate)
            preset_env+=("MAPFORGE_BUDGET_LOAD_ROAD_CAP=${MAPFORGE_D2_L0_RELIEF_LOAD_ROAD_CAP:-7}")
            preset_env+=("MAPFORGE_BUDGET_LOAD_POLY_CAP_SMALL=${MAPFORGE_D2_L0_RELIEF_POLY_CAP_SMALL:-4}")
            preset_env+=("MAPFORGE_BUDGET_LOAD_POLY_CAP_MEDIUM=${MAPFORGE_D2_L0_RELIEF_POLY_CAP_MEDIUM:-3}")
            preset_env+=("MAPFORGE_BUDGET_LOAD_POLY_CAP_LARGE=${MAPFORGE_D2_L0_RELIEF_POLY_CAP_LARGE:-2}")
            preset_env+=("MAPFORGE_BUDGET_INTEGRATE_CAP=${MAPFORGE_D2_L0_RELIEF_INTEGRATE_CAP:-60}")
            ;;
        *)
            printf "unknown D2 preset '%s' for profile '%s'\n" "$preset" "$profile" >&2
            return 1
            ;;
    esac

    while [[ "$attempt" -le "$d2_profile_max_attempts" ]]; do
        if ! output="$(cd "$repo_root" && env "${preset_env[@]}" "$matrix_script" 2>&1)"; then
            if [[ "$attempt" -lt "$d2_profile_max_attempts" ]]; then
                printf "phase_d2_tuning_profiles: retry profile=%s preset=%s attempt=%d/%d after matrix failure\n" \
                    "$profile" "$preset" "$attempt" "$d2_profile_max_attempts" >&2
                printf "%s\n" "$output" >&2
                attempt=$((attempt + 1))
                continue
            fi
            printf "%s\n" "$output" >&2
            return 1
        fi

        matrix_report="$(printf "%s\n" "$output" | sed -n 's/.*report=\([^ ]*\).*/\1/p' | tail -n 1)"
        if [[ -z "$matrix_report" ]]; then
            printf "unable to locate matrix report for profile '%s'\n" "$profile" >&2
            printf "%s\n" "$output" >&2
            return 1
        fi

        seattle_cov="$(awk -F '\t' '$1=="seattle" { print $4; exit }' "$matrix_report")"
        downtown_cov="$(awk -F '\t' '$1=="seattle_downtown" { print $4; exit }' "$matrix_report")"
        seattle_load_ex="$(awk -F '\t' '$1=="seattle" { print $11; exit }' "$matrix_report")"
        local needs_load_floor=0
        if [[ "$preset" == "baseline" ]]; then
            needs_load_floor=1
        fi
        if awk -v s="$seattle_cov" -v d="$downtown_cov" -v l="$seattle_load_ex" -v floor="$d2_profile_min_cov_floor" -v min_load="$d2_profile_min_seattle_load_ex" -v need_load="$needs_load_floor" 'BEGIN { exit !((s + 0) >= floor && (d + 0) >= floor && (need_load == 0 || (l + 0) >= min_load)) }'; then
            stable_report=1
            break
        fi

        printf "d2_profile retry profile=%s preset=%s attempt=%d/%s unstable_row(seattle_cov=%s downtown_cov=%s cov_floor=%s seattle_load_ex=%s min_load_ex=%s baseline_load_floor=%s)\n" \
            "$profile" "$preset" "$attempt" "$d2_profile_max_attempts" "$seattle_cov" "$downtown_cov" "$d2_profile_min_cov_floor" "$seattle_load_ex" "$d2_profile_min_seattle_load_ex" "$needs_load_floor" >&2
        attempt=$((attempt + 1))
    done

    if [[ "$stable_report" -ne 1 ]]; then
        printf "d2_profile unstable profile=%s preset=%s attempts=%s row(seattle_cov=%s downtown_cov=%s cov_floor=%s seattle_load_ex=%s min_load_ex=%s)\n" \
            "$profile" "$preset" "$d2_profile_max_attempts" "$seattle_cov" "$downtown_cov" "$d2_profile_min_cov_floor" "$seattle_load_ex" "$d2_profile_min_seattle_load_ex" >&2
        return 1
    fi

    append_profile_rows "$profile" "$matrix_report"

    local seattle_summary=""
    seattle_summary="$(awk -F '\t' -v p="$profile" '$1==p && $2=="seattle" {printf "min_cov=%s l0_peak=%s load_ex=%s integ_ex=%s", $5, $6, $12, $14}' "$report_path")"
    local downtown_summary=""
    downtown_summary="$(awk -F '\t' -v p="$profile" '$1==p && $2=="seattle_downtown" {printf "min_cov=%s l0_peak=%s load_ex=%s integ_ex=%s", $5, $6, $12, $14}' "$report_path")"
    printf "d2_profile profile=%s preset=%s downtown(%s) seattle(%s)\n" "$profile" "$preset" "$downtown_summary" "$seattle_summary"
}

append_trend_metric() {
    local metric="$1"
    local region="$2"
    local mode="$3"
    local column="$4"
    shift 4 || true
    local report_paths=("$@")

    local tmp_values=""
    tmp_values="$(mktemp /tmp/mapforge_d2_trend_values.XXXXXX)"
    local report=""
    local baseline_value=""
    local candidate_value=""
    local delta=""
    local sample_count=0
    for report in "${report_paths[@]}"; do
        baseline_value="$(metric_value_from_report_profiles "$report" "$region" "$column" "${baseline_aliases[@]}" 2>/dev/null || true)"
        candidate_value="$(metric_value_from_report_profiles "$report" "$region" "$column" "${candidate_aliases[@]}" 2>/dev/null || true)"
        if [[ -z "$baseline_value" || -z "$candidate_value" ]]; then
            continue
        fi

        if [[ "$mode" == "drop" ]]; then
            delta="$(delta_f64 "$candidate_value" "$baseline_value")"
        else
            delta="$(delta_f64 "$baseline_value" "$candidate_value")"
        fi
        printf "%s\n" "$delta" >>"$tmp_values"
        sample_count=$((sample_count + 1))
    done

    if [[ "$sample_count" -eq 0 ]]; then
        rm -f "$tmp_values"
        printf "%s\t%s\t0\tNA\tNA\tNA\n" "$metric" "$region" >>"$trend_report_path"
        return 0
    fi

    local stats=""
    stats="$(awk '
        NR == 1 { min = $1; max = $1; sum = $1; count = 1; next }
        { if ($1 < min) min = $1; if ($1 > max) max = $1; sum += $1; count += 1 }
        END { printf "%.6f\t%.6f\t%.6f\t%d", (sum / count), min, max, count }
    ' "$tmp_values")"
    rm -f "$tmp_values"

    local mean=""
    local min=""
    local max=""
    local count=""
    IFS=$'\t' read -r mean min max count <<<"$stats"
    printf "%s\t%s\t%s\t%s\t%s\t%s\n" "$metric" "$region" "$count" "$mean" "$min" "$max" >>"$trend_report_path"
}

run_profile "$d2_baseline_profile_name" "$d2_baseline_preset"
run_profile "$d2_candidate_profile_name" "$d2_candidate_preset"

baseline_load_ex="$(metric_value_from_report_profiles "$report_path" "seattle" "12" "${baseline_aliases[@]}")"
candidate_load_ex="$(metric_value_from_report_profiles "$report_path" "seattle" "12" "${candidate_aliases[@]}")"
baseline_l0_peak="$(metric_value_from_report_profiles "$report_path" "seattle" "6" "${baseline_aliases[@]}")"
candidate_l0_peak="$(metric_value_from_report_profiles "$report_path" "seattle" "6" "${candidate_aliases[@]}")"

baseline_load_ex_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "12" "${baseline_aliases[@]}")"
candidate_load_ex_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "12" "${candidate_aliases[@]}")"
baseline_l0_peak_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "6" "${baseline_aliases[@]}")"
candidate_l0_peak_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "6" "${candidate_aliases[@]}")"

baseline_cov_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "5" "${baseline_aliases[@]}")"
candidate_cov_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "5" "${candidate_aliases[@]}")"
baseline_cov_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "5" "${baseline_aliases[@]}")"
candidate_cov_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "5" "${candidate_aliases[@]}")"

baseline_fb_ratio_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "10" "${baseline_aliases[@]}")"
candidate_fb_ratio_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "10" "${candidate_aliases[@]}")"
baseline_fb_ratio_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "10" "${baseline_aliases[@]}")"
candidate_fb_ratio_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "10" "${candidate_aliases[@]}")"

baseline_churn_band_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "8" "${baseline_aliases[@]}")"
candidate_churn_band_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "8" "${candidate_aliases[@]}")"
baseline_churn_queue_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "9" "${baseline_aliases[@]}")"
candidate_churn_queue_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "9" "${candidate_aliases[@]}")"

baseline_churn_band_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "8" "${baseline_aliases[@]}")"
candidate_churn_band_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "8" "${candidate_aliases[@]}")"
baseline_churn_queue_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "9" "${baseline_aliases[@]}")"
candidate_churn_queue_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "9" "${candidate_aliases[@]}")"

baseline_band_fb_peak_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "7" "${baseline_aliases[@]}")"
candidate_band_fb_peak_seattle="$(metric_value_from_report_profiles "$report_path" "seattle" "7" "${candidate_aliases[@]}")"
baseline_band_fb_peak_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "7" "${baseline_aliases[@]}")"
candidate_band_fb_peak_downtown="$(metric_value_from_report_profiles "$report_path" "seattle_downtown" "7" "${candidate_aliases[@]}")"

if [[ "$skip_guardrails" != "1" ]]; then
    assert_delta_le_f64 "throughput_drift" "seattle load_ex" "$baseline_load_ex" "$candidate_load_ex" "$d2_max_load_ex_delta_seattle"
    assert_delta_le_f64 "throughput_drift" "seattle l0_peak_ms" "$baseline_l0_peak" "$candidate_l0_peak" "$d2_max_l0_peak_delta_seattle"
    assert_delta_le_f64 "throughput_drift" "seattle_downtown load_ex" "$baseline_load_ex_downtown" "$candidate_load_ex_downtown" "$d2_max_load_ex_delta_downtown"
    assert_delta_le_f64 "throughput_drift" "seattle_downtown l0_peak_ms" "$baseline_l0_peak_downtown" "$candidate_l0_peak_downtown" "$d2_max_l0_peak_delta_downtown"

    assert_delta_le_f64 "continuity_contract" "seattle min_cov drop" "$candidate_cov_seattle" "$baseline_cov_seattle" "$d2_max_cov_drop"
    assert_delta_le_f64 "continuity_contract" "seattle_downtown min_cov drop" "$candidate_cov_downtown" "$baseline_cov_downtown" "$d2_max_cov_drop"
    assert_delta_le_f64 "continuity_contract" "seattle fb_ratio_peak" "$baseline_fb_ratio_seattle" "$candidate_fb_ratio_seattle" "$d2_max_fb_ratio_delta"
    assert_delta_le_f64 "continuity_contract" "seattle_downtown fb_ratio_peak" "$baseline_fb_ratio_downtown" "$candidate_fb_ratio_downtown" "$d2_max_fb_ratio_delta"
    assert_delta_le_f64 "continuity_contract" "seattle churn_band_peak" "$baseline_churn_band_seattle" "$candidate_churn_band_seattle" "$d2_max_churn_band_delta"
    assert_delta_le_f64 "continuity_contract" "seattle churn_queue_peak" "$baseline_churn_queue_seattle" "$candidate_churn_queue_seattle" "$d2_max_churn_queue_delta"
    assert_delta_le_f64 "continuity_contract" "seattle_downtown churn_band_peak" "$baseline_churn_band_downtown" "$candidate_churn_band_downtown" "$d2_max_churn_band_delta"
    assert_delta_le_f64 "continuity_contract" "seattle_downtown churn_queue_peak" "$baseline_churn_queue_downtown" "$candidate_churn_queue_downtown" "$d2_max_churn_queue_delta"
    assert_delta_le_f64 "continuity_contract" "seattle band_fb_peak" "$baseline_band_fb_peak_seattle" "$candidate_band_fb_peak_seattle" "$d2_max_band_fb_peak_delta"
    assert_delta_le_f64 "continuity_contract" "seattle_downtown band_fb_peak" "$baseline_band_fb_peak_downtown" "$candidate_band_fb_peak_downtown" "$d2_max_band_fb_peak_delta"
fi

if [[ "$guardrail_failure_count" -gt 0 ]]; then
    if [[ "$phase_gate_mode" == "d3" ]]; then
        # D3 only hard-blocks continuity contract metrics; throughput metrics remain trend alerts.
        continuity_only_entries=""
        while IFS= read -r item; do
            [[ -n "$item" ]] || continue
            failure_class="${item%%|*}"
            if [[ "$failure_class" == "continuity_contract" ]]; then
                continuity_only_entries="$(append_newline_message "$continuity_only_entries" "$item")"
            fi
        done <<<"$guardrail_failure_entries"
        if [[ -n "$continuity_only_entries" ]]; then
            guardrail_failure_entries="$continuity_only_entries"
            guardrail_failure_count="$(printf "%s\n" "$continuity_only_entries" | awk 'NF { count += 1 } END { print count + 0 }')"
            emit_guardrail_failures
            exit 1
        fi
        guardrail_failure_count=0
        guardrail_failure_entries=""
    else
        if [[ "$d2_guardrail_retry_attempt" =~ ^[0-9]+$ ]] &&
           [[ "$d2_guardrail_max_attempts" =~ ^[0-9]+$ ]] &&
           [[ "$d2_guardrail_retry_attempt" -lt "$d2_guardrail_max_attempts" ]]; then
            next_attempt=$((d2_guardrail_retry_attempt + 1))
            printf "phase_d2_tuning_profiles: retry whole-guardrail attempt=%d/%d after final guardrail failure\n" \
                "$next_attempt" "$d2_guardrail_max_attempts" >&2
            exec env MAPFORGE_PHASE_D2_GUARDRAIL_RETRY_ATTEMPT="$next_attempt" "$0"
        fi
        emit_guardrail_failures
        exit 1
    fi
fi

if [[ "$phase_gate_mode" == "d3" ]]; then
    assert_trend_alert_delta_f64 "seattle load_ex" "$baseline_load_ex" "$candidate_load_ex" "$d3_alert_load_ex_delta_seattle"
    assert_trend_alert_delta_f64 "seattle l0_peak_ms" "$baseline_l0_peak" "$candidate_l0_peak" "$d3_alert_l0_peak_delta_seattle"
    assert_trend_alert_delta_f64 "seattle_downtown load_ex" "$baseline_load_ex_downtown" "$candidate_load_ex_downtown" "$d3_alert_load_ex_delta_downtown"
    assert_trend_alert_delta_f64 "seattle_downtown l0_peak_ms" "$baseline_l0_peak_downtown" "$candidate_l0_peak_downtown" "$d3_alert_l0_peak_delta_downtown"
    emit_trend_alerts
fi

mapfile -t recent_reports < <(ls -1t "$d2_trace_dir"/phase_d2_tuning_profiles_*.tsv 2>/dev/null | head -n "$d2_trend_window")
append_trend_metric "load_ex_delta" "seattle" "delta" "12" "${recent_reports[@]}"
append_trend_metric "load_ex_delta" "seattle_downtown" "delta" "12" "${recent_reports[@]}"
append_trend_metric "l0_peak_delta" "seattle" "delta" "6" "${recent_reports[@]}"
append_trend_metric "l0_peak_delta" "seattle_downtown" "delta" "6" "${recent_reports[@]}"
append_trend_metric "min_cov_drop" "seattle" "drop" "5" "${recent_reports[@]}"
append_trend_metric "min_cov_drop" "seattle_downtown" "drop" "5" "${recent_reports[@]}"
append_trend_metric "fb_ratio_delta" "seattle" "delta" "10" "${recent_reports[@]}"
append_trend_metric "fb_ratio_delta" "seattle_downtown" "delta" "10" "${recent_reports[@]}"
append_trend_metric "churn_band_delta" "seattle" "delta" "8" "${recent_reports[@]}"
append_trend_metric "churn_band_delta" "seattle_downtown" "delta" "8" "${recent_reports[@]}"
append_trend_metric "churn_queue_delta" "seattle" "delta" "9" "${recent_reports[@]}"
append_trend_metric "churn_queue_delta" "seattle_downtown" "delta" "9" "${recent_reports[@]}"
append_trend_metric "band_fb_peak_delta" "seattle" "delta" "7" "${recent_reports[@]}"
append_trend_metric "band_fb_peak_delta" "seattle_downtown" "delta" "7" "${recent_reports[@]}"

cp "$report_path" "$latest_path"
cp "$trend_report_path" "$trend_latest_path"
printf "phase_d2_tuning_profiles: PASS mode=%s trend_alerts=%d report=%s latest=%s trend=%s trend_latest=%s profiles(%s:%s %s:%s) skip_guardrails=%s seattle_delta(load_ex=%s->%s l0_peak=%s->%s) downtown_delta(load_ex=%s->%s l0_peak=%s->%s)\n" \
    "$phase_gate_mode" "$trend_alert_count" \
    "$report_path" "$latest_path" "$trend_report_path" "$trend_latest_path" \
    "$d2_baseline_profile_name" "$d2_baseline_preset" "$d2_candidate_profile_name" "$d2_candidate_preset" "$skip_guardrails" \
    "$baseline_load_ex" "$candidate_load_ex" "$baseline_l0_peak" "$candidate_l0_peak" \
    "$baseline_load_ex_downtown" "$candidate_load_ex_downtown" "$baseline_l0_peak_downtown" "$candidate_l0_peak_downtown"
