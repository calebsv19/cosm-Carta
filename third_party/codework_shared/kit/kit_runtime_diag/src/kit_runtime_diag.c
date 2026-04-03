#include "kit_runtime_diag.h"

double kit_runtime_diag_duration_ms(double start_time, double end_time) {
    return (end_time - start_time) * 1000.0;
}

void kit_runtime_diag_compute_timings(const KitRuntimeDiagStageMarks *marks,
                                      KitRuntimeDiagTimings *out_timings) {
    static const KitRuntimeDiagTimings k_zero = {0};
    if (!out_timings) {
        return;
    }
    *out_timings = k_zero;
    if (!marks) {
        return;
    }

    out_timings->frame_ms = kit_runtime_diag_duration_ms(marks->frame_begin, marks->after_render);
    out_timings->events_ms = kit_runtime_diag_duration_ms(marks->frame_begin, marks->after_events);
    out_timings->update_ms = kit_runtime_diag_duration_ms(marks->after_events, marks->after_update);
    out_timings->queue_ms = kit_runtime_diag_duration_ms(marks->after_update, marks->after_queue);
    out_timings->integrate_ms = kit_runtime_diag_duration_ms(marks->after_queue, marks->after_integrate);
    out_timings->route_ms = kit_runtime_diag_duration_ms(marks->after_integrate, marks->after_route);
    out_timings->render_ms = kit_runtime_diag_duration_ms(marks->after_route, marks->before_present);
    out_timings->present_ms = kit_runtime_diag_duration_ms(marks->before_present, marks->after_render);
    out_timings->render_derive_ms = kit_runtime_diag_duration_ms(marks->after_route, marks->after_render_derive);
    out_timings->render_submit_ms = kit_runtime_diag_duration_ms(marks->after_render_derive, marks->before_present);
}

void kit_runtime_diag_input_totals_accumulate(KitRuntimeDiagInputTotals *io_totals,
                                              const KitRuntimeDiagInputFrame *frame) {
    if (!io_totals || !frame) {
        return;
    }

    io_totals->raw_event_count += frame->raw_event_count;
    io_totals->action_count += frame->action_count;
    io_totals->routed_global_count += frame->routed_global_count;
    io_totals->routed_pane_count += frame->routed_pane_count;
    io_totals->routed_fallback_count += frame->routed_fallback_count;
    io_totals->target_invalidation_count += frame->target_invalidation_count;
    io_totals->full_invalidation_count += frame->full_invalidation_count;
    if (frame->text_entry_gate_active) {
        io_totals->shortcut_gated_count += frame->ignored_count;
    }
}
