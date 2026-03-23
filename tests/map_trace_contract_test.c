#include "core_pack.h"
#include "core_trace.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool has_lane(const CoreTraceSession *session, const char *lane) {
    size_t count = core_trace_sample_count(session);
    const CoreTraceSampleF32 *samples = core_trace_samples(session);
    if (!session || !lane || !samples) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(samples[i].lane, lane) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_marker(const CoreTraceSession *session, const char *label) {
    size_t count = core_trace_marker_count(session);
    const CoreTraceMarker *markers = core_trace_markers(session);
    if (!session || !label || !markers) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(markers[i].label, label) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_marker_in_lane(const CoreTraceSession *session, const char *lane, const char *label) {
    size_t count = core_trace_marker_count(session);
    const CoreTraceMarker *markers = core_trace_markers(session);
    if (!session || !lane || !label || !markers) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(markers[i].lane, lane) == 0 && strcmp(markers[i].label, label) == 0) {
            return true;
        }
    }
    return false;
}

int main(void) {
    const char *lanes[] = {
        "frame", "events", "update", "queue", "integrate", "route", "render", "present"
    };
    const char *queue_markers[] = {
        "tile_enq_drop", "tile_enq_evict", "tile_res_drop",
        "tile_res_evict", "vk_job_drop", "vk_job_evict",
        "vk_stage_drop", "vk_stage_evict"
    };

    char pack_path[] = "/tmp/mapforge_trace_contract_XXXXXX";
    CoreTraceSession session;
    CoreTraceSession loaded;
    CoreTraceConfig cfg = {128u, 64u};
    CoreResult r;
    CorePackReader reader;
    CorePackChunkInfo chunk;
    size_t i = 0;
    int fd = mkstemp(pack_path);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed\n");
        return 1;
    }
    close(fd);
    remove(pack_path);
    memset(&session, 0, sizeof(session));
    memset(&loaded, 0, sizeof(loaded));
    memset(&reader, 0, sizeof(reader));
    memset(&chunk, 0, sizeof(chunk));

    r = core_trace_session_init(&session, &cfg);
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_trace_session_init failed: %s\n", r.message ? r.message : "");
        return 1;
    }

    for (i = 0; i < sizeof(lanes) / sizeof(lanes[0]); ++i) {
        r = core_trace_emit_sample_f32(&session, lanes[i], 0.01 * (double)(i + 1u), (float)(i + 1u));
        if (r.code != CORE_OK) {
            fprintf(stderr, "core_trace_emit_sample_f32 failed: %s\n", r.message ? r.message : "");
            core_trace_session_reset(&session);
            return 1;
        }
    }
    r = core_trace_emit_marker(&session, "lifecycle", 0.0, "trace_start");
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_trace_emit_marker trace_start failed: %s\n", r.message ? r.message : "");
        core_trace_session_reset(&session);
        return 1;
    }
    for (i = 0; i < sizeof(queue_markers) / sizeof(queue_markers[0]); ++i) {
        r = core_trace_emit_marker(&session, "queue", 0.1 + 0.01 * (double)i, queue_markers[i]);
        if (r.code != CORE_OK) {
            fprintf(stderr, "core_trace_emit_marker failed: %s\n", r.message ? r.message : "");
            core_trace_session_reset(&session);
            return 1;
        }
    }
    r = core_trace_emit_marker(&session, "lifecycle", 0.5, "trace_end");
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_trace_emit_marker trace_end failed: %s\n", r.message ? r.message : "");
        core_trace_session_reset(&session);
        return 1;
    }

    r = core_trace_finalize(&session);
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_trace_finalize failed: %s\n", r.message ? r.message : "");
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_trace_export_pack(&session, pack_path);
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_trace_export_pack failed: %s\n", r.message ? r.message : "");
        core_trace_session_reset(&session);
        return 1;
    }

    r = core_pack_reader_open(pack_path, &reader);
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_pack_reader_open failed: %s\n", r.message ? r.message : "");
        core_trace_session_reset(&session);
        return 1;
    }
    if (core_pack_reader_chunk_count(&reader) != 3u) {
        fprintf(stderr, "unexpected chunk count: %zu\n", core_pack_reader_chunk_count(&reader));
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_pack_reader_get_chunk(&reader, 0u, &chunk);
    if (r.code != CORE_OK || strcmp(chunk.type, "TRHD") != 0) {
        fprintf(stderr, "chunk[0] mismatch (expected TRHD)\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_pack_reader_get_chunk(&reader, 1u, &chunk);
    if (r.code != CORE_OK || strcmp(chunk.type, "TRSM") != 0) {
        fprintf(stderr, "chunk[1] mismatch (expected TRSM)\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    if (chunk.size != (uint64_t)((sizeof(lanes) / sizeof(lanes[0])) * sizeof(CoreTraceSampleF32))) {
        fprintf(stderr, "TRSM size mismatch\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_pack_reader_get_chunk(&reader, 2u, &chunk);
    if (r.code != CORE_OK || strcmp(chunk.type, "TREV") != 0) {
        fprintf(stderr, "chunk[2] mismatch (expected TREV)\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    if (chunk.size != (uint64_t)(((sizeof(queue_markers) / sizeof(queue_markers[0])) + 2u) * sizeof(CoreTraceMarker))) {
        fprintf(stderr, "TREV size mismatch\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_pack_reader_find_chunk(&reader, "TRHD", 0u, &chunk);
    if (r.code != CORE_OK) {
        fprintf(stderr, "missing TRHD chunk\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_pack_reader_find_chunk(&reader, "TRSM", 0u, &chunk);
    if (r.code != CORE_OK) {
        fprintf(stderr, "missing TRSM chunk\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    r = core_pack_reader_find_chunk(&reader, "TREV", 0u, &chunk);
    if (r.code != CORE_OK) {
        fprintf(stderr, "missing TREV chunk\n");
        core_pack_reader_close(&reader);
        core_trace_session_reset(&session);
        return 1;
    }
    core_pack_reader_close(&reader);

    r = core_trace_import_pack(pack_path, &loaded);
    if (r.code != CORE_OK) {
        fprintf(stderr, "core_trace_import_pack failed: %s\n", r.message ? r.message : "");
        core_trace_session_reset(&session);
        return 1;
    }

    if (core_trace_sample_count(&loaded) != (sizeof(lanes) / sizeof(lanes[0]))) {
        fprintf(stderr, "sample count mismatch\n");
        core_trace_session_reset(&loaded);
        core_trace_session_reset(&session);
        return 1;
    }
    if (core_trace_marker_count(&loaded) != ((sizeof(queue_markers) / sizeof(queue_markers[0])) + 2u)) {
        fprintf(stderr, "marker count mismatch\n");
        core_trace_session_reset(&loaded);
        core_trace_session_reset(&session);
        return 1;
    }
    for (i = 0; i < sizeof(lanes) / sizeof(lanes[0]); ++i) {
        if (!has_lane(&loaded, lanes[i])) {
            fprintf(stderr, "missing lane: %s\n", lanes[i]);
            core_trace_session_reset(&loaded);
            core_trace_session_reset(&session);
            return 1;
        }
    }
    for (i = 0; i < sizeof(queue_markers) / sizeof(queue_markers[0]); ++i) {
        if (!has_marker(&loaded, queue_markers[i])) {
            fprintf(stderr, "missing marker: %s\n", queue_markers[i]);
            core_trace_session_reset(&loaded);
            core_trace_session_reset(&session);
            return 1;
        }
    }
    if (!has_marker_in_lane(&loaded, "lifecycle", "trace_start")) {
        fprintf(stderr, "missing lifecycle marker: trace_start\n");
        core_trace_session_reset(&loaded);
        core_trace_session_reset(&session);
        return 1;
    }
    if (!has_marker_in_lane(&loaded, "lifecycle", "trace_end")) {
        fprintf(stderr, "missing lifecycle marker: trace_end\n");
        core_trace_session_reset(&loaded);
        core_trace_session_reset(&session);
        return 1;
    }

    core_trace_session_reset(&loaded);
    core_trace_session_reset(&session);
    remove(pack_path);
    puts("map_trace_contract_test: success");
    return 0;
}
