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

static void assert_u64(uint64_t actual, uint64_t expected, const char *label) {
    if (actual != expected) {
        fprintf(stderr, "%s expected %llu got %llu\n",
                label,
                (unsigned long long)expected,
                (unsigned long long)actual);
        exit(1);
    }
}

static void test_duration_ms_raw_and_invalid_inputs(void) {
    assert_near(kit_runtime_diag_duration_ms(5.0, 5.002), 2.0);
    assert_near(kit_runtime_diag_duration_ms(5.0, 4.999), -1.0);
    assert_near(kit_runtime_diag_duration_ms(NAN, 1.0), 0.0);
    assert_near(kit_runtime_diag_duration_ms(1.0, INFINITY), 0.0);
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

static void test_timing_compute_null_and_invalid_inputs(void) {
    KitRuntimeDiagTimings timings = {
        .frame_ms = 1.0, .events_ms = 2.0, .update_ms = 3.0, .queue_ms = 4.0, .integrate_ms = 5.0,
        .route_ms = 6.0, .render_ms = 7.0, .present_ms = 8.0, .render_derive_ms = 9.0,
        .render_submit_ms = 10.0,
    };
    kit_runtime_diag_compute_timings(NULL, &timings);
    assert_near(timings.frame_ms, 0.0);
    assert_near(timings.render_submit_ms, 0.0);

    KitRuntimeDiagStageMarks invalid_marks = {
        .frame_begin = 1.0,
        .after_events = 2.0,
        .after_update = NAN,
        .after_queue = 4.0,
        .after_integrate = 5.0,
        .after_route = 6.0,
        .after_render_derive = 7.0,
        .before_present = 8.0,
        .after_render = 9.0,
    };
    timings.frame_ms = 123.0;
    timings.present_ms = 456.0;
    kit_runtime_diag_compute_timings(&invalid_marks, &timings);
    assert_near(timings.frame_ms, 0.0);
    assert_near(timings.present_ms, 0.0);

    kit_runtime_diag_compute_timings(&invalid_marks, NULL);
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

    assert_u64(totals.raw_event_count, 7u, "raw_event_count");
    assert_u64(totals.action_count, 5u, "action_count");
    assert_u64(totals.shortcut_gated_count, 1u, "shortcut_gated_count");
    assert_u64(totals.routed_global_count, 1u, "routed_global_count");
    assert_u64(totals.routed_pane_count, 3u, "routed_pane_count");
    assert_u64(totals.routed_fallback_count, 1u, "routed_fallback_count");
    assert_u64(totals.target_invalidation_count, 3u, "target_invalidation_count");
    assert_u64(totals.full_invalidation_count, 1u, "full_invalidation_count");
}

static void test_input_totals_null_and_saturation(void) {
    KitRuntimeDiagInputTotals totals = {
        .raw_event_count = UINT64_MAX - 1u,
        .action_count = UINT64_MAX,
        .shortcut_gated_count = UINT64_MAX - 2u,
        .routed_global_count = UINT64_MAX - 3u,
        .routed_pane_count = UINT64_MAX - 4u,
        .routed_fallback_count = UINT64_MAX - 5u,
        .target_invalidation_count = UINT64_MAX - 6u,
        .full_invalidation_count = UINT64_MAX - 7u,
    };
    KitRuntimeDiagInputFrame frame = {
        .raw_event_count = 9u,
        .action_count = 1u,
        .text_entry_gate_active = true,
        .ignored_count = 5u,
        .routed_global_count = 9u,
        .routed_pane_count = 9u,
        .routed_fallback_count = 9u,
        .target_invalidation_count = 9u,
        .full_invalidation_count = 9u,
    };
    kit_runtime_diag_input_totals_accumulate(NULL, &frame);
    kit_runtime_diag_input_totals_accumulate(&totals, NULL);
    kit_runtime_diag_input_totals_accumulate(&totals, &frame);

    assert_u64(totals.raw_event_count, UINT64_MAX, "sat_raw_event_count");
    assert_u64(totals.action_count, UINT64_MAX, "sat_action_count");
    assert_u64(totals.shortcut_gated_count, UINT64_MAX, "sat_shortcut_gated_count");
    assert_u64(totals.routed_global_count, UINT64_MAX, "sat_routed_global_count");
    assert_u64(totals.routed_pane_count, UINT64_MAX, "sat_routed_pane_count");
    assert_u64(totals.routed_fallback_count, UINT64_MAX, "sat_routed_fallback_count");
    assert_u64(totals.target_invalidation_count, UINT64_MAX, "sat_target_invalidation_count");
    assert_u64(totals.full_invalidation_count, UINT64_MAX, "sat_full_invalidation_count");
}

int main(void) {
    test_duration_ms_raw_and_invalid_inputs();
    test_timing_compute();
    test_timing_compute_null_and_invalid_inputs();
    test_input_totals_accumulate();
    test_input_totals_null_and_saturation();
    printf("kit_runtime_diag_test: success\n");
    return 0;
}
