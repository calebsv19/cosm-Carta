#include "core_sim_trace.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int test_defaults(void) {
    CoreSimTraceFrameEmitOptions options;

    options.emit_frame_marker = false;
    options.emit_reason_markers = false;
    core_sim_trace_frame_emit_options_defaults(&options);
    core_sim_trace_frame_emit_options_defaults(NULL);
    return expect_true(options.emit_frame_marker, "frame marker default") &&
           expect_true(options.emit_reason_markers, "reason marker default");
}

static int test_emit_frame_record(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 16u, 8u };
    CoreResult result;
    CoreSimFrameRecord record;
    const CoreTraceSampleF32 *samples = NULL;
    const CoreTraceMarker *markers = NULL;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));

    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init")) return 0;

    record.frame_index = 7u;
    record.input_dt_seconds = 0.016;
    record.simulation_time_after_seconds = 1.25;
    record.simulation_time_advanced_seconds = 0.032;
    record.accumulator_remaining_seconds = 0.004;
    record.ticks_executed = 2u;
    record.passes_executed = 6u;
    record.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED |
                         CORE_SIM_FRAME_REASON_RENDER_REQUESTED;
    record.status_name = "ok";

    result = core_sim_trace_emit_frame_record(&trace, &record, NULL);
    if (!expect_true(result.code == CORE_OK, "emit frame record")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 7u, "sample count")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_marker_count(&trace) == 3u, "marker count")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    samples = core_trace_samples(&trace);
    markers = core_trace_markers(&trace);
    if (!expect_true(samples && markers, "trace storage")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(samples[0].lane, CORE_SIM_TRACE_LANE_FRAME) == 0,
                     "frame lane")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(samples[2].lane, CORE_SIM_TRACE_LANE_TICKS) == 0 &&
                         samples[2].value == 2.0f,
                     "ticks lane")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(markers[0].label, "frame") == 0 &&
                         strcmp(markers[1].label, "tick_executed") == 0 &&
                         strcmp(markers[2].label, "render_requested") == 0,
                     "marker labels")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

static int test_emit_without_markers(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 16u, 8u };
    CoreSimTraceFrameEmitOptions options;
    CoreSimFrameRecord record;
    CoreResult result;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));
    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init no markers")) return 0;

    options.emit_frame_marker = false;
    options.emit_reason_markers = false;
    record.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED |
                         CORE_SIM_FRAME_REASON_RENDER_REQUESTED |
                         CORE_SIM_FRAME_REASON_MAX_TICK_CLAMP_HIT;

    result = core_sim_trace_emit_frame_record(&trace, &record, &options);
    if (!expect_true(result.code == CORE_OK, "emit no markers")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 7u, "no marker sample count") ||
        !expect_true(core_trace_marker_count(&trace) == 0u, "no marker marker count")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

static int test_invalid_args_and_finalized_session(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 16u, 8u };
    CoreSimFrameRecord record;
    CoreResult result;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));

    result = core_sim_trace_emit_frame_record(NULL, &record, NULL);
    if (!expect_true(result.code == CORE_ERR_INVALID_ARG, "null trace invalid arg")) return 0;
    result = core_sim_trace_emit_frame_record(&trace, NULL, NULL);
    if (!expect_true(result.code == CORE_ERR_INVALID_ARG, "null record invalid arg")) return 0;

    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init finalized")) return 0;

    record.simulation_time_after_seconds = 2.0;
    result = core_trace_finalize(&trace);
    if (!expect_true(result.code == CORE_OK, "trace finalize")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    result = core_sim_trace_emit_frame_record(&trace, &record, NULL);
    if (!expect_true(result.code != CORE_OK, "emit after finalize fails")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 0u, "finalized sample count unchanged") ||
        !expect_true(core_trace_marker_count(&trace) == 0u, "finalized marker count unchanged")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

static int test_unknown_reason_bits_do_not_emit_unknown_markers(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 16u, 8u };
    CoreSimFrameRecord record;
    CoreResult result;
    const CoreTraceMarker *markers = NULL;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));

    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init unknown reason")) return 0;

    record.simulation_time_after_seconds = 3.0;
    record.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED | 0x80000000u;

    result = core_sim_trace_emit_frame_record(&trace, &record, NULL);
    if (!expect_true(result.code == CORE_OK, "emit unknown reason")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 7u, "unknown reason sample count")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_marker_count(&trace) == 2u, "unknown reason marker count")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    markers = core_trace_markers(&trace);
    if (!expect_true(markers != NULL, "unknown reason markers storage")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(markers[0].label, "frame") == 0, "unknown reason frame marker") ||
        !expect_true(strcmp(markers[1].label, "tick_executed") == 0, "unknown reason known marker")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

static int test_bounded_retention_overflow_semantics(void) {
    CoreTraceSession trace;
    CoreTraceConfig config = { 7u, 1u };
    CoreSimFrameRecord record;
    CoreResult result;
    const CoreTraceMarker *markers = NULL;
    const CoreTraceStats *stats = NULL;

    memset(&trace, 0, sizeof(trace));
    memset(&record, 0, sizeof(record));

    result = core_trace_session_init(&trace, &config);
    if (!expect_true(result.code == CORE_OK, "trace init partial")) return 0;

    record.simulation_time_after_seconds = 4.0;
    record.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED |
                         CORE_SIM_FRAME_REASON_RENDER_REQUESTED;

    result = core_sim_trace_emit_frame_record(&trace, &record, NULL);
    if (!expect_true(result.code == CORE_OK, "bounded retention emit succeeds")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_sample_count(&trace) == 7u, "bounded retention samples retained")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(core_trace_marker_count(&trace) == 1u, "bounded retention marker count")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    markers = core_trace_markers(&trace);
    stats = core_trace_stats(&trace);
    if (!expect_true(markers != NULL, "bounded retention markers storage") ||
        !expect_true(stats != NULL, "bounded retention stats")) {
        core_trace_session_reset(&trace);
        return 0;
    }
    if (!expect_true(strcmp(markers[0].label, "render_requested") == 0,
                     "bounded retention keeps newest marker") ||
        !expect_true(stats->marker_overflow_count == 2u,
                     "bounded retention marker overflow counted")) {
        core_trace_session_reset(&trace);
        return 0;
    }

    core_trace_session_reset(&trace);
    return 1;
}

int main(void) {
    if (!test_defaults()) return 1;
    if (!test_emit_frame_record()) return 1;
    if (!test_emit_without_markers()) return 1;
    if (!test_invalid_args_and_finalized_session()) return 1;
    if (!test_unknown_reason_bits_do_not_emit_unknown_markers()) return 1;
    if (!test_bounded_retention_overflow_semantics()) return 1;

    puts("core_sim_trace_test: ok");
    return 0;
}
