#ifndef CORE_PACK_H
#define CORE_PACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "core_base.h"

#define CORE_PACK_FORMAT_VERSION_MAJOR 1u
#define CORE_PACK_FORMAT_VERSION_MINOR 0u

typedef struct CorePackChunkInfo {
    char type[5];
    uint64_t data_offset;
    uint64_t size;
} CorePackChunkInfo;

typedef enum CorePackCodec {
    CORE_PACK_CODEC_NONE = 0,
    CORE_PACK_CODEC_RLE8 = 1
} CorePackCodec;

typedef struct CorePackWriter {
    FILE *file;
    uint32_t version;
    uint32_t version_major;
    uint32_t version_minor;
    CorePackChunkInfo *index_entries;
    size_t index_count;
    size_t index_capacity;
} CorePackWriter;

typedef struct CorePackReader {
    FILE *file;
    uint32_t version;
    uint32_t version_major;
    uint32_t version_minor;
    CorePackChunkInfo *index_entries;
    size_t index_count;
    size_t index_capacity;
    size_t next_index;
} CorePackReader;

uint32_t core_pack_format_version_token(void);
uint32_t core_pack_format_version_major_from_token(uint32_t token);
uint32_t core_pack_format_version_minor_from_token(uint32_t token);

CoreResult core_pack_writer_open(const char *path, CorePackWriter *out_writer);
CoreResult core_pack_writer_add_chunk(CorePackWriter *writer, const char type[4], const void *data, uint64_t size);
CoreResult core_pack_writer_add_chunk_encoded(CorePackWriter *writer,
                                              const char type[4],
                                              const void *data,
                                              uint64_t size,
                                              CorePackCodec codec);
CoreResult core_pack_writer_close(CorePackWriter *writer);

CoreResult core_pack_reader_open(const char *path, CorePackReader *out_reader);
CoreResult core_pack_reader_next_chunk(CorePackReader *reader, CorePackChunkInfo *out_chunk);
CoreResult core_pack_reader_get_chunk(const CorePackReader *reader, size_t chunk_index, CorePackChunkInfo *out_chunk);
CoreResult core_pack_reader_find_chunk(const CorePackReader *reader, const char type[4], size_t occurrence, CorePackChunkInfo *out_chunk);
size_t core_pack_reader_chunk_count(const CorePackReader *reader);
CoreResult core_pack_reader_read_chunk_data(CorePackReader *reader, const CorePackChunkInfo *chunk, void *dst, uint64_t dst_size);
CoreResult core_pack_reader_read_chunk_decoded(CorePackReader *reader,
                                               const CorePackChunkInfo *chunk,
                                               void *dst,
                                               uint64_t dst_size,
                                               uint64_t *out_decoded_size);
CoreResult core_pack_reader_read_chunk_slice(CorePackReader *reader,
                                             const CorePackChunkInfo *chunk,
                                             uint64_t offset,
                                             void *dst,
                                             uint64_t size);
CoreResult core_pack_reader_close(CorePackReader *reader);

// Convert legacy PhysicsSim VF2D frame (+ optional manifest JSON) to .pack.
CoreResult core_pack_convert_vf2d(const char *vf2d_path, const char *pack_path, const char *manifest_path_optional);

// Convert truthful PhysicsSim VF3D frame (+ optional manifest JSON) to .pack.
CoreResult core_pack_convert_vf3d(const char *vf3d_path, const char *pack_path, const char *manifest_path_optional);

#endif
