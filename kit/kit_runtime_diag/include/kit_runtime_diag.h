#ifndef KIT_RUNTIME_DIAG_H
#define KIT_RUNTIME_DIAG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct KitRuntimeDiagStageMarks {
    double frame_begin;
    double after_events;
    double after_update;
    double after_queue;
    double after_integrate;
    double after_route;
    double after_render_derive;
    double before_present;
    double after_render;
} KitRuntimeDiagStageMarks;

typedef struct KitRuntimeDiagTimings {
    double frame_ms;
    double events_ms;
    double update_ms;
    double queue_ms;
    double integrate_ms;
    double route_ms;
    double render_ms;
    double present_ms;
    double render_derive_ms;
    double render_submit_ms;
} KitRuntimeDiagTimings;

typedef struct KitRuntimeDiagInputFrame {
    uint32_t raw_event_count;
    uint32_t action_count;
    bool text_entry_gate_active;
    uint32_t ignored_count;
    uint32_t routed_global_count;
    uint32_t routed_pane_count;
    uint32_t routed_fallback_count;
    uint32_t target_invalidation_count;
    uint32_t full_invalidation_count;
    uint32_t invalidation_reason_bits;
} KitRuntimeDiagInputFrame;

typedef struct KitRuntimeDiagInputTotals {
    uint64_t raw_event_count;
    uint64_t action_count;
    uint64_t shortcut_gated_count;
    uint64_t routed_global_count;
    uint64_t routed_pane_count;
    uint64_t routed_fallback_count;
    uint64_t target_invalidation_count;
    uint64_t full_invalidation_count;
} KitRuntimeDiagInputTotals;

double kit_runtime_diag_duration_ms(double start_time, double end_time);

void kit_runtime_diag_compute_timings(const KitRuntimeDiagStageMarks *marks,
                                      KitRuntimeDiagTimings *out_timings);

void kit_runtime_diag_input_totals_accumulate(KitRuntimeDiagInputTotals *io_totals,
                                              const KitRuntimeDiagInputFrame *frame);

#endif
