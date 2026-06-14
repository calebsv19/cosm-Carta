#include "core_sim_trace.h"

static CoreResult core_sim_trace_invalid_arg(const char *message) {
    CoreResult result = { CORE_ERR_INVALID_ARG, message };
    return result;
}

static CoreResult core_sim_trace_emit_reason_marker(CoreTraceSession *trace,
                                                    const CoreSimFrameRecord *record,
                                                    CoreSimFrameReason reason,
                                                    const char *label) {
    if ((record->reason_bits & (uint32_t)reason) == 0u) {
        return core_result_ok();
    }
    return core_trace_emit_marker(trace,
                                  CORE_SIM_TRACE_LANE_EVENT,
                                  record->simulation_time_after_seconds,
                                  label);
}

void core_sim_trace_frame_emit_options_defaults(CoreSimTraceFrameEmitOptions *options) {
    if (!options) {
        return;
    }

    options->emit_frame_marker = true;
    options->emit_reason_markers = true;
}

CoreResult core_sim_trace_emit_frame_record(CoreTraceSession *trace,
                                            const CoreSimFrameRecord *record,
                                            const CoreSimTraceFrameEmitOptions *options) {
    CoreSimTraceFrameEmitOptions resolved;
    CoreResult result;
    const double time_seconds = record ? record->simulation_time_after_seconds : 0.0;

    if (!trace || !record) {
        return core_sim_trace_invalid_arg("trace/record is null");
    }

    core_sim_trace_frame_emit_options_defaults(&resolved);
    if (options) {
        resolved = *options;
    }

    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_FRAME,
                                       time_seconds,
                                       (float)record->frame_index);
    if (result.code != CORE_OK) return result;
    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_DT,
                                       time_seconds,
                                       (float)record->input_dt_seconds);
    if (result.code != CORE_OK) return result;
    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_TICKS,
                                       time_seconds,
                                       (float)record->ticks_executed);
    if (result.code != CORE_OK) return result;
    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_PASSES,
                                       time_seconds,
                                       (float)record->passes_executed);
    if (result.code != CORE_OK) return result;
    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_REASON,
                                       time_seconds,
                                       (float)record->reason_bits);
    if (result.code != CORE_OK) return result;
    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_ACCUM,
                                       time_seconds,
                                       (float)record->accumulator_remaining_seconds);
    if (result.code != CORE_OK) return result;
    result = core_trace_emit_sample_f32(trace,
                                       CORE_SIM_TRACE_LANE_SIM_ADV,
                                       time_seconds,
                                       (float)record->simulation_time_advanced_seconds);
    if (result.code != CORE_OK) return result;

    if (resolved.emit_frame_marker) {
        result = core_trace_emit_marker(trace,
                                        CORE_SIM_TRACE_LANE_EVENT,
                                        time_seconds,
                                        "frame");
        if (result.code != CORE_OK) return result;
    }

    if (resolved.emit_reason_markers) {
        result = core_sim_trace_emit_reason_marker(trace,
                                                  record,
                                                  CORE_SIM_FRAME_REASON_TICK_EXECUTED,
                                                  "tick_executed");
        if (result.code != CORE_OK) return result;
        result = core_sim_trace_emit_reason_marker(trace,
                                                  record,
                                                  CORE_SIM_FRAME_REASON_RENDER_REQUESTED,
                                                  "render_requested");
        if (result.code != CORE_OK) return result;
        result = core_sim_trace_emit_reason_marker(trace,
                                                  record,
                                                  CORE_SIM_FRAME_REASON_MAX_TICK_CLAMP_HIT,
                                                  "max_tick_clamp_hit");
        if (result.code != CORE_OK) return result;
        result = core_sim_trace_emit_reason_marker(trace,
                                                  record,
                                                  CORE_SIM_FRAME_REASON_SINGLE_STEP_CONSUMED,
                                                  "single_step_consumed");
        if (result.code != CORE_OK) return result;
        result = core_sim_trace_emit_reason_marker(trace,
                                                  record,
                                                  CORE_SIM_FRAME_REASON_PASS_FAILED,
                                                  "pass_failed");
        if (result.code != CORE_OK) return result;
    }

    return core_result_ok();
}
