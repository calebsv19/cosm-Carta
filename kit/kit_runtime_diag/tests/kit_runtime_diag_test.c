#include "kit_runtime_diag.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void assert_near(double actual, double expected) {
    if (fabs(actual - expected) > 0.0001) {
        fprintf(stderr, "expected %.4f got %.4f\n", expected, actual);
        exit(1);
    }
}

static void test_timing_compute(void) {
    KitRuntimeDiagStageMarks marks = {
        .frame_begin = 100.000,
        .after_events = 100.002,
        .after_update = 100.007,
        .after_queue = 100.010,
        .after_integrate = 100.014,
        .after_route = 100.020,
        .after_render_derive = 100.023,
        .before_present = 100.031,
        .after_render = 100.034,
    };
    KitRuntimeDiagTimings timings = {0};
    kit_runtime_diag_compute_timings(&marks, &timings);

    assert_near(timings.frame_ms, 34.0);
    assert_near(timings.events_ms, 2.0);
    assert_near(timings.update_ms, 5.0);
    assert_near(timings.queue_ms, 3.0);
    assert_near(timings.integrate_ms, 4.0);
    assert_near(timings.route_ms, 6.0);
    assert_near(timings.render_ms, 11.0);
    assert_near(timings.present_ms, 3.0);
    assert_near(timings.render_derive_ms, 3.0);
    assert_near(timings.render_submit_ms, 8.0);
}

static void test_input_totals_accumulate(void) {
    KitRuntimeDiagInputTotals totals = {0};
    KitRuntimeDiagInputFrame frame_a = {
        .raw_event_count = 3u,
        .action_count = 2u,
        .text_entry_gate_active = true,
        .ignored_count = 1u,
        .routed_global_count = 1u,
        .routed_pane_count = 1u,
        .routed_fallback_count = 0u,
        .target_invalidation_count = 2u,
        .full_invalidation_count = 0u,
    };
    KitRuntimeDiagInputFrame frame_b = {
        .raw_event_count = 4u,
        .action_count = 3u,
        .text_entry_gate_active = false,
        .ignored_count = 5u,
        .routed_global_count = 0u,
        .routed_pane_count = 2u,
        .routed_fallback_count = 1u,
        .target_invalidation_count = 1u,
        .full_invalidation_count = 1u,
    };

    kit_runtime_diag_input_totals_accumulate(&totals, &frame_a);
    kit_runtime_diag_input_totals_accumulate(&totals, &frame_b);

    if (totals.raw_event_count != 7u || totals.action_count != 5u ||
        totals.shortcut_gated_count != 1u || totals.routed_global_count != 1u ||
        totals.routed_pane_count != 3u || totals.routed_fallback_count != 1u ||
        totals.target_invalidation_count != 3u || totals.full_invalidation_count != 1u) {
        fprintf(stderr, "input total mismatch\n");
        exit(1);
    }
}

int main(void) {
    test_timing_compute();
    test_input_totals_accumulate();
    printf("kit_runtime_diag_test: success\n");
    return 0;
}
