#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${MAPFORGE_BINARY:-$repo_root/build/toolchains/clang/bin/mapforge}"
if [[ ! -x "$binary" ]]; then
    make -C "$repo_root" BUILD_TOOLCHAIN="${MAPFORGE_SCRIPT_BUILD_TOOLCHAIN:-clang}" app >/dev/null
fi
target_contract_helper="${TARGET_CONTRACT_HELPER:-$repo_root/../bin/desktop_release_target_contract.sh}"
target_triple="$(TARGET_ARCH="${TARGET_ARCH:-}" TARGET_OS="${TARGET_OS:-}" TARGET_VARIANT="${TARGET_VARIANT:-desktop-app}" "$target_contract_helper" get target_triple)"
validator="$repo_root/build/targets/${target_triple}/tools/mapforge_region_validate"
if [[ ! -x "$validator" ]]; then
    make -C "$repo_root" tools-build >/dev/null
fi

region_name="${MAPFORGE_START_REGION:-seattle_downtown}"
duration_sec="${MAPFORGE_VIEWPORT_SCENARIO_DURATION_SEC:-22}"
require_valid_region="${MAPFORGE_PHASE_B_REQUIRE_VALID_REGION:-1}"
coverage_min="${MAPFORGE_PHASE_B_COVERAGE_MIN:-0.78}"
warmup_samples="${MAPFORGE_PHASE_B_WARMUP_SAMPLES:-4}"
max_consecutive_below="${MAPFORGE_PHASE_B_MAX_CONSEC_BELOW:-3}"
max_l0_latency_ms="${MAPFORGE_PHASE_B_MAX_L0_LATENCY_MS:-1250}"
max_cache_evict_frame="${MAPFORGE_PHASE_B_MAX_CACHE_EVICT_FRAME:-180}"
max_band_commit_frame="${MAPFORGE_PHASE_B_MAX_BAND_COMMIT_FRAME:-4}"
max_queue_rebuild_frame="${MAPFORGE_PHASE_B_MAX_QUEUE_REBUILD_FRAME:-3}"
max_fallback_ratio="${MAPFORGE_PHASE_B_MAX_FALLBACK_RATIO:-0.70}"
max_fallback_ratio_violations="${MAPFORGE_PHASE_B_MAX_FALLBACK_RATIO_VIOLATIONS:-2}"
max_band_fallback="${MAPFORGE_PHASE_B_MAX_BAND_FALLBACK:-64}"
max_load_clamp_total="${MAPFORGE_PHASE_D1_MAX_LOAD_CLAMP_TOTAL:-96}"
max_load_ex_total="${MAPFORGE_PHASE_D1_MAX_LOAD_EX_TOTAL:-64}"
max_lane_l2_hits_total="${MAPFORGE_PHASE_D1_MAX_L2_HITS_TOTAL:-24}"
max_lane_l3_hits_total="${MAPFORGE_PHASE_D1_MAX_L3_HITS_TOTAL:-24}"
max_integrate_clamp_total="${MAPFORGE_PHASE_D1_MAX_INTEG_CLAMP_TOTAL:-32}"
max_integrate_ex_total="${MAPFORGE_PHASE_D1_MAX_INTEG_EX_TOTAL:-32}"
max_vk_asset_sat_total="${MAPFORGE_PHASE_D1_MAX_VK_ASSET_SAT_TOTAL:-16}"
max_vk_poly_hit_total="${MAPFORGE_PHASE_D1_MAX_VK_POLY_HIT_TOTAL:-16}"
phase_b_retry_attempt="${MAPFORGE_PHASE_B_RETRY_ATTEMPT:-1}"
phase_b_max_attempts="${MAPFORGE_PHASE_B_MAX_ATTEMPTS:-3}"

log_file="$(mktemp /tmp/mapforge_phase_b_continuity.XXXXXX)"
validate_log="$(mktemp /tmp/mapforge_phase_b_validate.XXXXXX)"
cleanup_log=1
cleanup_validate_log=1
trap 'if [[ "$cleanup_log" -eq 1 ]]; then rm -f "$log_file"; fi; if [[ "$cleanup_validate_log" -eq 1 ]]; then rm -f "$validate_log"; fi' EXIT

if [[ "$require_valid_region" != "0" ]]; then
    if ! (cd "$repo_root" && "$validator" --region "$region_name" >"$validate_log" 2>&1); then
        cleanup_log=0
        cleanup_validate_log=0
        echo "phase_b continuity stress precondition failed: startup region '$region_name' did not validate" >&2
        echo "validator log: $validate_log" >&2
        echo "scenario log: $log_file" >&2
        exit 1
    fi
fi

if ! (cd "$repo_root" && SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
    SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}" \
    MAPFORGE_RENDER_BACKEND="${MAPFORGE_RENDER_BACKEND:-sdl}" \
    MAPFORGE_VK_DEBUG=1 \
    MAPFORGE_START_REGION="$region_name" \
    MAPFORGE_VIEWPORT_SCENARIO=phase_b \
    MAPFORGE_VIEWPORT_SCENARIO_DURATION_SEC="$duration_sec" \
    "$binary" >"$log_file" 2>&1); then
    cleanup_log=0
    echo "phase_b continuity stress failed to run executable" >&2
    echo "log: $log_file" >&2
    exit 1
fi

if rg -q "perf region=no-region" "$log_file"; then
    cleanup_log=0
    echo "phase_b continuity stress precondition failed: runtime fell back to no-region" >&2
    echo "log: $log_file" >&2
    exit 1
fi

phase_samples="$(rg -c "perf_phase_a" "$log_file" || true)"
draw_samples="$(rg -c "perf region=" "$log_file" || true)"
if [[ "${phase_samples:-0}" -lt 2 || "${draw_samples:-0}" -lt 2 ]]; then
    cleanup_log=0
    echo "phase_b continuity stress produced insufficient telemetry samples (phase=$phase_samples draw=$draw_samples)" >&2
    echo "log: $log_file" >&2
    exit 1
fi

summary="$(
    awk \
        -v min_cov="$coverage_min" \
        -v warmup="$warmup_samples" \
        -v max_consec="$max_consecutive_below" \
        -v max_l0="$max_l0_latency_ms" \
        -v max_cache_evict="$max_cache_evict_frame" \
        -v max_band_commit="$max_band_commit_frame" \
        -v max_queue_rebuild="$max_queue_rebuild_frame" \
        -v max_fb_ratio="$max_fallback_ratio" \
        -v max_fb_ratio_viol="$max_fallback_ratio_violations" \
        -v max_band_fb="$max_band_fallback" \
        -v max_load_clamp_total="$max_load_clamp_total" \
        -v max_load_ex_total="$max_load_ex_total" \
        -v max_lane_l2_hits_total="$max_lane_l2_hits_total" \
        -v max_lane_l3_hits_total="$max_lane_l3_hits_total" \
        -v max_integrate_clamp_total="$max_integrate_clamp_total" \
        -v max_integrate_ex_total="$max_integrate_ex_total" \
        -v max_vk_asset_sat_total="$max_vk_asset_sat_total" \
        -v max_vk_poly_hit_total="$max_vk_poly_hit_total" \
        '
        /perf_phase_a/ {
            cov = -1.0;
            l0 = -1.0;
            evict = -1;
            band_commit = -1;
            queue_rebuild = -1;
            load_clamp = 0;
            load_ex = 0;
            lane_l2_hits = 0;
            lane_l3_hits = 0;
            integ_clamp = 0;
            integ_ex = 0;
            vk_asset_sat = 0;
            vk_poly_hit = 0;
            if (match($0, /cov\(global=[0-9.]+/)) {
                cov = substr($0, RSTART + 11, RLENGTH - 11) + 0.0;
            }
            if (match($0, /l0\(lat_ms=[0-9.]+/)) {
                l0 = substr($0, RSTART + 10, RLENGTH - 10) + 0.0;
            }
            if (match($0, /cache\(evict=[0-9]+/)) {
                evict = substr($0, RSTART + 12, RLENGTH - 12) + 0;
            }
            if (match($0, /churn\(frame_band=[0-9]+/)) {
                band_commit = substr($0, RSTART + 18, RLENGTH - 18) + 0;
            }
            if (match($0, /frame_rebuild=[0-9]+/)) {
                queue_rebuild = substr($0, RSTART + 14, RLENGTH - 14) + 0;
            }
            if (match($0, /load req=[0-9]+ app=[0-9]+ clamp=[0-9]+ ex=[0-9]+/)) {
                seg = substr($0, RSTART, RLENGTH);
                split(seg, parts, / /);
                split(parts[4], pair, /=/);
                load_clamp = pair[2] + 0;
                split(parts[5], pair, /=/);
                load_ex = pair[2] + 0;
            }
            if (match($0, /lane_hit=[0-9]+\/[0-9]+\/[0-9]+\/[0-9]+/)) {
                seg = substr($0, RSTART + 9, RLENGTH - 9);
                split(seg, lane_parts, /\//);
                lane_l2_hits = lane_parts[3] + 0;
                lane_l3_hits = lane_parts[4] + 0;
            }
            if (match($0, /integ req=[0-9]+ app=[0-9]+ clamp=[0-9]+ ex=[0-9]+/)) {
                seg = substr($0, RSTART, RLENGTH);
                split(seg, parts, / /);
                split(parts[4], pair, /=/);
                integ_clamp = pair[2] + 0;
                split(parts[5], pair, /=/);
                integ_ex = pair[2] + 0;
            }
            if (match($0, /vk_asset=[0-9]+\/[0-9]+ sat=[0-9]+/)) {
                seg = substr($0, RSTART, RLENGTH);
                split(seg, parts, / /);
                split(parts[2], pair, /=/);
                vk_asset_sat = pair[2] + 0;
            }
            if (match($0, /vk_poly_asset=[0-9]+\/[0-9]+ hit=[0-9]+/)) {
                seg = substr($0, RSTART, RLENGTH);
                split(seg, parts, / /);
                split(parts[2], pair, /=/);
                vk_poly_hit = pair[2] + 0;
            }
            phase_count += 1;
            if (samples_total == 0 || (cov >= 0.0 && cov < cov_min_seen)) {
                cov_min_seen = cov;
            }
            if (l0 > l0_peak) {
                l0_peak = l0;
            }
            if (evict > cache_evict_peak) {
                cache_evict_peak = evict;
            }
            if (band_commit > band_commit_peak) {
                band_commit_peak = band_commit;
            }
            if (queue_rebuild > queue_rebuild_peak) {
                queue_rebuild_peak = queue_rebuild;
            }

            if (phase_count > warmup) {
                if (cov >= 0.0 && cov < min_cov) {
                    cov_below_total += 1;
                    cov_below_run += 1;
                    if (cov_below_run > cov_below_run_max) {
                        cov_below_run_max = cov_below_run;
                    }
                } else {
                    cov_below_run = 0;
                }
                if (l0 > max_l0) {
                    l0_violations += 1;
                }
                if (evict > max_cache_evict) {
                    cache_evict_violations += 1;
                }
                if (band_commit > max_band_commit) {
                    band_commit_violations += 1;
                }
                if (queue_rebuild > max_queue_rebuild) {
                    queue_rebuild_violations += 1;
                }
                load_clamp_total += load_clamp;
                load_ex_total += load_ex;
                lane_l2_hits_total += lane_l2_hits;
                lane_l3_hits_total += lane_l3_hits;
                integ_clamp_total += integ_clamp;
                integ_ex_total += integ_ex;
                vk_asset_sat_total += vk_asset_sat;
                vk_poly_hit_total += vk_poly_hit;
            }
            samples_total += 1;
        }
        /perf region=/ {
            vk = -1;
            fb = -1;
            band_fb = -1;
            if (match($0, /draw_path\(vk=[0-9]+/)) {
                vk = substr($0, RSTART + 13, RLENGTH - 13) + 0;
            }
            if (match($0, /fallback=[0-9]+/)) {
                fb = substr($0, RSTART + 9, RLENGTH - 9) + 0;
            }
            if (match($0, /band_fallback=[0-9]+/)) {
                band_fb = substr($0, RSTART + 14, RLENGTH - 14) + 0;
            }
            draw_count += 1;
            if (vk > 0 && fb >= 0) {
                total = vk + fb;
                if (total > 0) {
                    ratio = fb / total;
                    fb_ratio_samples += 1;
                    if (ratio > fb_ratio_peak) {
                        fb_ratio_peak = ratio;
                    }
                    if (ratio > max_fb_ratio) {
                        fb_ratio_violations += 1;
                    }
                }
            }
            if (band_fb > band_fb_peak) {
                band_fb_peak = band_fb;
            }
            if (band_fb > max_band_fb) {
                band_fb_violations += 1;
            }
        }
        END {
            if (phase_count < 2 || draw_count < 2) {
                printf("FAIL insufficient_samples phase=%d draw=%d\n", phase_count, draw_count);
                exit 0;
            }
            if (cov_below_run_max > max_consec) {
                printf("FAIL coverage_run=%d min_cov=%.3f\n", cov_below_run_max, cov_min_seen);
                exit 0;
            }
            if (l0_violations > 0) {
                printf("FAIL l0_violations=%d l0_peak=%.2f\n", l0_violations, l0_peak);
                exit 0;
            }
            if (cache_evict_violations > 0) {
                printf("FAIL cache_evict_violations=%d peak=%d\n", cache_evict_violations, cache_evict_peak);
                exit 0;
            }
            if (band_commit_violations > 0) {
                printf("FAIL band_commit_violations=%d peak=%d\n", band_commit_violations, band_commit_peak);
                exit 0;
            }
            if (queue_rebuild_violations > 0) {
                printf("FAIL queue_rebuild_violations=%d peak=%d\n", queue_rebuild_violations, queue_rebuild_peak);
                exit 0;
            }
            if (fb_ratio_violations > max_fb_ratio_viol) {
                printf("FAIL fallback_ratio_violations=%d peak=%.3f\n", fb_ratio_violations, fb_ratio_peak);
                exit 0;
            }
            if (band_fb_violations > 0) {
                printf("FAIL band_fallback_violations=%d peak=%d\n", band_fb_violations, band_fb_peak);
                exit 0;
            }
            if (load_clamp_total > max_load_clamp_total) {
                printf("FAIL budget_load_clamp_total=%d max=%d\n", load_clamp_total, max_load_clamp_total);
                exit 0;
            }
            if (load_ex_total > max_load_ex_total) {
                printf("FAIL budget_load_exhaust_total=%d max=%d\n", load_ex_total, max_load_ex_total);
                exit 0;
            }
            if (lane_l2_hits_total > max_lane_l2_hits_total) {
                printf("FAIL budget_lane_l2_hits_total=%d max=%d\n", lane_l2_hits_total, max_lane_l2_hits_total);
                exit 0;
            }
            if (lane_l3_hits_total > max_lane_l3_hits_total) {
                printf("FAIL budget_lane_l3_hits_total=%d max=%d\n", lane_l3_hits_total, max_lane_l3_hits_total);
                exit 0;
            }
            if (integ_clamp_total > max_integrate_clamp_total) {
                printf("FAIL budget_integrate_clamp_total=%d max=%d\n", integ_clamp_total, max_integrate_clamp_total);
                exit 0;
            }
            if (integ_ex_total > max_integrate_ex_total) {
                printf("FAIL budget_integrate_exhaust_total=%d max=%d\n", integ_ex_total, max_integrate_ex_total);
                exit 0;
            }
            if (vk_asset_sat_total > max_vk_asset_sat_total) {
                printf("FAIL budget_vk_asset_sat_total=%d max=%d\n", vk_asset_sat_total, max_vk_asset_sat_total);
                exit 0;
            }
            if (vk_poly_hit_total > max_vk_poly_hit_total) {
                printf("FAIL budget_vk_poly_hit_total=%d max=%d\n", vk_poly_hit_total, max_vk_poly_hit_total);
                exit 0;
            }
            printf("OK phase=%d draw=%d min_cov=%.3f l0_peak=%.2f evict_peak=%d churn_peak(b=%d q=%d) fb_ratio_peak=%.3f fb_ratio_samples=%d band_fb_peak=%d budget(load_clamp=%d load_ex=%d lane_l2=%d lane_l3=%d integ_clamp=%d integ_ex=%d vk_sat=%d vk_poly_hit=%d)\n",
                   phase_count,
                   draw_count,
                   cov_min_seen,
                   l0_peak,
                   cache_evict_peak,
                   band_commit_peak,
                   queue_rebuild_peak,
                   fb_ratio_peak,
                   fb_ratio_samples,
                   band_fb_peak,
                   load_clamp_total,
                   load_ex_total,
                   lane_l2_hits_total,
                   lane_l3_hits_total,
                   integ_clamp_total,
                   integ_ex_total,
                   vk_asset_sat_total,
                   vk_poly_hit_total);
        }
        ' "$log_file"
)"

if [[ "$summary" != OK* ]]; then
    if [[ "$phase_b_retry_attempt" =~ ^[0-9]+$ ]] &&
       [[ "$phase_b_max_attempts" =~ ^[0-9]+$ ]] &&
       [[ "$phase_b_retry_attempt" -lt "$phase_b_max_attempts" ]]; then
        cleanup_log=0
        next_attempt=$((phase_b_retry_attempt + 1))
        echo "phase_b continuity stress retry attempt=${next_attempt}/${phase_b_max_attempts} after summary: $summary" >&2
        echo "log: $log_file" >&2
        exec env MAPFORGE_PHASE_B_RETRY_ATTEMPT="$next_attempt" "$0"
    fi
    cleanup_log=0
    echo "phase_b continuity stress gate failed: $summary" >&2
    echo "log: $log_file" >&2
    exit 1
fi

echo "phase_b continuity stress gate passed: $summary"
