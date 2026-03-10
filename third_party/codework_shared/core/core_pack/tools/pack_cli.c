#include "core_pack.h"

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Vf2dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    cell_size;
    uint32_t obstacle_mask_crc32;
} Vf2dHeaderCanonical;

typedef struct TraceHeaderV1 {
    uint32_t trace_profile_version;
    uint32_t reserved;
    uint64_t sample_count;
    uint64_t marker_count;
} TraceHeaderV1;

typedef struct TraceSampleV1 {
    double time_seconds;
    float value;
    char lane[32];
} TraceSampleV1;

typedef struct TraceMarkerV1 {
    double time_seconds;
    char lane[32];
    char label[64];
} TraceMarkerV1;

static void bounded_text_copy(char *dst, size_t dst_size, const char *src, size_t src_size) {
    size_t n = 0;
    if (!dst || !src || dst_size == 0) return;
    n = src_size;
    if (n >= dst_size) n = dst_size - 1u;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void print_ascii_preview(CorePackReader *r, const CorePackChunkInfo *info) {
    uint64_t n = info->size < 120 ? info->size : 120;
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return;
    if (core_pack_reader_read_chunk_slice(r, info, 0, buf, n).code == CORE_OK) {
        for (uint64_t i = 0; i < n; ++i) {
            unsigned char c = (unsigned char)buf[i];
            if (c < 32 || c > 126) buf[i] = '.';
        }
        buf[n] = '\0';
        printf("  preview=\"%s\"\n", buf);
    }
    free(buf);
}

static void print_float_stats(CorePackReader *r, const CorePackChunkInfo *info) {
    if (info->size % sizeof(float) != 0) return;
    uint64_t count = info->size / sizeof(float);
    if (count == 0) return;

    float min_v = FLT_MAX;
    float max_v = -FLT_MAX;
    float preview[4] = {0};
    uint64_t preview_n = count < 4 ? count : 4;

    if (core_pack_reader_read_chunk_slice(r, info, 0, preview, preview_n * sizeof(float)).code != CORE_OK) {
        return;
    }

    const uint64_t block_floats = 1024;
    float *buf = (float *)malloc((size_t)block_floats * sizeof(float));
    if (!buf) return;

    uint64_t consumed = 0;
    while (consumed < count) {
        uint64_t take = (count - consumed) < block_floats ? (count - consumed) : block_floats;
        uint64_t bytes = take * sizeof(float);
        uint64_t offset = consumed * sizeof(float);
        if (core_pack_reader_read_chunk_slice(r, info, offset, buf, bytes).code != CORE_OK) {
            free(buf);
            return;
        }
        for (uint64_t i = 0; i < take; ++i) {
            if (buf[i] < min_v) min_v = buf[i];
            if (buf[i] > max_v) max_v = buf[i];
        }
        consumed += take;
    }
    free(buf);

    printf("  float_count=%llu min=%g max=%g preview=", (unsigned long long)count, min_v, max_v);
    for (uint64_t i = 0; i < preview_n; ++i) {
        printf(i + 1 == preview_n ? "%g" : "%g,", preview[i]);
    }
    printf("\n");
}

static void print_vfhd(CorePackReader *r, const CorePackChunkInfo *info) {
    if (info->size != sizeof(Vf2dHeaderCanonical)) return;
    Vf2dHeaderCanonical h;
    if (core_pack_reader_read_chunk_data(r, info, &h, sizeof(h)).code != CORE_OK) return;
    printf("  vf2d: version=%u grid=%ux%u frame=%llu time=%.6f dt=%.6f origin=(%.3f,%.3f) cell=%.3f crc=%u\n",
           h.version,
           h.grid_w,
           h.grid_h,
           (unsigned long long)h.frame_index,
           h.time_seconds,
           h.dt_seconds,
           h.origin_x,
           h.origin_y,
           h.cell_size,
           h.obstacle_mask_crc32);
}

static void print_trace_header(CorePackReader *r, const CorePackChunkInfo *info) {
    TraceHeaderV1 h;
    if (info->size != sizeof(TraceHeaderV1)) return;
    if (core_pack_reader_read_chunk_data(r, info, &h, sizeof(h)).code != CORE_OK) return;
    printf("  trace: profile_version=%u samples=%llu markers=%llu\n",
           h.trace_profile_version,
           (unsigned long long)h.sample_count,
           (unsigned long long)h.marker_count);
}

static void print_trace_sample_preview(CorePackReader *r, const CorePackChunkInfo *info) {
    TraceSampleV1 sample;
    uint64_t count = 0;
    uint64_t preview_count = 0;
    if (info->size % sizeof(TraceSampleV1) != 0) return;
    count = info->size / sizeof(TraceSampleV1);
    preview_count = count < 3u ? count : 3u;
    printf("  trace_samples=%llu", (unsigned long long)count);
    for (uint64_t i = 0; i < preview_count; ++i) {
        char lane[32];
        if (core_pack_reader_read_chunk_slice(r,
                                              info,
                                              i * sizeof(TraceSampleV1),
                                              &sample,
                                              sizeof(TraceSampleV1)).code != CORE_OK) {
            break;
        }
        bounded_text_copy(lane, sizeof(lane), sample.lane, sizeof(sample.lane));
        printf(i == 0 ? " preview=" : ";");
        printf("{lane=%s t=%.6f v=%g}", lane, sample.time_seconds, sample.value);
    }
    printf("\n");
}

static void print_trace_marker_preview(CorePackReader *r, const CorePackChunkInfo *info) {
    TraceMarkerV1 marker;
    uint64_t count = 0;
    uint64_t preview_count = 0;
    if (info->size % sizeof(TraceMarkerV1) != 0) return;
    count = info->size / sizeof(TraceMarkerV1);
    preview_count = count < 3u ? count : 3u;
    printf("  trace_markers=%llu", (unsigned long long)count);
    for (uint64_t i = 0; i < preview_count; ++i) {
        char lane[32];
        char label[64];
        if (core_pack_reader_read_chunk_slice(r,
                                              info,
                                              i * sizeof(TraceMarkerV1),
                                              &marker,
                                              sizeof(TraceMarkerV1)).code != CORE_OK) {
            break;
        }
        bounded_text_copy(lane, sizeof(lane), marker.lane, sizeof(marker.lane));
        bounded_text_copy(label, sizeof(label), marker.label, sizeof(marker.label));
        printf(i == 0 ? " preview=" : ";");
        printf("{lane=%s t=%.6f label=%s}", lane, marker.time_seconds, label);
    }
    printf("\n");
}

static int cmd_write(const char *path) {
    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    if (r.code != CORE_OK) {
        fprintf(stderr, "write: %s\n", r.message);
        return 1;
    }

    const char meta[] = "manifest=v1";
    const float data[4] = {0.1f, 0.2f, 0.3f, 0.4f};

    r = core_pack_writer_add_chunk(&w, "META", meta, (uint64_t)(sizeof(meta) - 1));
    if (r.code == CORE_OK) r = core_pack_writer_add_chunk(&w, "F32A", data, (uint64_t)sizeof(data));

    CoreResult close_r = core_pack_writer_close(&w);
    if (r.code != CORE_OK) {
        fprintf(stderr, "write: %s\n", r.message);
        return 1;
    }
    if (close_r.code != CORE_OK) {
        fprintf(stderr, "close: %s\n", close_r.message);
        return 1;
    }

    printf("wrote %s\n", path);
    return 0;
}

static int cmd_inspect(const char *path) {
    CorePackReader r = {0};
    CoreResult rr = core_pack_reader_open(path, &r);
    if (rr.code != CORE_OK) {
        fprintf(stderr, "inspect: %s\n", rr.message);
        return 1;
    }

    size_t count = core_pack_reader_chunk_count(&r);
    printf("pack=%s version=%u (%u.%u) chunk_count=%zu\n",
           path,
           r.version,
           core_pack_format_version_major_from_token(r.version),
           core_pack_format_version_minor_from_token(r.version),
           count);

    for (size_t i = 0; i < count; ++i) {
        CorePackChunkInfo info;
        CoreResult cr = core_pack_reader_get_chunk(&r, i, &info);
        if (cr.code != CORE_OK) {
            fprintf(stderr, "inspect: %s\n", cr.message);
            core_pack_reader_close(&r);
            return 1;
        }

        printf("chunk[%zu] type=%s size=%llu offset=%llu\n",
               i,
               info.type,
               (unsigned long long)info.size,
               (unsigned long long)info.data_offset);

        if (strcmp(info.type, "META") == 0 || strcmp(info.type, "JSON") == 0) {
            print_ascii_preview(&r, &info);
        }
        if (strcmp(info.type, "VFHD") == 0) {
            print_vfhd(&r, &info);
        }
        if (strcmp(info.type, "TRHD") == 0) {
            print_trace_header(&r, &info);
        }
        if (strcmp(info.type, "TRSM") == 0) {
            print_trace_sample_preview(&r, &info);
        }
        if (strcmp(info.type, "TREV") == 0) {
            print_trace_marker_preview(&r, &info);
        }
        if (strcmp(info.type, "F32A") == 0 || strcmp(info.type, "ARRF") == 0 ||
            strcmp(info.type, "DENS") == 0 || strcmp(info.type, "VELX") == 0 ||
            strcmp(info.type, "VELY") == 0) {
            print_float_stats(&r, &info);
        }
    }

    core_pack_reader_close(&r);
    return 0;
}

static int cmd_roundtrip(const char *path) {
    if (cmd_write(path) != 0) return 1;

    CorePackReader r = {0};
    CoreResult rr = core_pack_reader_open(path, &r);
    if (rr.code != CORE_OK) {
        fprintf(stderr, "roundtrip: %s\n", rr.message);
        return 1;
    }

    int saw_meta = 0;
    int saw_data = 0;

    CorePackChunkInfo meta_info;
    if (core_pack_reader_find_chunk(&r, "META", 0, &meta_info).code == CORE_OK) {
        char *buf = (char *)malloc((size_t)meta_info.size + 1);
        if (!buf) {
            core_pack_reader_close(&r);
            return 1;
        }
        if (core_pack_reader_read_chunk_data(&r, &meta_info, buf, meta_info.size).code == CORE_OK) {
            buf[meta_info.size] = '\0';
            if (strcmp(buf, "manifest=v1") == 0) saw_meta = 1;
        }
        free(buf);
    }

    CorePackChunkInfo f32_info;
    if (core_pack_reader_find_chunk(&r, "F32A", 0, &f32_info).code == CORE_OK) {
        float values[4] = {0};
        if (f32_info.size == sizeof(values) &&
            core_pack_reader_read_chunk_data(&r, &f32_info, values, sizeof(values)).code == CORE_OK &&
            values[0] == 0.1f && values[1] == 0.2f && values[2] == 0.3f && values[3] == 0.4f) {
            saw_data = 1;
        }
    }

    core_pack_reader_close(&r);

    if (!saw_meta || !saw_data) {
        fprintf(stderr, "roundtrip validation failed\n");
        return 1;
    }

    printf("roundtrip ok: %s\n", path);
    return 0;
}

static int cmd_convert_vf2d(const char *vf2d_path, const char *pack_path, const char *manifest_opt) {
    CoreResult r = core_pack_convert_vf2d(vf2d_path, pack_path, manifest_opt);
    if (r.code != CORE_OK) {
        fprintf(stderr, "vf2d_to_pack: %s\n", r.message);
        return 1;
    }
    printf("converted %s -> %s\n", vf2d_path, pack_path);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "  %s write <path>\n", argv[0]);
        fprintf(stderr, "  %s inspect <path>\n", argv[0]);
        fprintf(stderr, "  %s roundtrip <path>\n", argv[0]);
        fprintf(stderr, "  %s vf2d_to_pack <vf2d_path> <pack_path> [manifest_json]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "write") == 0 && argc == 3) return cmd_write(argv[2]);
    if (strcmp(argv[1], "inspect") == 0 && argc == 3) return cmd_inspect(argv[2]);
    if (strcmp(argv[1], "roundtrip") == 0 && argc == 3) return cmd_roundtrip(argv[2]);
    if (strcmp(argv[1], "vf2d_to_pack") == 0 && (argc == 4 || argc == 5)) {
        return cmd_convert_vf2d(argv[2], argv[3], argc == 5 ? argv[4] : NULL);
    }

    fprintf(stderr, "invalid arguments\n");
    return 1;
}
