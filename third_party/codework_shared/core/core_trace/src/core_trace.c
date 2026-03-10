/*
 * core_trace.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_trace.h"
#include "core_pack.h"

#include <string.h>

static CoreResult invalid_arg(const char *msg) {
    CoreResult r = { CORE_ERR_INVALID_ARG, msg };
    return r;
}

static bool copy_bounded(char *dst, size_t dst_size, const char *src) {
    size_t n = 0;
    if (!dst || !src || dst_size == 0) return false;
    n = strlen(src);
    if (n >= dst_size) return false;
    memcpy(dst, src, n + 1);
    return true;
}

typedef struct CoreTraceHeaderV1 {
    uint32_t trace_profile_version;
    uint32_t reserved;
    uint64_t sample_count;
    uint64_t marker_count;
} CoreTraceHeaderV1;

CoreResult core_trace_session_init(CoreTraceSession *session, const CoreTraceConfig *config) {
    CoreTraceConfig local = {0};

    if (!session || !config) return invalid_arg("session/config is null");
    if (config->sample_capacity == 0 || config->marker_capacity == 0) {
        return invalid_arg("capacities must be > 0");
    }

    memset(session, 0, sizeof(*session));
    local = *config;

    session->samples = (CoreTraceSampleF32 *)core_calloc(local.sample_capacity, sizeof(CoreTraceSampleF32));
    if (!session->samples) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "failed to allocate samples" };
        return r;
    }

    session->markers = (CoreTraceMarker *)core_calloc(local.marker_capacity, sizeof(CoreTraceMarker));
    if (!session->markers) {
        core_free(session->samples);
        session->samples = NULL;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "failed to allocate markers" };
        return r;
    }

    session->config = local;
    return core_result_ok();
}

void core_trace_session_reset(CoreTraceSession *session) {
    if (!session) return;
    core_free(session->samples);
    core_free(session->markers);
    memset(session, 0, sizeof(*session));
}

CoreResult core_trace_emit_sample_f32(CoreTraceSession *session,
                                      const char *lane,
                                      double time_seconds,
                                      float value) {
    CoreTraceSampleF32 *slot = NULL;

    if (!session || !lane) return invalid_arg("session/lane is null");
    if (session->finalized) return invalid_arg("session is finalized");
    if (session->sample_count >= session->config.sample_capacity) {
        memmove(session->samples,
                session->samples + 1,
                (session->sample_count - 1u) * sizeof(CoreTraceSampleF32));
        session->sample_count -= 1u;
        session->stats.sample_overflow_count += 1u;
    }

    slot = &session->samples[session->sample_count];
    if (!copy_bounded(slot->lane, sizeof(slot->lane), lane)) {
        return invalid_arg("lane name too long");
    }

    slot->time_seconds = time_seconds;
    slot->value = value;
    session->sample_count += 1u;
    session->stats.samples_emitted += 1u;
    return core_result_ok();
}

CoreResult core_trace_emit_marker(CoreTraceSession *session,
                                  const char *lane,
                                  double time_seconds,
                                  const char *label) {
    CoreTraceMarker *slot = NULL;

    if (!session || !lane || !label) return invalid_arg("session/lane/label is null");
    if (session->finalized) return invalid_arg("session is finalized");
    if (session->marker_count >= session->config.marker_capacity) {
        memmove(session->markers,
                session->markers + 1,
                (session->marker_count - 1u) * sizeof(CoreTraceMarker));
        session->marker_count -= 1u;
        session->stats.marker_overflow_count += 1u;
    }

    slot = &session->markers[session->marker_count];
    if (!copy_bounded(slot->lane, sizeof(slot->lane), lane)) {
        return invalid_arg("lane name too long");
    }
    if (!copy_bounded(slot->label, sizeof(slot->label), label)) {
        return invalid_arg("marker label too long");
    }

    slot->time_seconds = time_seconds;
    session->marker_count += 1u;
    session->stats.markers_emitted += 1u;
    return core_result_ok();
}

CoreResult core_trace_finalize(CoreTraceSession *session) {
    if (!session) return invalid_arg("session is null");
    session->finalized = true;
    return core_result_ok();
}

CoreResult core_trace_export_pack(const CoreTraceSession *session, const char *pack_path) {
    CoreTraceHeaderV1 h;
    CorePackWriter w;
    CoreResult r;

    if (!session || !pack_path) return invalid_arg("session/pack_path is null");

    memset(&h, 0, sizeof(h));
    memset(&w, 0, sizeof(w));
    h.trace_profile_version = 1u;
    h.sample_count = session->sample_count;
    h.marker_count = session->marker_count;

    r = core_pack_writer_open(pack_path, &w);
    if (r.code != CORE_OK) return r;

    r = core_pack_writer_add_chunk(&w, "TRHD", &h, (uint64_t)sizeof(h));
    if (r.code == CORE_OK && session->sample_count > 0u) {
        r = core_pack_writer_add_chunk(&w,
                                       "TRSM",
                                       session->samples,
                                       (uint64_t)(session->sample_count * sizeof(CoreTraceSampleF32)));
    }
    if (r.code == CORE_OK && session->marker_count > 0u) {
        r = core_pack_writer_add_chunk(&w,
                                       "TREV",
                                       session->markers,
                                       (uint64_t)(session->marker_count * sizeof(CoreTraceMarker)));
    }

    {
        CoreResult close_r = core_pack_writer_close(&w);
        if (r.code != CORE_OK) return r;
        return close_r;
    }
}

CoreResult core_trace_import_pack(const char *pack_path, CoreTraceSession *out_session) {
    CorePackReader r;
    CoreTraceHeaderV1 h;
    CorePackChunkInfo trhd;
    CorePackChunkInfo trsm;
    CorePackChunkInfo trev;
    CoreTraceConfig cfg;
    CoreResult rr;

    if (!pack_path || !out_session) return invalid_arg("pack_path/out_session is null");
    memset(out_session, 0, sizeof(*out_session));
    memset(&r, 0, sizeof(r));
    memset(&h, 0, sizeof(h));
    memset(&trhd, 0, sizeof(trhd));
    memset(&trsm, 0, sizeof(trsm));
    memset(&trev, 0, sizeof(trev));

    rr = core_pack_reader_open(pack_path, &r);
    if (rr.code != CORE_OK) return rr;

    rr = core_pack_reader_find_chunk(&r, "TRHD", 0, &trhd);
    if (rr.code != CORE_OK) {
        core_pack_reader_close(&r);
        return rr;
    }
    if (trhd.size != sizeof(CoreTraceHeaderV1)) {
        core_pack_reader_close(&r);
        return invalid_arg("invalid TRHD size");
    }

    rr = core_pack_reader_read_chunk_data(&r, &trhd, &h, (uint64_t)sizeof(h));
    if (rr.code != CORE_OK) {
        core_pack_reader_close(&r);
        return rr;
    }
    if (h.trace_profile_version != 1u) {
        core_pack_reader_close(&r);
        return invalid_arg("unsupported trace profile version");
    }

    cfg.sample_capacity = (h.sample_count > 0u) ? (size_t)h.sample_count : 1u;
    cfg.marker_capacity = (h.marker_count > 0u) ? (size_t)h.marker_count : 1u;

    rr = core_trace_session_init(out_session, &cfg);
    if (rr.code != CORE_OK) {
        core_pack_reader_close(&r);
        return rr;
    }

    if (h.sample_count > 0u) {
        rr = core_pack_reader_find_chunk(&r, "TRSM", 0, &trsm);
        if (rr.code != CORE_OK) goto fail;
        if (trsm.size != h.sample_count * sizeof(CoreTraceSampleF32)) {
            rr = invalid_arg("invalid TRSM size");
            goto fail;
        }
        rr = core_pack_reader_read_chunk_data(&r, &trsm, out_session->samples, trsm.size);
        if (rr.code != CORE_OK) goto fail;
        out_session->sample_count = (size_t)h.sample_count;
        out_session->stats.samples_emitted = out_session->sample_count;
    }

    if (h.marker_count > 0u) {
        rr = core_pack_reader_find_chunk(&r, "TREV", 0, &trev);
        if (rr.code != CORE_OK) goto fail;
        if (trev.size != h.marker_count * sizeof(CoreTraceMarker)) {
            rr = invalid_arg("invalid TREV size");
            goto fail;
        }
        rr = core_pack_reader_read_chunk_data(&r, &trev, out_session->markers, trev.size);
        if (rr.code != CORE_OK) goto fail;
        out_session->marker_count = (size_t)h.marker_count;
        out_session->stats.markers_emitted = out_session->marker_count;
    }

    core_pack_reader_close(&r);
    return core_result_ok();

fail:
    core_pack_reader_close(&r);
    core_trace_session_reset(out_session);
    return rr;
}

const CoreTraceStats *core_trace_stats(const CoreTraceSession *session) {
    if (!session) return NULL;
    return &session->stats;
}

size_t core_trace_sample_count(const CoreTraceSession *session) {
    if (!session) return 0;
    return session->sample_count;
}

size_t core_trace_marker_count(const CoreTraceSession *session) {
    if (!session) return 0;
    return session->marker_count;
}

const CoreTraceSampleF32 *core_trace_samples(const CoreTraceSession *session) {
    if (!session) return NULL;
    return session->samples;
}

const CoreTraceMarker *core_trace_markers(const CoreTraceSession *session) {
    if (!session) return NULL;
    return session->markers;
}
