#include "core_mesh_compile.h"

#include "core_io.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CORE_MESH_COMPILE_IMPORTED_STL_MAX_TRIANGLES 250000u

typedef struct CoreMeshCompileParsedTriangle {
    size_t a;
    size_t b;
    size_t c;
} CoreMeshCompileParsedTriangle;

typedef struct CoreMeshCompileWeldEntry {
    int64_t cell_x;
    int64_t cell_y;
    int64_t cell_z;
    size_t vertex_index;
    size_t next;
} CoreMeshCompileWeldEntry;

typedef struct CoreMeshCompileWeldIndex {
    double tolerance;
    bool weld_vertices;
    size_t bucket_count;
    size_t *buckets;
    size_t entry_count;
    size_t entry_capacity;
    CoreMeshCompileWeldEntry *entries;
} CoreMeshCompileWeldIndex;

static void imported_mesh_emit_progress(CoreMeshCompileProgressCallback callback,
                                        void *user_data,
                                        CoreMeshCompileProgressStage stage,
                                        size_t current,
                                        size_t total,
                                        const char *message) {
    CoreMeshCompileProgress progress;
    if (!callback) return;
    progress.stage = stage;
    progress.current = current;
    progress.total = total;
    progress.message = message;
    callback(&progress, user_data);
}

static CoreResult imported_mesh_invalid_arg(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static CoreResult imported_mesh_copy_text(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') {
        return imported_mesh_invalid_arg("text field must be non-empty");
    }
    len = strlen(src);
    if (len >= dst_size) {
        return imported_mesh_invalid_arg("text field too long");
    }
    memcpy(dst, src, len + 1u);
    return core_result_ok();
}

static const char *imported_mesh_skip_space(const char *text) {
    while (text && *text && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static bool imported_mesh_vec3_near(CoreObjectVec3 a, CoreObjectVec3 b, double tolerance) {
    return fabs(a.x - b.x) <= tolerance && fabs(a.y - b.y) <= tolerance &&
           fabs(a.z - b.z) <= tolerance;
}

static int64_t imported_mesh_cell_for(double value, double tolerance) {
    if (tolerance <= 0.0) {
        return (int64_t)value;
    }
    if (value >= (double)INT64_MAX * tolerance) return INT64_MAX;
    if (value <= (double)INT64_MIN * tolerance) return INT64_MIN;
    return (int64_t)floor(value / tolerance);
}

static uint64_t imported_mesh_mix_u64(uint64_t value) {
    value ^= value >> 33u;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33u;
    value *= UINT64_C(0xc4ceb9fe1a85ec53);
    value ^= value >> 33u;
    return value;
}

static size_t imported_mesh_weld_bucket(int64_t cell_x,
                                        int64_t cell_y,
                                        int64_t cell_z,
                                        size_t bucket_count) {
    uint64_t h = imported_mesh_mix_u64((uint64_t)cell_x);
    h ^= imported_mesh_mix_u64((uint64_t)cell_y + UINT64_C(0x9e3779b97f4a7c15));
    h ^= imported_mesh_mix_u64((uint64_t)cell_z + UINT64_C(0xbf58476d1ce4e5b9));
    return (size_t)(h % (uint64_t)bucket_count);
}

static CoreResult imported_mesh_weld_index_init(CoreMeshCompileWeldIndex *index,
                                                size_t vertex_capacity,
                                                double tolerance,
                                                bool weld_vertices) {
    size_t bucket_count = 16u;
    if (!index) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    memset(index, 0, sizeof(*index));
    index->tolerance = tolerance;
    index->weld_vertices = weld_vertices;
    if (!weld_vertices) {
        return core_result_ok();
    }
    while (bucket_count < vertex_capacity && bucket_count <= (SIZE_MAX / 2u)) {
        bucket_count *= 2u;
    }
    if (bucket_count == 0u || bucket_count > (SIZE_MAX / sizeof(size_t))) {
        return imported_mesh_invalid_arg("weld index bucket capacity overflow");
    }
    index->buckets = (size_t *)core_alloc(bucket_count * sizeof(size_t));
    index->entries = (CoreMeshCompileWeldEntry *)core_alloc(vertex_capacity *
                                                           sizeof(CoreMeshCompileWeldEntry));
    if (!index->buckets || !index->entries) {
        core_free(index->buckets);
        core_free(index->entries);
        memset(index, 0, sizeof(*index));
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memset(index->buckets, 0, bucket_count * sizeof(size_t));
    index->bucket_count = bucket_count;
    index->entry_capacity = vertex_capacity;
    return core_result_ok();
}

static void imported_mesh_weld_index_free(CoreMeshCompileWeldIndex *index) {
    if (!index) return;
    core_free(index->buckets);
    core_free(index->entries);
    memset(index, 0, sizeof(*index));
}

static CoreResult imported_mesh_weld_index_add(CoreMeshCompileWeldIndex *index,
                                               CoreObjectVec3 vertex,
                                               size_t vertex_index) {
    CoreMeshCompileWeldEntry *entry = NULL;
    size_t bucket = 0u;
    if (!index || !index->weld_vertices) return core_result_ok();
    if (index->entry_count >= index->entry_capacity) {
        return imported_mesh_invalid_arg("weld index capacity exceeded");
    }
    entry = &index->entries[index->entry_count];
    entry->cell_x = imported_mesh_cell_for(vertex.x, index->tolerance);
    entry->cell_y = imported_mesh_cell_for(vertex.y, index->tolerance);
    entry->cell_z = imported_mesh_cell_for(vertex.z, index->tolerance);
    entry->vertex_index = vertex_index;
    bucket = imported_mesh_weld_bucket(entry->cell_x,
                                       entry->cell_y,
                                       entry->cell_z,
                                       index->bucket_count);
    entry->next = index->buckets[bucket];
    index->buckets[bucket] = index->entry_count + 1u;
    index->entry_count += 1u;
    return core_result_ok();
}

static bool imported_mesh_weld_index_find(const CoreMeshCompileWeldIndex *index,
                                          const CoreObjectVec3 *vertices,
                                          CoreObjectVec3 vertex,
                                          size_t *out_index) {
    int64_t cell_x;
    int64_t cell_y;
    int64_t cell_z;
    int dz;
    if (!index || !index->weld_vertices || !vertices || !out_index || index->bucket_count == 0u) {
        return false;
    }
    cell_x = imported_mesh_cell_for(vertex.x, index->tolerance);
    cell_y = imported_mesh_cell_for(vertex.y, index->tolerance);
    cell_z = imported_mesh_cell_for(vertex.z, index->tolerance);
    for (dz = -1; dz <= 1; ++dz) {
        int dy;
        for (dy = -1; dy <= 1; ++dy) {
            int dx;
            for (dx = -1; dx <= 1; ++dx) {
                int64_t nx = cell_x + dx;
                int64_t ny = cell_y + dy;
                int64_t nz = cell_z + dz;
                size_t bucket;
                size_t entry_ref;
                if ((dx < 0 && cell_x == INT64_MIN) || (dx > 0 && cell_x == INT64_MAX) ||
                    (dy < 0 && cell_y == INT64_MIN) || (dy > 0 && cell_y == INT64_MAX) ||
                    (dz < 0 && cell_z == INT64_MIN) || (dz > 0 && cell_z == INT64_MAX)) {
                    continue;
                }
                bucket = imported_mesh_weld_bucket(nx, ny, nz, index->bucket_count);
                entry_ref = index->buckets[bucket];
                while (entry_ref != 0u) {
                    const CoreMeshCompileWeldEntry *entry = &index->entries[entry_ref - 1u];
                    if (entry->cell_x == nx && entry->cell_y == ny && entry->cell_z == nz &&
                        imported_mesh_vec3_near(vertices[entry->vertex_index],
                                                vertex,
                                                index->tolerance)) {
                        *out_index = entry->vertex_index;
                        return true;
                    }
                    entry_ref = entry->next;
                }
            }
        }
    }
    return false;
}

static uint32_t imported_mesh_read_u32_le(const unsigned char *bytes) {
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static float imported_mesh_read_f32_le(const unsigned char *bytes) {
    uint32_t bits = imported_mesh_read_u32_le(bytes);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static CoreResult imported_mesh_source_path(const CoreMeshAssetImportedMeshSource *source,
                                            const char *source_root,
                                            char *out_path,
                                            size_t out_path_size) {
    int written;
    if (!source || !out_path || out_path_size == 0u) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    if (source->source_uri[0] == '/') {
        return imported_mesh_copy_text(out_path, out_path_size, source->source_uri);
    }
    if (!source_root || source_root[0] == '\0') {
        return imported_mesh_copy_text(out_path, out_path_size, source->source_uri);
    }
    written = snprintf(out_path, out_path_size, "%s/%s", source_root, source->source_uri);
    if (written < 0 || (size_t)written >= out_path_size) {
        return imported_mesh_invalid_arg("source path is too long");
    }
    return core_result_ok();
}

static CoreResult imported_mesh_add_vertex(CoreObjectVec3 *vertices,
                                           size_t *vertex_count,
                                           size_t vertex_capacity,
                                           CoreMeshCompileWeldIndex *weld_index,
                                           CoreObjectVec3 vertex,
                                           size_t *out_index) {
    CoreResult r;
    if (!vertices || !vertex_count || !out_index) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    if (imported_mesh_weld_index_find(weld_index, vertices, vertex, out_index)) {
        return core_result_ok();
    }
    if (*vertex_count >= vertex_capacity) {
        return imported_mesh_invalid_arg("vertex capacity exceeded");
    }
    vertices[*vertex_count] = vertex;
    *out_index = *vertex_count;
    *vertex_count += 1u;
    r = imported_mesh_weld_index_add(weld_index, vertex, *out_index);
    if (r.code != CORE_OK) {
        return r;
    }
    return core_result_ok();
}

static CoreResult imported_mesh_validate_triangle_count(size_t triangle_count) {
    if (triangle_count == 0u) {
        return imported_mesh_invalid_arg("imported STL triangle count is zero");
    }
    if (triangle_count > CORE_MESH_COMPILE_IMPORTED_STL_MAX_TRIANGLES) {
        return imported_mesh_invalid_arg("imported STL triangle count exceeds proof limit");
    }
    return core_result_ok();
}

static bool imported_mesh_parse_ascii_vertex_line(const char *line,
                                                  double *out_x,
                                                  double *out_y,
                                                  double *out_z) {
    char *end = NULL;
    double x;
    double y;
    double z;
    if (!line || !out_x || !out_y || !out_z) return false;
    if (strncmp(line, "vertex", 6u) != 0 || !isspace((unsigned char)line[6])) {
        return false;
    }
    line = imported_mesh_skip_space(line + 6u);
    x = strtod(line, &end);
    if (end == line) return false;
    line = imported_mesh_skip_space(end);
    y = strtod(line, &end);
    if (end == line) return false;
    line = imported_mesh_skip_space(end);
    z = strtod(line, &end);
    if (end == line) return false;
    *out_x = x;
    *out_y = y;
    *out_z = z;
    return true;
}

static double imported_mesh_triangle_points_area_sq(CoreObjectVec3 va,
                                                    CoreObjectVec3 vb,
                                                    CoreObjectVec3 vc) {
    double ux = vb.x - va.x;
    double uy = vb.y - va.y;
    double uz = vb.z - va.z;
    double vx = vc.x - va.x;
    double vy = vc.y - va.y;
    double vz = vc.z - va.z;
    double nx = uy * vz - uz * vy;
    double ny = uz * vx - ux * vz;
    double nz = ux * vy - uy * vx;
    return nx * nx + ny * ny + nz * nz;
}

static bool imported_mesh_triangle_points_are_degenerate(CoreObjectVec3 a,
                                                         CoreObjectVec3 b,
                                                         CoreObjectVec3 c) {
    return imported_mesh_triangle_points_area_sq(a, b, c) <= 1e-18;
}

static bool imported_mesh_triangle_indices_are_degenerate(const CoreObjectVec3 *vertices,
                                                          size_t a,
                                                          size_t b,
                                                          size_t c) {
    if (a == b || a == c || b == c) {
        return true;
    }
    return imported_mesh_triangle_points_are_degenerate(vertices[a], vertices[b], vertices[c]);
}

static CoreResult imported_mesh_parse_ascii_stl(const char *path,
                                                const CoreMeshAssetImportedMeshSource *source,
                                                CoreMeshCompileProgressCallback progress_callback,
                                                void *progress_user_data,
                                                CoreObjectVec3 **out_vertices,
                                                size_t *out_vertex_count,
                                                CoreMeshCompileParsedTriangle **out_triangles,
                                                size_t *out_triangle_count) {
    CoreBuffer file_data = {0};
    char *text = NULL;
    const char *cursor = NULL;
    size_t vertex_line_count = 0u;
    size_t parsed_vertex_in_triangle = 0u;
    size_t vertex_count = 0u;
    size_t triangle_count = 0u;
    size_t last_scan_progress_offset = 0u;
    size_t last_parse_progress_offset = 0u;
    size_t current_triangle_indices[3] = {0u, 0u, 0u};
    CoreObjectVec3 current_triangle_vertices[3];
    CoreObjectVec3 *vertices = NULL;
    CoreMeshCompileParsedTriangle *triangles = NULL;
    CoreMeshCompileWeldIndex weld_index;
    CoreResult r;
    size_t i;

    memset(current_triangle_vertices, 0, sizeof(current_triangle_vertices));
    memset(&weld_index, 0, sizeof(weld_index));
    if (!path || !source || !out_vertices || !out_vertex_count || !out_triangles ||
        !out_triangle_count) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    *out_vertices = NULL;
    *out_vertex_count = 0u;
    *out_triangles = NULL;
    *out_triangle_count = 0u;

    imported_mesh_emit_progress(progress_callback,
                                progress_user_data,
                                CORE_MESH_COMPILE_PROGRESS_STAGE_READING_SOURCE,
                                0u,
                                0u,
                                "Reading ASCII STL");
    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK) {
        return r;
    }
    text = (char *)core_alloc(file_data.size + 1u);
    if (!text) {
        core_io_buffer_free(&file_data);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (file_data.size > 0u && file_data.data) {
        memcpy(text, file_data.data, file_data.size);
    }
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    cursor = text;
    while (*cursor) {
        const char *line = imported_mesh_skip_space(cursor);
        if (strncmp(line, "vertex", 6u) == 0 && isspace((unsigned char)line[6])) {
            ++vertex_line_count;
        }
        if ((size_t)(cursor - text) >= last_scan_progress_offset + 262144u) {
            last_scan_progress_offset = (size_t)(cursor - text);
            imported_mesh_emit_progress(progress_callback,
                                        progress_user_data,
                                        CORE_MESH_COMPILE_PROGRESS_STAGE_SCANNING_STL,
                                        last_scan_progress_offset,
                                        file_data.size,
                                        "Scanning ASCII STL");
        }
        cursor = strchr(cursor, '\n');
        if (!cursor) {
            break;
        }
        ++cursor;
    }
    if (vertex_line_count == 0u || vertex_line_count % 3u != 0u) {
        core_free(text);
        return imported_mesh_invalid_arg("ASCII STL vertex line count is invalid");
    }
    r = imported_mesh_validate_triangle_count(vertex_line_count / 3u);
    if (r.code != CORE_OK) {
        core_free(text);
        return r;
    }
    vertices = (CoreObjectVec3 *)core_alloc(vertex_line_count * sizeof(CoreObjectVec3));
    triangles = (CoreMeshCompileParsedTriangle *)core_alloc((vertex_line_count / 3u) *
                                                            sizeof(CoreMeshCompileParsedTriangle));
    if (!vertices || !triangles) {
        core_free(vertices);
        core_free(triangles);
        core_free(text);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    r = imported_mesh_weld_index_init(&weld_index,
                                      vertex_line_count,
                                      source->weld_tolerance,
                                      source->weld_vertices);
    if (r.code != CORE_OK) {
        core_free(vertices);
        core_free(triangles);
        core_free(text);
        return r;
    }

    cursor = text;
    while (*cursor) {
        double x;
        double y;
        double z;
        const char *line = imported_mesh_skip_space(cursor);
        if (imported_mesh_parse_ascii_vertex_line(line, &x, &y, &z)) {
            CoreObjectVec3 vertex;
            vertex.x = x * source->source_to_asset_scale;
            vertex.y = y * source->source_to_asset_scale;
            vertex.z = z * source->source_to_asset_scale;
            if (!isfinite(vertex.x) || !isfinite(vertex.y) || !isfinite(vertex.z)) {
                imported_mesh_weld_index_free(&weld_index);
                core_free(vertices);
                core_free(triangles);
                core_free(text);
                return imported_mesh_invalid_arg("ASCII STL vertex is not finite");
            }
            current_triangle_vertices[parsed_vertex_in_triangle] = vertex;
            parsed_vertex_in_triangle += 1u;
            if (parsed_vertex_in_triangle == 3u) {
                if (!imported_mesh_triangle_points_are_degenerate(current_triangle_vertices[0],
                                                                  current_triangle_vertices[1],
                                                                  current_triangle_vertices[2])) {
                    for (i = 0u; i < 3u; ++i) {
                        r = imported_mesh_add_vertex(vertices,
                                                     &vertex_count,
                                                     vertex_line_count,
                                                     &weld_index,
                                                     current_triangle_vertices[i],
                                                     &current_triangle_indices[i]);
                        if (r.code != CORE_OK) {
                            imported_mesh_weld_index_free(&weld_index);
                            core_free(vertices);
                            core_free(triangles);
                            core_free(text);
                            return r;
                        }
                    }
                    if (!imported_mesh_triangle_indices_are_degenerate(vertices,
                                                                       current_triangle_indices[0],
                                                                       current_triangle_indices[1],
                                                                       current_triangle_indices[2])) {
                        triangles[triangle_count].a = current_triangle_indices[0];
                        triangles[triangle_count].b = current_triangle_indices[1];
                        triangles[triangle_count].c = current_triangle_indices[2];
                        triangle_count += 1u;
                    }
                }
                if ((size_t)(cursor - text) >= last_parse_progress_offset + 262144u) {
                    last_parse_progress_offset = (size_t)(cursor - text);
                    imported_mesh_emit_progress(progress_callback,
                                                progress_user_data,
                                                CORE_MESH_COMPILE_PROGRESS_STAGE_PARSING_STL,
                                                last_parse_progress_offset,
                                                file_data.size,
                                                "Parsing ASCII STL");
                }
                parsed_vertex_in_triangle = 0u;
            }
        }
        cursor = strchr(cursor, '\n');
        if (!cursor) {
            break;
        }
        ++cursor;
    }
    if (parsed_vertex_in_triangle != 0u || triangle_count == 0u || vertex_count == 0u) {
        imported_mesh_weld_index_free(&weld_index);
        core_free(vertices);
        core_free(triangles);
        core_free(text);
        return imported_mesh_invalid_arg("ASCII STL contains no non-degenerate triangles");
    }

    imported_mesh_weld_index_free(&weld_index);
    core_free(text);
    *out_vertices = vertices;
    *out_vertex_count = vertex_count;
    *out_triangles = triangles;
    *out_triangle_count = triangle_count;
    return core_result_ok();
}

static bool imported_mesh_binary_stl_layout_valid(const CoreBuffer *file_data,
                                                  uint32_t *out_triangle_count) {
    uint32_t triangle_count = 0u;
    size_t expected_size = 0u;
    if (out_triangle_count) *out_triangle_count = 0u;
    if (!file_data || !file_data->data || file_data->size < 84u) return false;
    triangle_count = imported_mesh_read_u32_le((const unsigned char *)file_data->data + 80u);
    if ((size_t)triangle_count > (SIZE_MAX - 84u) / 50u) return false;
    expected_size = 84u + ((size_t)triangle_count * 50u);
    if (expected_size != file_data->size || triangle_count == 0u) return false;
    if (out_triangle_count) *out_triangle_count = triangle_count;
    return true;
}

static CoreResult imported_mesh_parse_binary_stl(const char *path,
                                                 const CoreMeshAssetImportedMeshSource *source,
                                                 CoreMeshCompileProgressCallback progress_callback,
                                                 void *progress_user_data,
                                                 CoreObjectVec3 **out_vertices,
                                                 size_t *out_vertex_count,
                                                 CoreMeshCompileParsedTriangle **out_triangles,
                                                 size_t *out_triangle_count) {
    CoreBuffer file_data = {0};
    uint32_t binary_triangle_count = 0u;
    size_t vertex_capacity = 0u;
    size_t vertex_count = 0u;
    size_t triangle_count = 0u;
    CoreObjectVec3 *vertices = NULL;
    CoreMeshCompileParsedTriangle *triangles = NULL;
    CoreMeshCompileWeldIndex weld_index;
    CoreResult r;
    size_t i;

    memset(&weld_index, 0, sizeof(weld_index));
    if (!path || !source || !out_vertices || !out_vertex_count || !out_triangles ||
        !out_triangle_count) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    *out_vertices = NULL;
    *out_vertex_count = 0u;
    *out_triangles = NULL;
    *out_triangle_count = 0u;

    if (sizeof(float) != 4u) {
        return imported_mesh_invalid_arg("binary STL requires 32-bit float support");
    }
    imported_mesh_emit_progress(progress_callback,
                                progress_user_data,
                                CORE_MESH_COMPILE_PROGRESS_STAGE_READING_SOURCE,
                                0u,
                                0u,
                                "Reading binary STL");
    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK) {
        return r;
    }
    if (!imported_mesh_binary_stl_layout_valid(&file_data, &binary_triangle_count)) {
        core_io_buffer_free(&file_data);
        return imported_mesh_invalid_arg("binary STL layout is invalid");
    }
    r = imported_mesh_validate_triangle_count((size_t)binary_triangle_count);
    if (r.code != CORE_OK) {
        core_io_buffer_free(&file_data);
        return r;
    }
    vertex_capacity = (size_t)binary_triangle_count * 3u;
    vertices = (CoreObjectVec3 *)core_alloc(vertex_capacity * sizeof(CoreObjectVec3));
    triangles = (CoreMeshCompileParsedTriangle *)core_alloc((size_t)binary_triangle_count *
                                                            sizeof(CoreMeshCompileParsedTriangle));
    if (!vertices || !triangles) {
        core_free(vertices);
        core_free(triangles);
        core_io_buffer_free(&file_data);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    r = imported_mesh_weld_index_init(&weld_index,
                                      vertex_capacity,
                                      source->weld_tolerance,
                                      source->weld_vertices);
    if (r.code != CORE_OK) {
        core_free(vertices);
        core_free(triangles);
        core_io_buffer_free(&file_data);
        return r;
    }

    for (i = 0u; i < (size_t)binary_triangle_count; ++i) {
        const unsigned char *triangle =
            (const unsigned char *)file_data.data + 84u + (i * 50u);
        size_t triangle_indices[3] = {0u, 0u, 0u};
        CoreObjectVec3 triangle_vertices[3];
        size_t j;
        memset(triangle_vertices, 0, sizeof(triangle_vertices));
        for (j = 0u; j < 3u; ++j) {
            const unsigned char *vertex_bytes = triangle + 12u + (j * 12u);
            CoreObjectVec3 vertex;
            vertex.x = (double)imported_mesh_read_f32_le(vertex_bytes) *
                       source->source_to_asset_scale;
            vertex.y = (double)imported_mesh_read_f32_le(vertex_bytes + 4u) *
                       source->source_to_asset_scale;
            vertex.z = (double)imported_mesh_read_f32_le(vertex_bytes + 8u) *
                       source->source_to_asset_scale;
            if (!isfinite(vertex.x) || !isfinite(vertex.y) || !isfinite(vertex.z)) {
                imported_mesh_weld_index_free(&weld_index);
                core_free(vertices);
                core_free(triangles);
                core_io_buffer_free(&file_data);
                return imported_mesh_invalid_arg("binary STL vertex is not finite");
            }
            triangle_vertices[j] = vertex;
        }
        if (imported_mesh_triangle_points_are_degenerate(triangle_vertices[0],
                                                         triangle_vertices[1],
                                                         triangle_vertices[2])) {
            continue;
        }
        for (j = 0u; j < 3u; ++j) {
            r = imported_mesh_add_vertex(vertices,
                                         &vertex_count,
                                         vertex_capacity,
                                         &weld_index,
                                         triangle_vertices[j],
                                         &triangle_indices[j]);
            if (r.code != CORE_OK) {
                imported_mesh_weld_index_free(&weld_index);
                core_free(vertices);
                core_free(triangles);
                core_io_buffer_free(&file_data);
                return r;
            }
        }
        if (!imported_mesh_triangle_indices_are_degenerate(vertices,
                                                           triangle_indices[0],
                                                           triangle_indices[1],
                                                           triangle_indices[2])) {
            triangles[triangle_count].a = triangle_indices[0];
            triangles[triangle_count].b = triangle_indices[1];
            triangles[triangle_count].c = triangle_indices[2];
            triangle_count += 1u;
        }
        if ((i % 1024u) == 0u || i + 1u == (size_t)binary_triangle_count) {
            imported_mesh_emit_progress(progress_callback,
                                        progress_user_data,
                                        CORE_MESH_COMPILE_PROGRESS_STAGE_PARSING_STL,
                                        i + 1u,
                                        (size_t)binary_triangle_count,
                                        "Parsing binary STL");
        }
    }
    if (triangle_count == 0u || vertex_count == 0u) {
        imported_mesh_weld_index_free(&weld_index);
        core_free(vertices);
        core_free(triangles);
        core_io_buffer_free(&file_data);
        return imported_mesh_invalid_arg("binary STL contains no non-degenerate triangles");
    }
    imported_mesh_weld_index_free(&weld_index);
    core_io_buffer_free(&file_data);

    *out_vertices = vertices;
    *out_vertex_count = vertex_count;
    *out_triangles = triangles;
    *out_triangle_count = triangle_count;
    return core_result_ok();
}

static CoreResult imported_mesh_parse_stl(const char *path,
                                          const CoreMeshAssetImportedMeshSource *source,
                                          CoreMeshCompileProgressCallback progress_callback,
                                          void *progress_user_data,
                                          CoreObjectVec3 **out_vertices,
                                          size_t *out_vertex_count,
                                          CoreMeshCompileParsedTriangle **out_triangles,
                                          size_t *out_triangle_count) {
    CoreBuffer file_data = {0};
    uint32_t binary_triangle_count = 0u;
    CoreResult r;
    if (!path || !source || !out_vertices || !out_vertex_count || !out_triangles ||
        !out_triangle_count) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK) {
        return r;
    }
    if (imported_mesh_binary_stl_layout_valid(&file_data, &binary_triangle_count)) {
        core_io_buffer_free(&file_data);
        return imported_mesh_parse_binary_stl(path,
                                              source,
                                              progress_callback,
                                              progress_user_data,
                                              out_vertices,
                                              out_vertex_count,
                                              out_triangles,
                                              out_triangle_count);
    }
    core_io_buffer_free(&file_data);
    return imported_mesh_parse_ascii_stl(path,
                                         source,
                                         progress_callback,
                                         progress_user_data,
                                         out_vertices,
                                         out_vertex_count,
                                         out_triangles,
                                         out_triangle_count);
}

static void imported_mesh_compute_bounds(const CoreObjectVec3 *vertices,
                                         size_t vertex_count,
                                         CoreMeshAssetBounds3 *out_bounds) {
    size_t i;
    out_bounds->min = vertices[0];
    out_bounds->max = vertices[0];
    for (i = 1u; i < vertex_count; ++i) {
        if (vertices[i].x < out_bounds->min.x) out_bounds->min.x = vertices[i].x;
        if (vertices[i].y < out_bounds->min.y) out_bounds->min.y = vertices[i].y;
        if (vertices[i].z < out_bounds->min.z) out_bounds->min.z = vertices[i].z;
        if (vertices[i].x > out_bounds->max.x) out_bounds->max.x = vertices[i].x;
        if (vertices[i].y > out_bounds->max.y) out_bounds->max.y = vertices[i].y;
        if (vertices[i].z > out_bounds->max.z) out_bounds->max.z = vertices[i].z;
    }
}

CoreResult core_mesh_compile_imported_mesh_to_runtime_document_with_progress(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    CoreMeshAssetRuntimeDocument *out_document,
    CoreMeshCompileProgressCallback progress_callback,
    void *progress_user_data) {
    CoreMeshCompileAuthoringContract compile_contract;
    char source_path[512];
    CoreObjectVec3 *parsed_vertices = NULL;
    CoreMeshCompileParsedTriangle *parsed_triangles = NULL;
    size_t parsed_vertex_count = 0u;
    size_t parsed_triangle_count = 0u;
    CoreResult r;
    size_t i;

    if (!document || !runtime_asset_id || !out_document) {
        return imported_mesh_invalid_arg("invalid argument");
    }
    imported_mesh_emit_progress(progress_callback,
                                progress_user_data,
                                CORE_MESH_COMPILE_PROGRESS_STAGE_PREPARING,
                                0u,
                                0u,
                                "Preparing imported mesh");
    if (document->contract.source_mode != CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        return imported_mesh_invalid_arg("document source_mode must be imported_mesh");
    }
    if (!document->has_imported_mesh_source) {
        return imported_mesh_invalid_arg("imported mesh source is required");
    }
    r = core_mesh_compile_authoring_contract_prepare(&compile_contract, document, runtime_asset_id);
    if (r.code != CORE_OK) {
        return r;
    }
    if (document->imported_mesh_source.source_format !=
        CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL) {
        return imported_mesh_invalid_arg("only STL imported mesh compile is supported");
    }
    r = imported_mesh_source_path(&document->imported_mesh_source,
                                  source_root,
                                  source_path,
                                  sizeof(source_path));
    if (r.code != CORE_OK) {
        return r;
    }
    r = imported_mesh_parse_stl(source_path,
                                &document->imported_mesh_source,
                                progress_callback,
                                progress_user_data,
                                &parsed_vertices,
                                &parsed_vertex_count,
                                &parsed_triangles,
                                &parsed_triangle_count);
    if (r.code != CORE_OK) {
        return r;
    }

    imported_mesh_emit_progress(progress_callback,
                                progress_user_data,
                                CORE_MESH_COMPILE_PROGRESS_STAGE_EMITTING_RUNTIME,
                                0u,
                                parsed_triangle_count,
                                "Emitting runtime mesh");
    core_mesh_asset_runtime_document_init(out_document);
    r = core_mesh_asset_runtime_contract_set_asset_id(&out_document->contract, runtime_asset_id);
    if (r.code != CORE_OK) goto fail;
    r = core_mesh_asset_runtime_contract_set_source_asset_id(&out_document->contract,
                                                             document->contract.asset_id);
    if (r.code != CORE_OK) goto fail;
    out_document->contract.asset_type = document->contract.asset_type;
    out_document->contract.pivot = document->contract.pivot;
    out_document->contract.vertex_count = parsed_vertex_count;
    out_document->contract.triangle_count = parsed_triangle_count;
    out_document->contract.topology_closed_volume =
        document->imported_mesh_source.topology_closed_volume_observed;
    out_document->contract.topology_manifold_expected =
        document->imported_mesh_source.topology_manifold_observed;
    imported_mesh_compute_bounds(parsed_vertices,
                                 parsed_vertex_count,
                                 &out_document->contract.local_bounds);

    r = core_mesh_asset_runtime_document_set_vertex_count(out_document, parsed_vertex_count);
    if (r.code != CORE_OK) goto fail;
    r = core_mesh_asset_runtime_document_set_triangle_count(out_document, parsed_triangle_count);
    if (r.code != CORE_OK) goto fail;
    r = core_mesh_asset_runtime_document_set_surface_group_count(out_document, 1u);
    if (r.code != CORE_OK) goto fail;

    for (i = 0u; i < parsed_vertex_count; ++i) {
        out_document->vertices[i].position = parsed_vertices[i];
    }
    for (i = 0u; i < parsed_triangle_count; ++i) {
        out_document->triangles[i].a = parsed_triangles[i].a;
        out_document->triangles[i].b = parsed_triangles[i].b;
        out_document->triangles[i].c = parsed_triangles[i].c;
        r = imported_mesh_copy_text(out_document->triangles[i].surface_group_id,
                                    sizeof(out_document->triangles[i].surface_group_id),
                                    document->imported_mesh_source.default_surface_group_id);
        if (r.code != CORE_OK) goto fail;
        if ((i % 4096u) == 0u || i + 1u == parsed_triangle_count) {
            imported_mesh_emit_progress(progress_callback,
                                        progress_user_data,
                                        CORE_MESH_COMPILE_PROGRESS_STAGE_EMITTING_RUNTIME,
                                        i + 1u,
                                        parsed_triangle_count,
                                        "Emitting runtime mesh");
        }
    }
    r = imported_mesh_copy_text(out_document->surface_groups[0].group_id,
                                sizeof(out_document->surface_groups[0].group_id),
                                document->imported_mesh_source.default_surface_group_id);
    if (r.code != CORE_OK) goto fail;
    out_document->surface_groups[0].triangle_start = 0u;
    out_document->surface_groups[0].triangle_count = parsed_triangle_count;

    r = core_mesh_asset_runtime_document_validate(out_document);
    if (r.code != CORE_OK) goto fail;

    core_free(parsed_vertices);
    core_free(parsed_triangles);
    imported_mesh_emit_progress(progress_callback,
                                progress_user_data,
                                CORE_MESH_COMPILE_PROGRESS_STAGE_COMPLETE,
                                1u,
                                1u,
                                "Runtime mesh compiled");
    return core_result_ok();

fail:
    core_free(parsed_vertices);
    core_free(parsed_triangles);
    core_mesh_asset_runtime_document_free(out_document);
    return r;
}

CoreResult core_mesh_compile_imported_mesh_to_runtime_document(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    CoreMeshAssetRuntimeDocument *out_document) {
    return core_mesh_compile_imported_mesh_to_runtime_document_with_progress(document,
                                                                            source_root,
                                                                            runtime_asset_id,
                                                                            out_document,
                                                                            NULL,
                                                                            NULL);
}

CoreResult core_mesh_compile_imported_mesh_to_runtime_file(
    const CoreMeshAssetAuthoringDocument *document,
    const char *source_root,
    const char *runtime_asset_id,
    const char *runtime_output_path) {
    CoreMeshAssetRuntimeDocument runtime_document;
    CoreResult r;

    if (!runtime_output_path) {
        return imported_mesh_invalid_arg("runtime output path is required");
    }
    core_mesh_asset_runtime_document_init(&runtime_document);
    r = core_mesh_compile_imported_mesh_to_runtime_document(document,
                                                            source_root,
                                                            runtime_asset_id,
                                                            &runtime_document);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_runtime_document_save_file(&runtime_document, runtime_output_path);
    core_mesh_asset_runtime_document_free(&runtime_document);
    return r;
}
