#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${MAPFORGE_BINARY:-$repo_root/build/toolchains/clang/bin/mapforge}"
if [[ ! -x "$binary" ]]; then
    make -C "$repo_root" BUILD_TOOLCHAIN="${MAPFORGE_SCRIPT_BUILD_TOOLCHAIN:-clang}" app >/dev/null
fi

region_name="${MAPFORGE_START_REGION:-seattle_downtown}"
duration_sec="${MAPFORGE_VIEWPORT_SCENARIO_DURATION_SEC:-18}"
coverage_min="${MAPFORGE_PHASE_A_COVERAGE_MIN:-0.80}"
warmup_samples="${MAPFORGE_PHASE_A_WARMUP_SAMPLES:-3}"
max_consecutive_below="${MAPFORGE_PHASE_A_MAX_CONSEC_BELOW:-2}"
max_l0_latency_ms="${MAPFORGE_PHASE_A_MAX_L0_LATENCY_MS:-900}"

log_file="$(mktemp /tmp/mapforge_phase_a_viewport.XXXXXX)"
cleanup_log=1
trap 'if [[ "$cleanup_log" -eq 1 ]]; then rm -f "$log_file"; fi' EXIT

if ! SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
    SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}" \
    MAPFORGE_RENDER_BACKEND="${MAPFORGE_RENDER_BACKEND:-sdl}" \
    MAPFORGE_VK_DEBUG=1 \
    MAPFORGE_START_REGION="$region_name" \
    MAPFORGE_VIEWPORT_SCENARIO=phase_a \
    MAPFORGE_VIEWPORT_SCENARIO_DURATION_SEC="$duration_sec" \
    "$binary" >"$log_file" 2>&1; then
    cleanup_log=0
    echo "phase_a viewport scenario failed to run executable" >&2
    echo "log: $log_file" >&2
    exit 1
fi

sample_count="$(rg -c "perf_phase_a" "$log_file" || true)"
if [[ "${sample_count:-0}" -lt 2 ]]; then
    cleanup_log=0
    echo "phase_a viewport scenario produced insufficient perf_phase_a samples (count=$sample_count)" >&2
    echo "log: $log_file" >&2
    exit 1
fi

summary="$(
    awk \
        -v min_cov="$coverage_min" \
        -v warmup="$warmup_samples" \
        -v max_consec="$max_consecutive_below" \
        -v max_l0="$max_l0_latency_ms" \
        '
        /perf_phase_a/ {
            cov = -1.0;
            l0 = -1.0;
            if (match($0, /cov\(global=[0-9.]+/)) {
                cov = substr($0, RSTART + 11, RLENGTH - 11) + 0.0;
            }
            if (match($0, /l0\(lat_ms=[0-9.]+/)) {
                l0 = substr($0, RSTART + 10, RLENGTH - 10) + 0.0;
            }
            samples += 1;
            if (samples > warmup) {
                if (cov >= 0.0 && cov < min_cov) {
                    below_total += 1;
                    below_run += 1;
                    if (below_run > below_max) {
                        below_max = below_run;
                    }
                } else {
                    below_run = 0;
                }
                if (l0 > max_l0) {
                    l0_violations += 1;
                }
            }
            if (l0 > l0_peak) {
                l0_peak = l0;
            }
            if (cov >= 0.0 && (cov_min_seen == 0 || cov < cov_min_seen)) {
                cov_min_seen = cov;
            }
        }
        END {
            if (samples == 0) {
                print "FAIL no_samples";
                exit 0;
            }
            if (below_max > max_consec) {
                printf("FAIL coverage_below_run=%d min_cov_seen=%.3f\n", below_max, cov_min_seen);
                exit 0;
            }
            if (l0_violations > 0) {
                printf("FAIL l0_latency_violations=%d l0_peak=%.2f\n", l0_violations, l0_peak);
                exit 0;
            }
            printf("OK samples=%d min_cov=%.3f l0_peak=%.2f below_total=%d\n", samples, cov_min_seen, l0_peak, below_total);
        }
        ' "$log_file"
)"

if [[ "$summary" != OK* ]]; then
    cleanup_log=0
    echo "phase_a viewport scenario gate failed: $summary" >&2
    echo "log: $log_file" >&2
    exit 1
fi

echo "phase_a viewport scenario gate passed: $summary"
