#include "core_pack.h"

#include "core_io.h"

#include <stdio.h>
#include <string.h>

typedef struct VolumeFrameHeaderVf3dV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    origin_z;
    float    voxel_size;
    float    scene_up_x;
    float    scene_up_y;
    float    scene_up_z;
    uint32_t solid_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderVf3dV1;

typedef struct Vf3dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    origin_z;
    float    voxel_size;
    float    scene_up_x;
    float    scene_up_y;
    float    scene_up_z;
    uint32_t solid_mask_crc32;
} Vf3dHeaderCanonical;

static const uint32_t VOLUME_VF3D_MAGIC = ('V' << 24) | ('F' << 16) | ('3' << 8) | ('D');
static const uint32_t VOLUME_VF3D_VERSION_V1 = 1;

CoreResult core_pack_convert_vf3d(const char *vf3d_path, const char *pack_path, const char *manifest_path_optional) {
    if (!vf3d_path || !pack_path) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    FILE *f = fopen(vf3d_path, "rb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open vf3d" };
        return r;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    VolumeFrameHeaderVf3dV1 h = {0};
    Vf3dHeaderCanonical canon = {0};
    size_t count = 0;
    size_t float_bytes = 0;
    size_t solid_bytes = 0;
    float *density = NULL;
    float *velx = NULL;
    float *vely = NULL;
    float *velz = NULL;
    float *pressure = NULL;
    uint8_t *solid = NULL;
    CorePackWriter writer = {0};
    CoreResult wr = core_result_ok();
    CoreResult close_r = core_result_ok();

    if (fread(&magic, sizeof(magic), 1, f) != 1 || fread(&version, sizeof(version), 1, f) != 1) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to read vf3d header" };
        return r;
    }
    if (magic != VOLUME_VF3D_MAGIC) {
        fclose(f);
        CoreResult r = { CORE_ERR_FORMAT, "invalid vf3d magic" };
        return r;
    }
    if (version != VOLUME_VF3D_VERSION_V1) {
        fclose(f);
        CoreResult r = { CORE_ERR_FORMAT, "unsupported vf3d version" };
        return r;
    }

    h.magic = magic;
    h.version = version;
    if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to read vf3d v1 header" };
        return r;
    }

    canon.version = h.version;
    canon.grid_w = h.grid_w;
    canon.grid_h = h.grid_h;
    canon.grid_d = h.grid_d;
    canon.time_seconds = h.time_seconds;
    canon.frame_index = h.frame_index;
    canon.dt_seconds = h.dt_seconds;
    canon.origin_x = h.origin_x;
    canon.origin_y = h.origin_y;
    canon.origin_z = h.origin_z;
    canon.voxel_size = h.voxel_size;
    canon.scene_up_x = h.scene_up_x;
    canon.scene_up_y = h.scene_up_y;
    canon.scene_up_z = h.scene_up_z;
    canon.solid_mask_crc32 = h.solid_mask_crc32;

    count = (size_t)canon.grid_w * (size_t)canon.grid_h * (size_t)canon.grid_d;
    float_bytes = count * sizeof(float);
    solid_bytes = count * sizeof(uint8_t);
    density = (float *)core_alloc(float_bytes);
    velx = (float *)core_alloc(float_bytes);
    vely = (float *)core_alloc(float_bytes);
    velz = (float *)core_alloc(float_bytes);
    pressure = (float *)core_alloc(float_bytes);
    solid = (uint8_t *)core_alloc(solid_bytes);
    if (!density || !velx || !vely || !velz || !pressure || !solid) {
        core_free(density);
        core_free(velx);
        core_free(vely);
        core_free(velz);
        core_free(pressure);
        core_free(solid);
        fclose(f);
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    if (fread(density, 1, float_bytes, f) != float_bytes ||
        fread(velx, 1, float_bytes, f) != float_bytes ||
        fread(vely, 1, float_bytes, f) != float_bytes ||
        fread(velz, 1, float_bytes, f) != float_bytes ||
        fread(pressure, 1, float_bytes, f) != float_bytes ||
        fread(solid, 1, solid_bytes, f) != solid_bytes) {
        core_free(density);
        core_free(velx);
        core_free(vely);
        core_free(velz);
        core_free(pressure);
        core_free(solid);
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to read vf3d data arrays" };
        return r;
    }
    fclose(f);

    wr = core_pack_writer_open(pack_path, &writer);
    if (wr.code != CORE_OK) {
        core_free(density);
        core_free(velx);
        core_free(vely);
        core_free(velz);
        core_free(pressure);
        core_free(solid);
        return wr;
    }

    wr = core_pack_writer_add_chunk(&writer, "VF3H", &canon, (uint64_t)sizeof(canon));
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "DENS", density, (uint64_t)float_bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELX", velx, (uint64_t)float_bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELY", vely, (uint64_t)float_bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELZ", velz, (uint64_t)float_bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "PRES", pressure, (uint64_t)float_bytes);
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "SOLI", solid, (uint64_t)solid_bytes);

    if (wr.code == CORE_OK && manifest_path_optional && manifest_path_optional[0]) {
        CoreBuffer manifest = {0};
        CoreResult mr = core_io_read_all(manifest_path_optional, &manifest);
        if (mr.code == CORE_OK) {
            wr = core_pack_writer_add_chunk(&writer, "JSON", manifest.data, (uint64_t)manifest.size);
            core_io_buffer_free(&manifest);
        }
    }

    close_r = core_pack_writer_close(&writer);

    core_free(density);
    core_free(velx);
    core_free(vely);
    core_free(velz);
    core_free(pressure);
    core_free(solid);

    if (wr.code != CORE_OK) return wr;
    if (close_r.code != CORE_OK) return close_r;
    return core_result_ok();
}
