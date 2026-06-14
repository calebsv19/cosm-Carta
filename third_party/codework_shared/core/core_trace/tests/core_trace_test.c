#include "core_trace.h"
#include "core_pack.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct TestTraceHeaderV1 {
    uint32_t trace_profile_version;
    uint32_t reserved;
    uint64_t sample_count;
    uint64_t marker_count;
} TestTraceHeaderV1;

static void make_overlong_string(char *dst, size_t length, char fill) {
    size_t i = 0;
    for (i = 0; i < length; ++i) {
        dst[i] = fill;
    }
    dst[length] = '\0';
}

static int write_test_pack(const char *path,
                           int include_trhd,
                           const void *trhd_data,
                           uint64_t trhd_size,
                           int include_trsm,
                           const void *trsm_data,
                           uint64_t trsm_size,
                           int include_trev,
                           const void *trev_data,
                           uint64_t trev_size,
                           int include_extra) {
    CorePackWriter writer;
    CoreResult r;
    char extra_payload[4] = {'E', 'X', 'T', 'R'};

    memset(&writer, 0, sizeof(writer));
    r = core_pack_writer_open(path, &writer);
    if (r.code != CORE_OK) return 1;

    if (include_trhd) {
        r = core_pack_writer_add_chunk(&writer, "TRHD", trhd_data, trhd_size);
        if (r.code != CORE_OK) goto fail;
    }
    if (include_trsm) {
        r = core_pack_writer_add_chunk(&writer, "TRSM", trsm_data, trsm_size);
        if (r.code != CORE_OK) goto fail;
    }
    if (include_trev) {
        r = core_pack_writer_add_chunk(&writer, "TREV", trev_data, trev_size);
        if (r.code != CORE_OK) goto fail;
    }
    if (include_extra) {
        r = core_pack_writer_add_chunk(&writer, "EXTR", extra_payload, (uint64_t)sizeof(extra_payload));
        if (r.code != CORE_OK) goto fail;
    }

    r = core_pack_writer_close(&writer);
    return (r.code == CORE_OK) ? 0 : 1;

fail:
    (void)core_pack_writer_close(&writer);
    return 1;
}

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

static int test_full_buffer_invalid_emit_does_not_mutate(void) {
    CoreTraceSession s;
    CoreTraceConfig c = {2, 2};
    CoreResult r = core_trace_session_init(&s, &c);
    const CoreTraceStats *stats = NULL;
    char long_lane[CORE_TRACE_LANE_NAME_MAX + 1];
    char long_label[CORE_TRACE_MARKER_LABEL_MAX + 1];

    if (r.code != CORE_OK) return 1;

    make_overlong_string(long_lane, CORE_TRACE_LANE_NAME_MAX, 'L');
    make_overlong_string(long_label, CORE_TRACE_MARKER_LABEL_MAX, 'M');

    r = core_trace_emit_sample_f32(&s, "dt", 0.0, 1.0f);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_sample_f32(&s, "dt", 1.0, 2.0f);
    if (r.code != CORE_OK) goto fail;

    r = core_trace_emit_sample_f32(&s, long_lane, 2.0, 3.0f);
    if (r.code == CORE_OK) goto fail;

    stats = core_trace_stats(&s);
    if (!stats || stats->sample_overflow_count != 0u) goto fail;
    if (core_trace_sample_count(&s) != 2u) goto fail;
    if (core_trace_samples(&s)[0].time_seconds != 0.0 || core_trace_samples(&s)[1].time_seconds != 1.0) goto fail;

    r = core_trace_emit_marker(&s, "event", 0.0, "start");
    if (r.code != CORE_OK) goto fail;
    r = core_trace_emit_marker(&s, "event", 1.0, "end");
    if (r.code != CORE_OK) goto fail;

    r = core_trace_emit_marker(&s, long_lane, 2.0, "loop");
    if (r.code == CORE_OK) goto fail;
    stats = core_trace_stats(&s);
    if (!stats || stats->marker_overflow_count != 0u) goto fail;
    if (core_trace_marker_count(&s) != 2u) goto fail;
    if (strcmp(core_trace_markers(&s)[0].label, "start") != 0 ||
        strcmp(core_trace_markers(&s)[1].label, "end") != 0) goto fail;

    r = core_trace_emit_marker(&s, "event", 2.0, long_label);
    if (r.code == CORE_OK) goto fail;
    stats = core_trace_stats(&s);
    if (!stats || stats->marker_overflow_count != 0u) goto fail;
    if (core_trace_marker_count(&s) != 2u) goto fail;
    if (strcmp(core_trace_markers(&s)[0].label, "start") != 0 ||
        strcmp(core_trace_markers(&s)[1].label, "end") != 0) goto fail;

    core_trace_session_reset(&s);
    return 0;

fail:
    core_trace_session_reset(&s);
    return 1;
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

    r = core_trace_export_pack(&s, path);
    if (r.code == CORE_OK) goto fail;

    r = core_trace_finalize(&s);
    if (r.code != CORE_OK) goto fail;

    r = core_trace_export_pack(&s, path);
    if (r.code != CORE_OK) goto fail;
    r = core_trace_import_pack(path, &loaded);
    if (r.code != CORE_OK) goto fail;

    if (core_trace_sample_count(&loaded) != 2u || core_trace_marker_count(&loaded) != 1u) goto fail;
    if (strcmp(core_trace_samples(&loaded)[1].lane, "density_avg") != 0) goto fail;
    if (strcmp(core_trace_markers(&loaded)[0].label, "reload") != 0) goto fail;
    r = core_trace_emit_sample_f32(&loaded, "dt", 0.03, 0.1f);
    if (r.code == CORE_OK) goto fail;
    r = core_trace_emit_marker(&loaded, "event", 0.04, "mutate");
    if (r.code == CORE_OK) goto fail;
    r = core_trace_export_pack(&loaded, path);
    if (r.code != CORE_OK) goto fail;

    core_trace_session_reset(&loaded);
    core_trace_session_reset(&s);
    return 0;

fail:
    core_trace_session_reset(&loaded);
    core_trace_session_reset(&s);
    return 1;
}

static int test_pack_import_malformed_cases(void) {
    const char *missing_trhd = "/tmp/core_trace_missing_trhd.pack";
    const char *invalid_trhd = "/tmp/core_trace_invalid_trhd.pack";
    const char *unsupported_version = "/tmp/core_trace_unsupported_version.pack";
    const char *missing_trsm = "/tmp/core_trace_missing_trsm.pack";
    const char *invalid_trsm = "/tmp/core_trace_invalid_trsm.pack";
    const char *missing_trev = "/tmp/core_trace_missing_trev.pack";
    const char *invalid_trev = "/tmp/core_trace_invalid_trev.pack";
    const char *unknown_extra = "/tmp/core_trace_unknown_extra.pack";
    const char *oversized_counts = "/tmp/core_trace_oversized_counts.pack";
    TestTraceHeaderV1 header = {1u, 0u, 0u, 0u};
    CoreTraceSampleF32 sample = {0};
    CoreTraceMarker marker = {0};
    uint8_t one_byte = 0u;
    CoreTraceSession loaded;
    CoreResult r;

    sample.time_seconds = 0.016;
    sample.value = 1.0f;
    (void)strcpy(sample.lane, "dt");
    marker.time_seconds = 0.032;
    (void)strcpy(marker.lane, "event");
    (void)strcpy(marker.label, "start");

    if (write_test_pack(missing_trhd, 0, NULL, 0, 0, NULL, 0, 0, NULL, 0, 0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(missing_trhd, &loaded);
    if (r.code == CORE_OK) return 1;

    if (write_test_pack(invalid_trhd,
                        1, &header, (uint64_t)(sizeof(header) - 1u),
                        0, NULL, 0,
                        0, NULL, 0,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(invalid_trhd, &loaded);
    if (r.code == CORE_OK) return 1;

    header.trace_profile_version = 2u;
    if (write_test_pack(unsupported_version,
                        1, &header, (uint64_t)sizeof(header),
                        0, NULL, 0,
                        0, NULL, 0,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(unsupported_version, &loaded);
    if (r.code == CORE_OK) return 1;

    header.trace_profile_version = 1u;
    header.sample_count = 1u;
    header.marker_count = 0u;
    if (write_test_pack(missing_trsm,
                        1, &header, (uint64_t)sizeof(header),
                        0, NULL, 0,
                        0, NULL, 0,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(missing_trsm, &loaded);
    if (r.code == CORE_OK) return 1;

    if (write_test_pack(invalid_trsm,
                        1, &header, (uint64_t)sizeof(header),
                        1, &one_byte, 1u,
                        0, NULL, 0,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(invalid_trsm, &loaded);
    if (r.code == CORE_OK) return 1;

    header.sample_count = 0u;
    header.marker_count = 1u;
    if (write_test_pack(missing_trev,
                        1, &header, (uint64_t)sizeof(header),
                        0, NULL, 0,
                        0, NULL, 0,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(missing_trev, &loaded);
    if (r.code == CORE_OK) return 1;

    if (write_test_pack(invalid_trev,
                        1, &header, (uint64_t)sizeof(header),
                        0, NULL, 0,
                        1, &one_byte, 1u,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(invalid_trev, &loaded);
    if (r.code == CORE_OK) return 1;

    header.sample_count = 0u;
    header.marker_count = 0u;
    if (write_test_pack(unknown_extra,
                        1, &header, (uint64_t)sizeof(header),
                        0, NULL, 0,
                        0, NULL, 0,
                        1) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(unknown_extra, &loaded);
    if (r.code != CORE_OK) return 1;
    if (core_trace_sample_count(&loaded) != 0u || core_trace_marker_count(&loaded) != 0u) {
        core_trace_session_reset(&loaded);
        return 1;
    }
    core_trace_session_reset(&loaded);

    header.sample_count = UINT64_MAX;
    header.marker_count = 0u;
    if (write_test_pack(oversized_counts,
                        1, &header, (uint64_t)sizeof(header),
                        0, NULL, 0,
                        0, NULL, 0,
                        0) != 0) return 1;
    memset(&loaded, 0, sizeof(loaded));
    r = core_trace_import_pack(oversized_counts, &loaded);
    if (r.code == CORE_OK) return 1;

    return 0;
}

int main(void) {
    if (test_init_rejects_invalid_capacity() != 0) return 1;
    if (test_emit_and_finalize() != 0) return 1;
    if (test_capacity_overflow_counts() != 0) return 1;
    if (test_full_buffer_invalid_emit_does_not_mutate() != 0) return 1;
    if (test_pack_roundtrip() != 0) return 1;
    if (test_pack_import_malformed_cases() != 0) return 1;
    puts("core_trace tests passed");
    return 0;
}
