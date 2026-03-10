#include "core_pack.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct VolumeFrameHeaderV2 {
    uint32_t magic;
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
    uint32_t reserved[3];
} VolumeFrameHeaderV2;

typedef struct DawHeaderV1 {
    uint32_t version;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t samples_per_pixel;
    uint64_t point_count;
    uint64_t start_frame;
    uint64_t end_frame;
    uint64_t project_duration_frames;
} DawHeaderV1;

typedef struct DawMarkerV1 {
    uint64_t frame;
    double beat;
    uint32_t kind;
    uint32_t reserved;
    double value_a;
    double value_b;
} DawMarkerV1;

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

static void write_test_vf2d(const char *path) {
    FILE *f = fopen(path, "wb");
    assert(f != NULL);

    const uint32_t magic = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
    VolumeFrameHeaderV2 h;
    h.magic = magic;
    h.version = 2;
    h.grid_w = 2;
    h.grid_h = 2;
    h.time_seconds = 3.5;
    h.frame_index = 7;
    h.dt_seconds = 0.016;
    h.origin_x = 0.0f;
    h.origin_y = 0.0f;
    h.cell_size = 1.0f;
    h.obstacle_mask_crc32 = 123;
    h.reserved[0] = h.reserved[1] = h.reserved[2] = 0;

    float density[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float velx[4] = {1.0f, 0.0f, -1.0f, 0.5f};
    float vely[4] = {0.5f, -0.5f, 0.25f, -0.25f};

    assert(fwrite(&h, sizeof(h), 1, f) == 1);
    assert(fwrite(density, sizeof(float), 4, f) == 4);
    assert(fwrite(velx, sizeof(float), 4, f) == 4);
    assert(fwrite(vely, sizeof(float), 4, f) == 4);
    fclose(f);
}

static void write_test_manifest(const char *path) {
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    const char *json = "{\"manifest_version\":1,\"frames\":[{\"frame_index\":7}]}";
    assert(fwrite(json, 1, strlen(json), f) == strlen(json));
    fclose(f);
}

static void overwrite_header_version_token(const char *path, uint32_t token) {
    FILE *f = fopen(path, "rb+");
    assert(f != NULL);
    assert(fseek(f, 4L, SEEK_SET) == 0);
    assert(fwrite(&token, sizeof(token), 1, f) == 1);
    fclose(f);
}

int main(void) {
    const char *path = "/tmp/core_pack_test.pack";

    CorePackWriter w = {0};
    CoreResult r = core_pack_writer_open(path, &w);
    assert(r.code == CORE_OK);

    const char meta[] = "pack-test";
    r = core_pack_writer_add_chunk(&w, "META", meta, (uint64_t)(sizeof(meta) - 1));
    assert(r.code == CORE_OK);

    float values[3] = {1.0f, 2.0f, 3.0f};
    r = core_pack_writer_add_chunk(&w, "ARRF", values, (uint64_t)sizeof(values));
    assert(r.code == CORE_OK);

    unsigned char rle_bytes[32];
    for (int i = 0; i < 16; ++i) rle_bytes[i] = 7;
    for (int i = 16; i < 32; ++i) rle_bytes[i] = 9;
    r = core_pack_writer_add_chunk_encoded(&w, "RCMP", rle_bytes, sizeof(rle_bytes), CORE_PACK_CODEC_RLE8);
    assert(r.code == CORE_OK);

    r = core_pack_writer_close(&w);
    assert(r.code == CORE_OK);

    // Validate file header was encoded little-endian.
    FILE *raw = fopen(path, "rb");
    assert(raw != NULL);
    unsigned char hdr[8] = {0};
    assert(fread(hdr, 1, sizeof(hdr), raw) == sizeof(hdr));
    fclose(raw);
    assert(hdr[0] == 'K' && hdr[1] == 'A' && hdr[2] == 'P' && hdr[3] == 'C');
    assert(hdr[4] == 1 && hdr[5] == 0 && hdr[6] == 0 && hdr[7] == 0);

    CorePackReader rd = {0};
    r = core_pack_reader_open(path, &rd);
    assert(r.code == CORE_OK);

    assert(core_pack_reader_chunk_count(&rd) == 3);

    CorePackChunkInfo meta_chunk;
    r = core_pack_reader_find_chunk(&rd, "META", 0, &meta_chunk);
    assert(r.code == CORE_OK);
    assert(meta_chunk.size == 9);

    char meta_buf[16] = {0};
    r = core_pack_reader_read_chunk_data(&rd, &meta_chunk, meta_buf, sizeof(meta_buf));
    assert(r.code == CORE_OK);
    assert(strcmp(meta_buf, "pack-test") == 0);

    CorePackChunkInfo arr_chunk;
    r = core_pack_reader_get_chunk(&rd, 1, &arr_chunk);
    assert(r.code == CORE_OK);
    assert(strcmp(arr_chunk.type, "ARRF") == 0);

    float mid = 0.0f;
    r = core_pack_reader_read_chunk_slice(&rd, &arr_chunk, sizeof(float), &mid, sizeof(float));
    assert(r.code == CORE_OK);
    assert(mid == 2.0f);

    CorePackChunkInfo rcmp_chunk;
    r = core_pack_reader_find_chunk(&rd, "RCMP", 0, &rcmp_chunk);
    assert(r.code == CORE_OK);
    assert(rcmp_chunk.size > 0);
    unsigned char decoded[32] = {0};
    uint64_t decoded_size = 0;
    r = core_pack_reader_read_chunk_decoded(&rd, &rcmp_chunk, decoded, sizeof(decoded), &decoded_size);
    assert(r.code == CORE_OK);
    assert(decoded_size == sizeof(decoded));
    for (int i = 0; i < 16; ++i) assert(decoded[i] == 7);
    for (int i = 16; i < 32; ++i) assert(decoded[i] == 9);

    int seen_meta = 0;
    int seen_arr = 0;
    int seen_rcmp = 0;
    for (;;) {
        CorePackChunkInfo info;
        r = core_pack_reader_next_chunk(&rd, &info);
        if (r.code == CORE_ERR_NOT_FOUND) break;
        assert(r.code == CORE_OK);
        if (strcmp(info.type, "META") == 0) seen_meta = 1;
        if (strcmp(info.type, "ARRF") == 0) seen_arr = 1;
        if (strcmp(info.type, "RCMP") == 0) seen_rcmp = 1;
    }
    assert(seen_meta == 1);
    assert(seen_arr == 1);
    assert(seen_rcmp == 1);

    r = core_pack_reader_close(&rd);
    assert(r.code == CORE_OK);

    const char *vf2d = "/tmp/core_pack_test.vf2d";
    const char *manifest = "/tmp/core_pack_test_manifest.json";
    const char *converted = "/tmp/core_pack_from_vf2d.pack";

    write_test_vf2d(vf2d);
    write_test_manifest(manifest);

    r = core_pack_convert_vf2d(vf2d, converted, manifest);
    assert(r.code == CORE_OK);

    CorePackReader rd2 = {0};
    r = core_pack_reader_open(converted, &rd2);
    assert(r.code == CORE_OK);

    assert(core_pack_reader_chunk_count(&rd2) == 5);

    CorePackChunkInfo info;
    assert(core_pack_reader_find_chunk(&rd2, "VFHD", 0, &info).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&rd2, "DENS", 0, &info).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&rd2, "VELX", 0, &info).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&rd2, "VELY", 0, &info).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&rd2, "JSON", 0, &info).code == CORE_OK);

    r = core_pack_reader_close(&rd2);
    assert(r.code == CORE_OK);

    assert(core_pack_format_version_token() == 1u);
    assert(core_pack_format_version_major_from_token(1u) == 1u);
    assert(core_pack_format_version_minor_from_token(1u) == 0u);
    assert(core_pack_format_version_major_from_token(0x00010002u) == 1u);
    assert(core_pack_format_version_minor_from_token(0x00010002u) == 2u);

    const char *future_minor = "/tmp/core_pack_future_minor.pack";
    FILE *src = fopen(path, "rb");
    assert(src != NULL);
    FILE *dst = fopen(future_minor, "wb");
    assert(dst != NULL);
    unsigned char copy_buf[4096];
    for (;;) {
        size_t n = fread(copy_buf, 1, sizeof(copy_buf), src);
        if (n == 0) break;
        assert(fwrite(copy_buf, 1, n, dst) == n);
    }
    fclose(src);
    fclose(dst);
    overwrite_header_version_token(future_minor, 0x00010001u);

    CorePackReader bad = {0};
    r = core_pack_reader_open(future_minor, &bad);
    assert(r.code == CORE_ERR_FORMAT);

    const char *physics_fixture = "tests/fixtures/physics_v1_sample.pack";
    const char *daw_fixture = "tests/fixtures/daw_v1_sample.pack";

    CorePackReader pfx = {0};
    r = core_pack_reader_open(physics_fixture, &pfx);
    assert(r.code == CORE_OK);

    CorePackChunkInfo vfhd = {0};
    CorePackChunkInfo dens = {0};
    CorePackChunkInfo velx = {0};
    CorePackChunkInfo vely = {0};
    assert(core_pack_reader_find_chunk(&pfx, "VFHD", 0, &vfhd).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&pfx, "DENS", 0, &dens).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&pfx, "VELX", 0, &velx).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&pfx, "VELY", 0, &vely).code == CORE_OK);
    assert(vfhd.size == sizeof(Vf2dHeaderCanonical));

    Vf2dHeaderCanonical vh = {0};
    r = core_pack_reader_read_chunk_data(&pfx, &vfhd, &vh, sizeof(vh));
    assert(r.code == CORE_OK);
    uint64_t physics_bytes = (uint64_t)vh.grid_w * (uint64_t)vh.grid_h * sizeof(float);
    assert(dens.size == physics_bytes);
    assert(velx.size == physics_bytes);
    assert(vely.size == physics_bytes);
    r = core_pack_reader_close(&pfx);
    assert(r.code == CORE_OK);

    CorePackReader dfx = {0};
    r = core_pack_reader_open(daw_fixture, &dfx);
    assert(r.code == CORE_OK);

    CorePackChunkInfo dawh = {0};
    CorePackChunkInfo wmin = {0};
    CorePackChunkInfo wmax = {0};
    CorePackChunkInfo mrks = {0};
    assert(core_pack_reader_find_chunk(&dfx, "DAWH", 0, &dawh).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&dfx, "WMIN", 0, &wmin).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&dfx, "WMAX", 0, &wmax).code == CORE_OK);
    assert(core_pack_reader_find_chunk(&dfx, "MRKS", 0, &mrks).code == CORE_OK);
    assert(dawh.size == sizeof(DawHeaderV1));
    assert((mrks.size % sizeof(DawMarkerV1)) == 0);

    DawHeaderV1 dh = {0};
    r = core_pack_reader_read_chunk_data(&dfx, &dawh, &dh, sizeof(dh));
    assert(r.code == CORE_OK);
    assert(wmin.size == dh.point_count * sizeof(float));
    assert(wmax.size == dh.point_count * sizeof(float));

    r = core_pack_reader_close(&dfx);
    assert(r.code == CORE_OK);

    return 0;
}
