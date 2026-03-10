#include "core_pack.h"

#include "core_io.h"

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

typedef struct VolumeFrameHeaderV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
} VolumeFrameHeaderV1;

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

static const uint32_t VOLUME_MAGIC = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
static const uint32_t VOLUME_VERSION_V2 = 2;
static const uint32_t VOLUME_VERSION_V1 = 1;

CoreResult core_pack_convert_vf2d(const char *vf2d_path, const char *pack_path, const char *manifest_path_optional) {
    if (!vf2d_path || !pack_path) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    FILE *f = fopen(vf2d_path, "rb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open vf2d" };
        return r;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || fread(&version, sizeof(version), 1, f) != 1) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to read vf2d header" };
        return r;
    }

    if (magic != VOLUME_MAGIC) {
        fclose(f);
        CoreResult r = { CORE_ERR_FORMAT, "invalid vf2d magic" };
        return r;
    }

    Vf2dHeaderCanonical canon;
    memset(&canon, 0, sizeof(canon));

    if (version == VOLUME_VERSION_V2) {
        VolumeFrameHeaderV2 h;
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            CoreResult r = { CORE_ERR_IO, "failed to read vf2d v2 header" };
            return r;
        }
        canon.version = h.version;
        canon.grid_w = h.grid_w;
        canon.grid_h = h.grid_h;
        canon.time_seconds = h.time_seconds;
        canon.frame_index = h.frame_index;
        canon.dt_seconds = h.dt_seconds;
        canon.origin_x = h.origin_x;
        canon.origin_y = h.origin_y;
        canon.cell_size = h.cell_size;
        canon.obstacle_mask_crc32 = h.obstacle_mask_crc32;
    } else if (version == VOLUME_VERSION_V1) {
        VolumeFrameHeaderV1 h;
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            CoreResult r = { CORE_ERR_IO, "failed to read vf2d v1 header" };
            return r;
        }
        canon.version = h.version;
        canon.grid_w = h.grid_w;
        canon.grid_h = h.grid_h;
        canon.time_seconds = h.time_seconds;
        canon.frame_index = h.frame_index;
        canon.dt_seconds = 0.0;
        canon.origin_x = 0.0f;
        canon.origin_y = 0.0f;
        canon.cell_size = 1.0f;
        canon.obstacle_mask_crc32 = 0;
    } else {
        fclose(f);
        CoreResult r = { CORE_ERR_FORMAT, "unsupported vf2d version" };
        return r;
    }

    size_t count = (size_t)canon.grid_w * (size_t)canon.grid_h;
    size_t bytes = count * sizeof(float);

    float *density = (float *)core_alloc(bytes);
    float *velx = (float *)core_alloc(bytes);
    float *vely = (float *)core_alloc(bytes);
    if (!density || !velx || !vely) {
        core_free(density);
        core_free(velx);
        core_free(vely);
        fclose(f);
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    if (fread(density, 1, bytes, f) != bytes ||
        fread(velx, 1, bytes, f) != bytes ||
        fread(vely, 1, bytes, f) != bytes) {
        core_free(density);
        core_free(velx);
        core_free(vely);
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to read vf2d data arrays" };
        return r;
    }
    fclose(f);

    CorePackWriter writer;
    CoreResult wr = core_pack_writer_open(pack_path, &writer);
    if (wr.code != CORE_OK) {
        core_free(density);
        core_free(velx);
        core_free(vely);
        return wr;
    }

    wr = core_pack_writer_add_chunk(&writer, "VFHD", &canon, (uint64_t)sizeof(canon));
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "DENS", density, (uint64_t)bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELX", velx, (uint64_t)bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELY", vely, (uint64_t)bytes);

    if (wr.code == CORE_OK && manifest_path_optional && manifest_path_optional[0]) {
        CoreBuffer manifest = {0};
        CoreResult mr = core_io_read_all(manifest_path_optional, &manifest);
        if (mr.code == CORE_OK) {
            wr = core_pack_writer_add_chunk(&writer, "JSON", manifest.data, (uint64_t)manifest.size);
            core_io_buffer_free(&manifest);
        }
    }

    CoreResult close_r = core_pack_writer_close(&writer);

    core_free(density);
    core_free(velx);
    core_free(vely);

    if (wr.code != CORE_OK) return wr;
    if (close_r.code != CORE_OK) return close_r;
    return core_result_ok();
}
