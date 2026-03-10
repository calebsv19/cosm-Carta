/*
 * core_pack.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_pack.h"

#include <string.h>

static const uint32_t CORE_PACK_MAGIC = ('C' << 24) | ('P' << 16) | ('A' << 8) | ('K');
static const uint32_t CORE_PACK_LEGACY_V1_TOKEN = 1;
static const uint32_t CORE_PACK_INDEX_MAGIC = ('C' << 24) | ('P' << 16) | ('K' << 8) | ('I');
static const uint32_t CORE_PACK_FOOTER_MAGIC = ('C' << 24) | ('P' << 16) | ('K' << 8) | ('F');
static const uint32_t CORE_PACK_CODEC_MAGIC = ('C' << 24) | ('M' << 16) | ('P' << 8) | ('1');

typedef struct CorePackHeader {
    uint32_t magic;
    uint32_t version;
} CorePackHeader;

typedef struct CorePackChunkHeader {
    char type[4];
    uint64_t size;
} CorePackChunkHeader;

typedef struct CorePackIndexHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t count;
} CorePackIndexHeader;

typedef struct CorePackIndexEntryDisk {
    char type[4];
    uint64_t data_offset;
    uint64_t size;
} CorePackIndexEntryDisk;

typedef struct CorePackFooter {
    uint32_t magic;
    uint32_t version;
    uint64_t index_offset;
} CorePackFooter;

typedef struct CorePackCodecHeader {
    uint32_t magic;
    uint32_t codec;
    uint64_t decoded_size;
    uint64_t encoded_size;
} CorePackCodecHeader;

uint32_t core_pack_format_version_token(void) {
    // Preserve legacy v1 token compatibility for current frozen major/minor.
    if (CORE_PACK_FORMAT_VERSION_MAJOR == 1u && CORE_PACK_FORMAT_VERSION_MINOR == 0u) {
        return CORE_PACK_LEGACY_V1_TOKEN;
    }
    return (CORE_PACK_FORMAT_VERSION_MAJOR << 16) | (CORE_PACK_FORMAT_VERSION_MINOR & 0xffffu);
}

uint32_t core_pack_format_version_major_from_token(uint32_t token) {
    if (token <= 0xffffu) {
        return token;
    }
    return token >> 16;
}

uint32_t core_pack_format_version_minor_from_token(uint32_t token) {
    if (token <= 0xffffu) {
        return 0u;
    }
    return token & 0xffffu;
}

static uint32_t bswap32(uint32_t v) {
    return ((v & 0x000000ffu) << 24) |
           ((v & 0x0000ff00u) << 8) |
           ((v & 0x00ff0000u) >> 8) |
           ((v & 0xff000000u) >> 24);
}

static uint64_t bswap64(uint64_t v) {
    return ((v & 0x00000000000000ffULL) << 56) |
           ((v & 0x000000000000ff00ULL) << 40) |
           ((v & 0x0000000000ff0000ULL) << 24) |
           ((v & 0x00000000ff000000ULL) << 8) |
           ((v & 0x000000ff00000000ULL) >> 8) |
           ((v & 0x0000ff0000000000ULL) >> 24) |
           ((v & 0x00ff000000000000ULL) >> 40) |
           ((v & 0xff00000000000000ULL) >> 56);
}

static uint32_t to_le32(uint32_t v) {
    if (core_is_little_endian()) return v;
    return bswap32(v);
}

static uint64_t to_le64(uint64_t v) {
    if (core_is_little_endian()) return v;
    return bswap64(v);
}

static uint32_t from_le32(uint32_t v) {
    if (core_is_little_endian()) return v;
    return bswap32(v);
}

static uint64_t from_le64(uint64_t v) {
    if (core_is_little_endian()) return v;
    return bswap64(v);
}

static CoreResult rle8_encode(const uint8_t *src, uint64_t src_size, uint8_t **out_data, uint64_t *out_size) {
    if (!src || !out_data || !out_size) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (src_size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return core_result_ok();
    }

    uint64_t cap = src_size * 2;
    uint8_t *dst = (uint8_t *)core_alloc((size_t)cap);
    if (!dst) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    uint64_t wr = 0;
    uint64_t i = 0;
    while (i < src_size) {
        uint8_t value = src[i];
        uint8_t run = 1;
        while ((i + run) < src_size && run < 255 && src[i + run] == value) {
            run++;
        }
        dst[wr++] = run;
        dst[wr++] = value;
        i += run;
    }

    *out_data = dst;
    *out_size = wr;
    return core_result_ok();
}

static CoreResult rle8_decode(const uint8_t *src, uint64_t src_size, uint8_t *dst, uint64_t dst_size) {
    if ((!src && src_size > 0) || (!dst && dst_size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if ((src_size % 2) != 0) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid rle payload" };
        return r;
    }

    uint64_t rd = 0;
    uint64_t wr = 0;
    while (rd < src_size) {
        uint8_t run = src[rd++];
        uint8_t value = src[rd++];
        if (wr + run > dst_size) {
            CoreResult r = { CORE_ERR_FORMAT, "rle output overflow" };
            return r;
        }
        for (uint8_t i = 0; i < run; ++i) {
            dst[wr++] = value;
        }
    }
    if (wr != dst_size) {
        CoreResult r = { CORE_ERR_FORMAT, "rle output size mismatch" };
        return r;
    }
    return core_result_ok();
}

static CoreResult ensure_index_capacity(CorePackChunkInfo **entries, size_t *capacity, size_t needed) {
    if (needed <= *capacity) return core_result_ok();

    size_t cap = (*capacity == 0) ? 16 : (*capacity * 2);
    while (cap < needed) cap *= 2;

    CorePackChunkInfo *next = (CorePackChunkInfo *)core_realloc(*entries, cap * sizeof(CorePackChunkInfo));
    if (!next) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    *entries = next;
    *capacity = cap;
    return core_result_ok();
}

static CoreResult writer_index_push(CorePackWriter *writer, const char type[4], uint64_t data_offset, uint64_t size) {
    CoreResult cap = ensure_index_capacity(&writer->index_entries, &writer->index_capacity, writer->index_count + 1);
    if (cap.code != CORE_OK) return cap;

    CorePackChunkInfo *entry = &writer->index_entries[writer->index_count++];
    memcpy(entry->type, type, 4);
    entry->type[4] = '\0';
    entry->data_offset = data_offset;
    entry->size = size;
    return core_result_ok();
}

static CoreResult reader_index_push(CorePackReader *reader, const char type[4], uint64_t data_offset, uint64_t size) {
    CoreResult cap = ensure_index_capacity(&reader->index_entries, &reader->index_capacity, reader->index_count + 1);
    if (cap.code != CORE_OK) return cap;

    CorePackChunkInfo *entry = &reader->index_entries[reader->index_count++];
    memcpy(entry->type, type, 4);
    entry->type[4] = '\0';
    entry->data_offset = data_offset;
    entry->size = size;
    return core_result_ok();
}

static CoreResult reader_scan_chunks(CorePackReader *reader) {
    if (fseek(reader->file, (long)sizeof(CorePackHeader), SEEK_SET) != 0) {
        CoreResult r = { CORE_ERR_IO, "failed to seek chunk scan start" };
        return r;
    }

    reader->index_count = 0;
    reader->next_index = 0;

    for (;;) {
        CorePackChunkHeader ch;
        size_t n = fread(&ch, 1, sizeof(ch), reader->file);
        if (n == 0) {
            break;
        }
        if (n != sizeof(ch)) {
            CoreResult r = { CORE_ERR_FORMAT, "truncated chunk header" };
            return r;
        }

        uint64_t size = from_le64(ch.size);
        long pos = ftell(reader->file);
        if (pos < 0) {
            CoreResult r = { CORE_ERR_IO, "failed to query chunk offset" };
            return r;
        }

        CoreResult push = reader_index_push(reader, ch.type, (uint64_t)pos, size);
        if (push.code != CORE_OK) return push;

        if (fseek(reader->file, (long)size, SEEK_CUR) != 0) {
            CoreResult r = { CORE_ERR_FORMAT, "truncated chunk payload" };
            return r;
        }
    }

    return core_result_ok();
}

static CoreResult reader_load_index_from_footer(CorePackReader *reader) {
    if (fseek(reader->file, 0, SEEK_END) != 0) {
        CoreResult r = { CORE_ERR_IO, "failed to seek pack end" };
        return r;
    }

    long file_size = ftell(reader->file);
    if (file_size < 0) {
        CoreResult r = { CORE_ERR_IO, "failed to get pack size" };
        return r;
    }

    if ((uint64_t)file_size < sizeof(CorePackHeader) + sizeof(CorePackFooter)) {
        return reader_scan_chunks(reader);
    }

    if (fseek(reader->file, file_size - (long)sizeof(CorePackFooter), SEEK_SET) != 0) {
        CoreResult r = { CORE_ERR_IO, "failed to seek footer" };
        return r;
    }

    CorePackFooter footer;
    if (fread(&footer, sizeof(footer), 1, reader->file) != 1) {
        CoreResult r = { CORE_ERR_IO, "failed to read footer" };
        return r;
    }

    uint32_t footer_magic = from_le32(footer.magic);
    uint32_t footer_version = from_le32(footer.version);
    uint64_t index_offset = from_le64(footer.index_offset);

    if (footer_magic != CORE_PACK_FOOTER_MAGIC || footer_version != core_pack_format_version_token()) {
        return reader_scan_chunks(reader);
    }

    if (index_offset < sizeof(CorePackHeader) ||
        index_offset > (uint64_t)file_size - sizeof(CorePackFooter)) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid index offset" };
        return r;
    }

    if (fseek(reader->file, (long)index_offset, SEEK_SET) != 0) {
        CoreResult r = { CORE_ERR_IO, "failed to seek index" };
        return r;
    }

    CorePackIndexHeader ih;
    if (fread(&ih, sizeof(ih), 1, reader->file) != 1) {
        CoreResult r = { CORE_ERR_IO, "failed to read index header" };
        return r;
    }

    uint32_t idx_magic = from_le32(ih.magic);
    uint32_t idx_version = from_le32(ih.version);
    uint64_t idx_count = from_le64(ih.count);

    if (idx_magic != CORE_PACK_INDEX_MAGIC || idx_version != core_pack_format_version_token()) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid index header" };
        return r;
    }

    if (idx_count > SIZE_MAX) {
        CoreResult r = { CORE_ERR_FORMAT, "index too large" };
        return r;
    }

    reader->index_count = 0;
    reader->next_index = 0;

    for (uint64_t i = 0; i < idx_count; ++i) {
        CorePackIndexEntryDisk de;
        if (fread(&de, sizeof(de), 1, reader->file) != 1) {
            CoreResult r = { CORE_ERR_FORMAT, "truncated index entry" };
            return r;
        }

        CoreResult push = reader_index_push(reader, de.type, from_le64(de.data_offset), from_le64(de.size));
        if (push.code != CORE_OK) return push;
    }

    return core_result_ok();
}

CoreResult core_pack_writer_open(const char *path, CorePackWriter *out_writer) {
    if (!path || !out_writer) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    memset(out_writer, 0, sizeof(*out_writer));

    FILE *f = fopen(path, "wb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open pack for write" };
        return r;
    }

    CorePackHeader h;
    h.magic = to_le32(CORE_PACK_MAGIC);
    h.version = to_le32(core_pack_format_version_token());

    if (fwrite(&h, sizeof(h), 1, f) != 1) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to write pack header" };
        return r;
    }

    out_writer->file = f;
    out_writer->version = core_pack_format_version_token();
    out_writer->version_major = CORE_PACK_FORMAT_VERSION_MAJOR;
    out_writer->version_minor = CORE_PACK_FORMAT_VERSION_MINOR;
    return core_result_ok();
}

CoreResult core_pack_writer_add_chunk(CorePackWriter *writer, const char type[4], const void *data, uint64_t size) {
    return core_pack_writer_add_chunk_encoded(writer, type, data, size, CORE_PACK_CODEC_NONE);
}

CoreResult core_pack_writer_add_chunk_encoded(CorePackWriter *writer,
                                              const char type[4],
                                              const void *data,
                                              uint64_t size,
                                              CorePackCodec codec) {
    if (!writer || !writer->file || !type || (!data && size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (codec != CORE_PACK_CODEC_NONE && codec != CORE_PACK_CODEC_RLE8) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unsupported codec" };
        return r;
    }

    const uint8_t *payload = (const uint8_t *)data;
    uint64_t payload_size = size;
    uint8_t *owned_payload = NULL;
    uint64_t encoded_size = size;

    if (codec == CORE_PACK_CODEC_RLE8 && size > 0) {
        CoreResult enc = rle8_encode((const uint8_t *)data, size, &owned_payload, &encoded_size);
        if (enc.code != CORE_OK) return enc;
        payload = owned_payload;
        payload_size = sizeof(CorePackCodecHeader) + encoded_size;
    } else if (codec == CORE_PACK_CODEC_NONE && size > 0) {
        payload_size = size;
    } else if (size == 0) {
        payload_size = 0;
    }

    CorePackChunkHeader ch;
    memcpy(ch.type, type, 4);
    ch.size = to_le64(payload_size);
    if (fwrite(&ch, sizeof(ch), 1, writer->file) != 1) {
        core_free(owned_payload);
        CoreResult r = { CORE_ERR_IO, "failed to write chunk header" };
        return r;
    }

    long data_pos = ftell(writer->file);
    if (data_pos < 0) {
        core_free(owned_payload);
        CoreResult r = { CORE_ERR_IO, "failed to query data offset" };
        return r;
    }

    CoreResult push = writer_index_push(writer, type, (uint64_t)data_pos, payload_size);
    if (push.code != CORE_OK) {
        core_free(owned_payload);
        return push;
    }

    if (codec == CORE_PACK_CODEC_RLE8 && size > 0) {
        CorePackCodecHeader hdr;
        hdr.magic = to_le32(CORE_PACK_CODEC_MAGIC);
        hdr.codec = to_le32((uint32_t)codec);
        hdr.decoded_size = to_le64(size);
        hdr.encoded_size = to_le64(encoded_size);
        if (fwrite(&hdr, sizeof(hdr), 1, writer->file) != 1) {
            core_free(owned_payload);
            CoreResult r = { CORE_ERR_IO, "failed to write codec header" };
            return r;
        }
        if (encoded_size > 0 && fwrite(owned_payload, 1, (size_t)encoded_size, writer->file) != (size_t)encoded_size) {
            core_free(owned_payload);
            CoreResult r = { CORE_ERR_IO, "failed to write encoded payload" };
            return r;
        }
    } else if (size > 0) {
        if (fwrite(payload, 1, (size_t)size, writer->file) != (size_t)size) {
            core_free(owned_payload);
            CoreResult r = { CORE_ERR_IO, "failed to write chunk data" };
            return r;
        }
    }

    core_free(owned_payload);
    return core_result_ok();
}

CoreResult core_pack_writer_close(CorePackWriter *writer) {
    if (!writer || !writer->file) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    long index_pos = ftell(writer->file);
    if (index_pos < 0) {
        fclose(writer->file);
        writer->file = NULL;
        core_free(writer->index_entries);
        writer->index_entries = NULL;
        writer->index_count = 0;
        writer->index_capacity = 0;
        CoreResult r = { CORE_ERR_IO, "failed to query index position" };
        return r;
    }

    CorePackIndexHeader ih;
    ih.magic = to_le32(CORE_PACK_INDEX_MAGIC);
    ih.version = to_le32(core_pack_format_version_token());
    ih.count = to_le64((uint64_t)writer->index_count);

    if (fwrite(&ih, sizeof(ih), 1, writer->file) != 1) {
        fclose(writer->file);
        writer->file = NULL;
        core_free(writer->index_entries);
        writer->index_entries = NULL;
        writer->index_count = 0;
        writer->index_capacity = 0;
        CoreResult r = { CORE_ERR_IO, "failed to write index header" };
        return r;
    }

    for (size_t i = 0; i < writer->index_count; ++i) {
        CorePackIndexEntryDisk de;
        memcpy(de.type, writer->index_entries[i].type, 4);
        de.data_offset = to_le64(writer->index_entries[i].data_offset);
        de.size = to_le64(writer->index_entries[i].size);
        if (fwrite(&de, sizeof(de), 1, writer->file) != 1) {
            fclose(writer->file);
            writer->file = NULL;
            core_free(writer->index_entries);
            writer->index_entries = NULL;
            writer->index_count = 0;
            writer->index_capacity = 0;
            CoreResult r = { CORE_ERR_IO, "failed to write index entries" };
            return r;
        }
    }

    CorePackFooter footer;
    footer.magic = to_le32(CORE_PACK_FOOTER_MAGIC);
    footer.version = to_le32(core_pack_format_version_token());
    footer.index_offset = to_le64((uint64_t)index_pos);

    if (fwrite(&footer, sizeof(footer), 1, writer->file) != 1) {
        fclose(writer->file);
        writer->file = NULL;
        core_free(writer->index_entries);
        writer->index_entries = NULL;
        writer->index_count = 0;
        writer->index_capacity = 0;
        CoreResult r = { CORE_ERR_IO, "failed to write footer" };
        return r;
    }

    if (fclose(writer->file) != 0) {
        writer->file = NULL;
        core_free(writer->index_entries);
        writer->index_entries = NULL;
        writer->index_count = 0;
        writer->index_capacity = 0;
        CoreResult r = { CORE_ERR_IO, "failed to close pack" };
        return r;
    }

    writer->file = NULL;
    writer->version = 0;
    writer->version_major = 0;
    writer->version_minor = 0;
    core_free(writer->index_entries);
    writer->index_entries = NULL;
    writer->index_count = 0;
    writer->index_capacity = 0;
    return core_result_ok();
}

CoreResult core_pack_reader_open(const char *path, CorePackReader *out_reader) {
    if (!path || !out_reader) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    memset(out_reader, 0, sizeof(*out_reader));

    FILE *f = fopen(path, "rb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open pack for read" };
        return r;
    }

    CorePackHeader h;
    if (fread(&h, sizeof(h), 1, f) != 1) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to read pack header" };
        return r;
    }

    uint32_t magic = from_le32(h.magic);
    uint32_t version = from_le32(h.version);

    if (magic != CORE_PACK_MAGIC) {
        fclose(f);
        CoreResult r = { CORE_ERR_FORMAT, "invalid pack magic" };
        return r;
    }

    uint32_t version_major = core_pack_format_version_major_from_token(version);
    uint32_t version_minor = core_pack_format_version_minor_from_token(version);
    if (version_major != CORE_PACK_FORMAT_VERSION_MAJOR || version_minor > CORE_PACK_FORMAT_VERSION_MINOR) {
        fclose(f);
        CoreResult r = { CORE_ERR_FORMAT, "unsupported pack version" };
        return r;
    }

    out_reader->file = f;
    out_reader->version = version;
    out_reader->version_major = version_major;
    out_reader->version_minor = version_minor;

    CoreResult idx = reader_load_index_from_footer(out_reader);
    if (idx.code != CORE_OK) {
        fclose(f);
        core_free(out_reader->index_entries);
        memset(out_reader, 0, sizeof(*out_reader));
        return idx;
    }

    out_reader->next_index = 0;
    return core_result_ok();
}

CoreResult core_pack_reader_next_chunk(CorePackReader *reader, CorePackChunkInfo *out_chunk) {
    if (!reader || !out_chunk) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (reader->next_index >= reader->index_count) {
        CoreResult r = { CORE_ERR_NOT_FOUND, "end of pack" };
        return r;
    }

    *out_chunk = reader->index_entries[reader->next_index++];
    return core_result_ok();
}

CoreResult core_pack_reader_get_chunk(const CorePackReader *reader, size_t chunk_index, CorePackChunkInfo *out_chunk) {
    if (!reader || !out_chunk) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (chunk_index >= reader->index_count) {
        CoreResult r = { CORE_ERR_NOT_FOUND, "chunk index out of range" };
        return r;
    }
    *out_chunk = reader->index_entries[chunk_index];
    return core_result_ok();
}

CoreResult core_pack_reader_find_chunk(const CorePackReader *reader, const char type[4], size_t occurrence, CorePackChunkInfo *out_chunk) {
    if (!reader || !type || !out_chunk) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    size_t hit = 0;
    for (size_t i = 0; i < reader->index_count; ++i) {
        if (memcmp(reader->index_entries[i].type, type, 4) == 0) {
            if (hit == occurrence) {
                *out_chunk = reader->index_entries[i];
                return core_result_ok();
            }
            hit++;
        }
    }

    CoreResult r = { CORE_ERR_NOT_FOUND, "chunk type not found" };
    return r;
}

size_t core_pack_reader_chunk_count(const CorePackReader *reader) {
    if (!reader) return 0;
    return reader->index_count;
}

CoreResult core_pack_reader_read_chunk_data(CorePackReader *reader, const CorePackChunkInfo *chunk, void *dst, uint64_t dst_size) {
    if (!reader || !chunk || (!dst && chunk->size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (dst_size < chunk->size) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "destination buffer too small" };
        return r;
    }
    return core_pack_reader_read_chunk_slice(reader, chunk, 0, dst, chunk->size);
}

CoreResult core_pack_reader_read_chunk_decoded(CorePackReader *reader,
                                               const CorePackChunkInfo *chunk,
                                               void *dst,
                                               uint64_t dst_size,
                                               uint64_t *out_decoded_size) {
    if (!reader || !chunk || (!dst && dst_size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (out_decoded_size) *out_decoded_size = 0;

    if (chunk->size < sizeof(CorePackCodecHeader)) {
        if (dst_size < chunk->size) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "destination buffer too small" };
            return r;
        }
        CoreResult raw = core_pack_reader_read_chunk_data(reader, chunk, dst, dst_size);
        if (raw.code == CORE_OK && out_decoded_size) *out_decoded_size = chunk->size;
        return raw;
    }

    CorePackCodecHeader hdr;
    CoreResult hr = core_pack_reader_read_chunk_slice(reader, chunk, 0, &hdr, sizeof(hdr));
    if (hr.code != CORE_OK) return hr;

    uint32_t magic = from_le32(hdr.magic);
    if (magic != CORE_PACK_CODEC_MAGIC) {
        if (dst_size < chunk->size) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "destination buffer too small" };
            return r;
        }
        CoreResult raw = core_pack_reader_read_chunk_data(reader, chunk, dst, dst_size);
        if (raw.code == CORE_OK && out_decoded_size) *out_decoded_size = chunk->size;
        return raw;
    }

    uint32_t codec_u32 = from_le32(hdr.codec);
    uint64_t decoded_size = from_le64(hdr.decoded_size);
    uint64_t encoded_size = from_le64(hdr.encoded_size);
    uint64_t payload_size = chunk->size - sizeof(CorePackCodecHeader);
    if (payload_size != encoded_size) {
        CoreResult r = { CORE_ERR_FORMAT, "encoded size mismatch" };
        return r;
    }
    if (dst_size < decoded_size) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "destination buffer too small" };
        return r;
    }

    if (codec_u32 == CORE_PACK_CODEC_NONE) {
        CoreResult raw = core_pack_reader_read_chunk_slice(reader, chunk, sizeof(CorePackCodecHeader), dst, decoded_size);
        if (raw.code == CORE_OK && out_decoded_size) *out_decoded_size = decoded_size;
        return raw;
    }
    if (codec_u32 == CORE_PACK_CODEC_RLE8) {
        uint8_t *enc = NULL;
        if (encoded_size > 0) {
            enc = (uint8_t *)core_alloc((size_t)encoded_size);
            if (!enc) {
                CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                return r;
            }
            CoreResult rr = core_pack_reader_read_chunk_slice(reader, chunk, sizeof(CorePackCodecHeader), enc, encoded_size);
            if (rr.code != CORE_OK) {
                core_free(enc);
                return rr;
            }
        }
        CoreResult dec = rle8_decode(enc, encoded_size, (uint8_t *)dst, decoded_size);
        core_free(enc);
        if (dec.code == CORE_OK && out_decoded_size) *out_decoded_size = decoded_size;
        return dec;
    }

    CoreResult r = { CORE_ERR_FORMAT, "unknown codec" };
    return r;
}

CoreResult core_pack_reader_read_chunk_slice(CorePackReader *reader,
                                             const CorePackChunkInfo *chunk,
                                             uint64_t offset,
                                             void *dst,
                                             uint64_t size) {
    if (!reader || !reader->file || !chunk || (!dst && size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (offset > chunk->size || size > chunk->size - offset) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "slice out of bounds" };
        return r;
    }

    uint64_t start = chunk->data_offset + offset;
    if (fseek(reader->file, (long)start, SEEK_SET) != 0) {
        CoreResult r = { CORE_ERR_IO, "failed to seek chunk data" };
        return r;
    }

    if (size > 0 && fread(dst, 1, (size_t)size, reader->file) != (size_t)size) {
        CoreResult r = { CORE_ERR_IO, "failed to read chunk data" };
        return r;
    }

    return core_result_ok();
}

CoreResult core_pack_reader_close(CorePackReader *reader) {
    if (!reader || !reader->file) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (fclose(reader->file) != 0) {
        reader->file = NULL;
        core_free(reader->index_entries);
        reader->index_entries = NULL;
        reader->index_count = 0;
        reader->index_capacity = 0;
        reader->next_index = 0;
        CoreResult r = { CORE_ERR_IO, "failed to close pack" };
        return r;
    }

    reader->file = NULL;
    reader->version = 0;
    reader->version_major = 0;
    reader->version_minor = 0;
    core_free(reader->index_entries);
    reader->index_entries = NULL;
    reader->index_count = 0;
    reader->index_capacity = 0;
    reader->next_index = 0;
    return core_result_ok();
}
