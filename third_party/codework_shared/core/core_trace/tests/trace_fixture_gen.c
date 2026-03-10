#include "core_trace.h"

#include <stdio.h>

int main(int argc, char **argv) {
    const char *out_path = "tests/fixtures/trace_v1_sample.pack";
    CoreTraceSession session;
    CoreTraceConfig cfg;
    CoreResult r;

    if (argc >= 2 && argv[1] && argv[1][0] != '\0') {
        out_path = argv[1];
    }

    cfg.sample_capacity = 16;
    cfg.marker_capacity = 8;

    r = core_trace_session_init(&session, &cfg);
    if (r.code != CORE_OK) {
        fprintf(stderr, "init failed: %s\n", r.message);
        return 1;
    }

    r = core_trace_emit_sample_f32(&session, "dt", 0.016, 0.016f);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_sample_f32(&session, "dt", 0.032, 0.016f);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_sample_f32(&session, "density_avg", 0.032, 0.42f);
    if (r.code != CORE_OK) goto fail;

    r = core_trace_emit_marker(&session, "event", 0.0, "start");
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_marker(&session, "event", 0.048, "checkpoint");
    if (r.code != CORE_OK) goto fail;

    r = core_trace_finalize(&session);
    if (r.code != CORE_OK) goto fail;

    r = core_trace_export_pack(&session, out_path);
    if (r.code != CORE_OK) goto fail;

    core_trace_session_reset(&session);
    return 0;

fail:
    fprintf(stderr, "trace fixture generation failed: %s\n", r.message);
    core_trace_session_reset(&session);
    return 1;
}
