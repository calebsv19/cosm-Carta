#include "core_trace.h"

#include <stdio.h>
#include <string.h>

static int test_init_rejects_invalid_capacity(void) {
    CoreTraceSession s;
    CoreTraceConfig c = {0, 2};
    CoreResult r = core_trace_session_init(&s, &c);
    if (r.code == CORE_OK) {
        return 1;
    }
    return 0;
}

static int test_emit_and_finalize(void) {
    CoreTraceSession s;
    CoreTraceConfig c = {8, 4};
    CoreResult r = core_trace_session_init(&s, &c);
    const CoreTraceSampleF32 *samples = NULL;
    const CoreTraceMarker *markers = NULL;

    if (r.code != CORE_OK) return 1;

    r = core_trace_emit_sample_f32(&s, "dt", 0.016, 0.016f);
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }
    r = core_trace_emit_marker(&s, "state", 0.032, "reload");
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }

    if (core_trace_sample_count(&s) != 1 || core_trace_marker_count(&s) != 1) {
        core_trace_session_reset(&s);
        return 1;
    }

    samples = core_trace_samples(&s);
    markers = core_trace_markers(&s);
    if (!samples || !markers) {
        core_trace_session_reset(&s);
        return 1;
    }

    if (strcmp(samples[0].lane, "dt") != 0 || strcmp(markers[0].label, "reload") != 0) {
        core_trace_session_reset(&s);
        return 1;
    }

    r = core_trace_finalize(&s);
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }

    r = core_trace_emit_sample_f32(&s, "dt", 0.048, 0.016f);
    if (r.code == CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }

    core_trace_session_reset(&s);
    return 0;
}

static int test_capacity_overflow_counts(void) {
    CoreTraceSession s;
    CoreTraceConfig c = {2, 2};
    CoreResult r = core_trace_session_init(&s, &c);
    const CoreTraceStats *stats = NULL;
    const CoreTraceSampleF32 *samples = NULL;
    const CoreTraceMarker *markers = NULL;

    if (r.code != CORE_OK) return 1;

    r = core_trace_emit_sample_f32(&s, "dt", 0.0, 1.0f);
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }
    r = core_trace_emit_sample_f32(&s, "dt", 1.0, 2.0f);
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }
    r = core_trace_emit_sample_f32(&s, "dt", 2.0, 3.0f);
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }

    r = core_trace_emit_marker(&s, "event", 0.0, "start");
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }
    r = core_trace_emit_marker(&s, "event", 1.0, "end");
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }
    r = core_trace_emit_marker(&s, "event", 2.0, "loop");
    if (r.code != CORE_OK) {
        core_trace_session_reset(&s);
        return 1;
    }

    stats = core_trace_stats(&s);
    if (!stats || stats->sample_overflow_count != 1u || stats->marker_overflow_count != 1u) {
        core_trace_session_reset(&s);
        return 1;
    }
    if (core_trace_sample_count(&s) != 2u || core_trace_marker_count(&s) != 2u) {
        core_trace_session_reset(&s);
        return 1;
    }

    samples = core_trace_samples(&s);
    markers = core_trace_markers(&s);
    if (!samples || !markers) {
        core_trace_session_reset(&s);
        return 1;
    }
    if (samples[0].time_seconds != 1.0 || samples[1].time_seconds != 2.0) {
        core_trace_session_reset(&s);
        return 1;
    }
    if (strcmp(markers[0].label, "end") != 0 || strcmp(markers[1].label, "loop") != 0) {
        core_trace_session_reset(&s);
        return 1;
    }

    core_trace_session_reset(&s);
    return 0;
}

static int test_pack_roundtrip(void) {
    CoreTraceSession s;
    CoreTraceSession loaded;
    CoreTraceConfig c = {4, 4};
    CoreResult r = core_trace_session_init(&s, &c);
    const char *path = "/tmp/core_trace_roundtrip.pack";

    if (r.code != CORE_OK) return 1;
    memset(&loaded, 0, sizeof(loaded));

    r = core_trace_emit_sample_f32(&s, "dt", 0.016, 0.016f);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_sample_f32(&s, "density_avg", 0.016, 0.42f);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_marker(&s, "event", 0.02, "reload");
    if (r.code != CORE_OK) goto fail;
    r = core_trace_finalize(&s);
    if (r.code != CORE_OK) goto fail;

    r = core_trace_export_pack(&s, path);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_import_pack(path, &loaded);
    if (r.code != CORE_OK) goto fail;

    if (core_trace_sample_count(&loaded) != 2u || core_trace_marker_count(&loaded) != 1u) goto fail;
    if (strcmp(core_trace_samples(&loaded)[1].lane, "density_avg") != 0) goto fail;
    if (strcmp(core_trace_markers(&loaded)[0].label, "reload") != 0) goto fail;

    core_trace_session_reset(&loaded);
    core_trace_session_reset(&s);
    return 0;

fail:
    core_trace_session_reset(&loaded);
    core_trace_session_reset(&s);
    return 1;
}

int main(void) {
    if (test_init_rejects_invalid_capacity() != 0) return 1;
    if (test_emit_and_finalize() != 0) return 1;
    if (test_capacity_overflow_counts() != 0) return 1;
    if (test_pack_roundtrip() != 0) return 1;
    puts("core_trace tests passed");
    return 0;
}
