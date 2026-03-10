#ifndef CORE_TRACE_H
#define CORE_TRACE_H

#include <stdbool.h>
#include <stddef.h>

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_TRACE_LANE_NAME_MAX 32u
#define CORE_TRACE_MARKER_LABEL_MAX 64u

typedef struct CoreTraceConfig {
    size_t sample_capacity;
    size_t marker_capacity;
} CoreTraceConfig;

typedef struct CoreTraceSampleF32 {
    double time_seconds;
    float value;
    char lane[CORE_TRACE_LANE_NAME_MAX];
} CoreTraceSampleF32;

typedef struct CoreTraceMarker {
    double time_seconds;
    char lane[CORE_TRACE_LANE_NAME_MAX];
    char label[CORE_TRACE_MARKER_LABEL_MAX];
} CoreTraceMarker;

typedef struct CoreTraceStats {
    size_t samples_emitted;
    size_t markers_emitted;
    size_t sample_overflow_count;
    size_t marker_overflow_count;
} CoreTraceStats;

typedef struct CoreTraceSession {
    CoreTraceConfig config;
    CoreTraceSampleF32 *samples;
    CoreTraceMarker *markers;
    size_t sample_count;
    size_t marker_count;
    bool finalized;
    CoreTraceStats stats;
} CoreTraceSession;

CoreResult core_trace_session_init(CoreTraceSession *session, const CoreTraceConfig *config);
void core_trace_session_reset(CoreTraceSession *session);

CoreResult core_trace_emit_sample_f32(CoreTraceSession *session,
                                      const char *lane,
                                      double time_seconds,
                                      float value);

CoreResult core_trace_emit_marker(CoreTraceSession *session,
                                  const char *lane,
                                  double time_seconds,
                                  const char *label);

CoreResult core_trace_finalize(CoreTraceSession *session);
CoreResult core_trace_export_pack(const CoreTraceSession *session, const char *pack_path);
CoreResult core_trace_import_pack(const char *pack_path, CoreTraceSession *out_session);

const CoreTraceStats *core_trace_stats(const CoreTraceSession *session);

size_t core_trace_sample_count(const CoreTraceSession *session);
size_t core_trace_marker_count(const CoreTraceSession *session);
const CoreTraceSampleF32 *core_trace_samples(const CoreTraceSession *session);
const CoreTraceMarker *core_trace_markers(const CoreTraceSession *session);

#ifdef __cplusplus
}
#endif

#endif
