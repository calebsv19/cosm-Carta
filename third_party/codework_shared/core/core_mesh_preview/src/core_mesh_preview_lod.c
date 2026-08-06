#include "core_mesh_preview.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CORE_MESH_PREVIEW_LOD_MAX_CLUSTER_RESOLUTION 96

typedef struct CoreMeshPreviewClusterTriangle {
    uint32_t key_a;
    uint32_t key_b;
    uint32_t key_c;
    uint32_t a;
    uint32_t b;
    uint32_t c;
} CoreMeshPreviewClusterTriangle;

static CoreResult lod_invalid(const char *message) {
    CoreResult result = {CORE_ERR_INVALID_ARG, message};
    return result;
}

static CoreResult lod_nomem(const char *message) {
    CoreResult result = {CORE_ERR_OUT_OF_MEMORY, message};
    return result;
}

void core_mesh_preview_lod_mesh_init(CoreMeshPreviewLodMesh *mesh) {
    if (!mesh) return;
    memset(mesh, 0, sizeof(*mesh));
}

void core_mesh_preview_lod_mesh_free(CoreMeshPreviewLodMesh *mesh) {
    if (!mesh) return;
    free(mesh->vertices);
    free(mesh->indices);
    core_mesh_preview_lod_mesh_init(mesh);
}

static CoreResult lod_copy_runtime_mesh(const CoreMeshAssetRuntimeDocument *document,
                                        CoreMeshPreviewLodMesh *out_mesh) {
    if (document->vertex_count > UINT32_MAX ||
        document->triangle_count > SIZE_MAX / (3u * sizeof(uint32_t))) {
        return lod_invalid("runtime mesh is too large for indexed preview LOD");
    }
    out_mesh->vertices = (CoreObjectVec3 *)calloc(document->vertex_count,
                                                  sizeof(CoreObjectVec3));
    out_mesh->indices = (uint32_t *)calloc(document->triangle_count * 3u,
                                           sizeof(uint32_t));
    if (!out_mesh->vertices || !out_mesh->indices) {
        core_mesh_preview_lod_mesh_free(out_mesh);
        return lod_nomem("failed to allocate exact preview LOD");
    }
    for (size_t i = 0u; i < document->vertex_count; ++i) {
        out_mesh->vertices[i] = document->vertices[i].position;
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        if (triangle->a > UINT32_MAX || triangle->b > UINT32_MAX || triangle->c > UINT32_MAX) {
            core_mesh_preview_lod_mesh_free(out_mesh);
            return lod_invalid("runtime mesh index exceeds preview LOD range");
        }
        out_mesh->indices[i * 3u + 0u] = (uint32_t)triangle->a;
        out_mesh->indices[i * 3u + 1u] = (uint32_t)triangle->b;
        out_mesh->indices[i * 3u + 2u] = (uint32_t)triangle->c;
    }
    out_mesh->vertex_count = document->vertex_count;
    out_mesh->triangle_count = document->triangle_count;
    out_mesh->source_vertex_count = document->vertex_count;
    out_mesh->source_triangle_count = document->triangle_count;
    return core_result_ok();
}

static int lod_cluster_coordinate(double value,
                                  double minimum,
                                  double maximum,
                                  int resolution) {
    const double extent = maximum - minimum;
    double normalized = 0.0;
    int coordinate = 0;
    if (resolution <= 1 || !isfinite(extent) || extent <= DBL_EPSILON) return 0;
    normalized = (value - minimum) / extent;
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;
    coordinate = (int)floor(normalized * (double)resolution);
    if (coordinate >= resolution) coordinate = resolution - 1;
    if (coordinate < 0) coordinate = 0;
    return coordinate;
}

static int lod_compare_cluster_triangles(const void *lhs, const void *rhs) {
    const CoreMeshPreviewClusterTriangle *a =
        (const CoreMeshPreviewClusterTriangle *)lhs;
    const CoreMeshPreviewClusterTriangle *b =
        (const CoreMeshPreviewClusterTriangle *)rhs;
    if (a->key_a != b->key_a) return a->key_a < b->key_a ? -1 : 1;
    if (a->key_b != b->key_b) return a->key_b < b->key_b ? -1 : 1;
    if (a->key_c != b->key_c) return a->key_c < b->key_c ? -1 : 1;
    return 0;
}

static CoreResult lod_build_at_resolution(const CoreMeshAssetRuntimeDocument *document,
                                          int resolution,
                                          CoreMeshPreviewLodMesh *out_mesh) {
    size_t cell_count = 0u;
    int32_t *cell_to_cluster = NULL;
    uint32_t *vertex_map = NULL;
    CoreObjectVec3 *sums = NULL;
    uint32_t *counts = NULL;
    CoreObjectVec3 *vertices = NULL;
    uint32_t *indices = NULL;
    CoreMeshPreviewClusterTriangle *clustered_triangles = NULL;
    size_t cluster_count = 0u;
    size_t triangle_count = 0u;
    CoreResult result = core_result_ok();
    const CoreMeshAssetBounds3 bounds = document->contract.local_bounds;

    if ((size_t)resolution > SIZE_MAX / (size_t)resolution) {
        return lod_invalid("preview LOD cluster grid overflows");
    }
    cell_count = (size_t)resolution * (size_t)resolution;
    if (cell_count > SIZE_MAX / (size_t)resolution) {
        return lod_invalid("preview LOD cluster grid overflows");
    }
    cell_count *= (size_t)resolution;
    if (document->triangle_count > SIZE_MAX / (3u * sizeof(uint32_t))) {
        return lod_invalid("preview LOD index allocation overflows");
    }

    cell_to_cluster = (int32_t *)malloc(cell_count * sizeof(int32_t));
    vertex_map = (uint32_t *)malloc(document->vertex_count * sizeof(uint32_t));
    sums = (CoreObjectVec3 *)calloc(document->vertex_count, sizeof(CoreObjectVec3));
    counts = (uint32_t *)calloc(document->vertex_count, sizeof(uint32_t));
    vertices = (CoreObjectVec3 *)calloc(document->vertex_count, sizeof(CoreObjectVec3));
    indices = (uint32_t *)malloc(document->triangle_count * 3u * sizeof(uint32_t));
    clustered_triangles = (CoreMeshPreviewClusterTriangle *)malloc(
        document->triangle_count * sizeof(CoreMeshPreviewClusterTriangle));
    if (!cell_to_cluster || !vertex_map || !sums || !counts || !vertices || !indices ||
        !clustered_triangles) {
        result = lod_nomem("failed to allocate clustered preview LOD");
        goto fail;
    }
    for (size_t i = 0u; i < cell_count; ++i) cell_to_cluster[i] = -1;

    for (size_t i = 0u; i < document->vertex_count; ++i) {
        const CoreObjectVec3 position = document->vertices[i].position;
        const int x = lod_cluster_coordinate(position.x, bounds.min.x, bounds.max.x, resolution);
        const int y = lod_cluster_coordinate(position.y, bounds.min.y, bounds.max.y, resolution);
        const int z = lod_cluster_coordinate(position.z, bounds.min.z, bounds.max.z, resolution);
        const size_t cell = (size_t)x + ((size_t)y * (size_t)resolution) +
                            ((size_t)z * (size_t)resolution * (size_t)resolution);
        int32_t cluster = cell_to_cluster[cell];
        if (cluster < 0) {
            if (cluster_count >= UINT32_MAX) {
                result = lod_invalid("preview LOD cluster count exceeds index range");
                goto fail;
            }
            cluster = (int32_t)cluster_count++;
            cell_to_cluster[cell] = cluster;
        }
        vertex_map[i] = (uint32_t)cluster;
        sums[cluster].x += position.x;
        sums[cluster].y += position.y;
        sums[cluster].z += position.z;
        counts[cluster]++;
    }

    for (size_t i = 0u; i < cluster_count; ++i) {
        const double inverse = counts[i] > 0u ? 1.0 / (double)counts[i] : 1.0;
        vertices[i].x = sums[i].x * inverse;
        vertices[i].y = sums[i].y * inverse;
        vertices[i].z = sums[i].z * inverse;
    }

    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *source = &document->triangles[i];
        CoreMeshPreviewClusterTriangle triangle;
        uint32_t swap = 0u;
        if (source->a >= document->vertex_count || source->b >= document->vertex_count ||
            source->c >= document->vertex_count) {
            continue;
        }
        triangle.a = vertex_map[source->a];
        triangle.b = vertex_map[source->b];
        triangle.c = vertex_map[source->c];
        if (triangle.a == triangle.b || triangle.b == triangle.c || triangle.c == triangle.a) {
            continue;
        }
        triangle.key_a = triangle.a;
        triangle.key_b = triangle.b;
        triangle.key_c = triangle.c;
        if (triangle.key_a > triangle.key_b) {
            swap = triangle.key_a; triangle.key_a = triangle.key_b; triangle.key_b = swap;
        }
        if (triangle.key_b > triangle.key_c) {
            swap = triangle.key_b; triangle.key_b = triangle.key_c; triangle.key_c = swap;
        }
        if (triangle.key_a > triangle.key_b) {
            swap = triangle.key_a; triangle.key_a = triangle.key_b; triangle.key_b = swap;
        }
        clustered_triangles[triangle_count++] = triangle;
    }
    if (cluster_count == 0u || triangle_count == 0u) {
        result = lod_invalid("preview LOD clustering produced no drawable triangles");
        goto fail;
    }

    qsort(clustered_triangles,
          triangle_count,
          sizeof(clustered_triangles[0]),
          lod_compare_cluster_triangles);
    {
        size_t unique_count = 0u;
        for (size_t i = 0u; i < triangle_count; ++i) {
            const bool duplicate = i > 0u &&
                clustered_triangles[i].key_a == clustered_triangles[i - 1u].key_a &&
                clustered_triangles[i].key_b == clustered_triangles[i - 1u].key_b &&
                clustered_triangles[i].key_c == clustered_triangles[i - 1u].key_c;
            if (duplicate) continue;
            indices[unique_count * 3u + 0u] = clustered_triangles[i].a;
            indices[unique_count * 3u + 1u] = clustered_triangles[i].b;
            indices[unique_count * 3u + 2u] = clustered_triangles[i].c;
            unique_count++;
        }
        triangle_count = unique_count;
    }

    out_mesh->vertices = vertices;
    out_mesh->indices = indices;
    out_mesh->vertex_count = cluster_count;
    out_mesh->triangle_count = triangle_count;
    out_mesh->source_vertex_count = document->vertex_count;
    out_mesh->source_triangle_count = document->triangle_count;
    out_mesh->cluster_resolution = resolution;
    free(cell_to_cluster);
    free(vertex_map);
    free(sums);
    free(counts);
    free(clustered_triangles);
    return core_result_ok();

fail:
    free(cell_to_cluster);
    free(vertex_map);
    free(sums);
    free(counts);
    free(vertices);
    free(indices);
    free(clustered_triangles);
    return result;
}

CoreResult core_mesh_preview_build_lod_mesh(const CoreMeshAssetRuntimeDocument *document,
                                            size_t target_triangles,
                                            CoreMeshPreviewLodMesh *out_mesh) {
    int low = 2;
    int high = CORE_MESH_PREVIEW_LOD_MAX_CLUSTER_RESOLUTION;
    CoreMeshPreviewLodMesh best;
    CoreMeshPreviewLodMesh smallest;
    CoreResult validation;
    if (!document || !out_mesh || target_triangles == 0u) {
        return lod_invalid("missing runtime mesh, output, or triangle budget");
    }
    core_mesh_preview_lod_mesh_init(out_mesh);
    core_mesh_preview_lod_mesh_init(&best);
    core_mesh_preview_lod_mesh_init(&smallest);
    validation = core_mesh_asset_runtime_document_validate(document);
    if (validation.code != CORE_OK) return validation;
    if (document->vertex_count == 0u || document->triangle_count == 0u ||
        document->vertex_count > UINT32_MAX) {
        return lod_invalid("runtime mesh cannot produce an indexed preview LOD");
    }
    if (document->triangle_count <= target_triangles) {
        return lod_copy_runtime_mesh(document, out_mesh);
    }

    while (low <= high) {
        const int resolution = low + ((high - low) / 2);
        CoreMeshPreviewLodMesh candidate;
        CoreResult result;
        core_mesh_preview_lod_mesh_init(&candidate);
        result = lod_build_at_resolution(document, resolution, &candidate);
        if (result.code != CORE_OK) {
            core_mesh_preview_lod_mesh_free(&best);
            core_mesh_preview_lod_mesh_free(&smallest);
            return result;
        }
        if (candidate.triangle_count <= target_triangles) {
            if (candidate.triangle_count > best.triangle_count) {
                core_mesh_preview_lod_mesh_free(&best);
                best = candidate;
                core_mesh_preview_lod_mesh_init(&candidate);
            }
            low = resolution + 1;
        } else {
            if (smallest.triangle_count == 0u ||
                candidate.triangle_count < smallest.triangle_count) {
                core_mesh_preview_lod_mesh_free(&smallest);
                smallest = candidate;
                core_mesh_preview_lod_mesh_init(&candidate);
            }
            high = resolution - 1;
        }
        core_mesh_preview_lod_mesh_free(&candidate);
    }

    if (best.triangle_count > 0u) {
        core_mesh_preview_lod_mesh_free(&smallest);
        *out_mesh = best;
        return core_result_ok();
    }
    if (smallest.triangle_count > 0u) {
        *out_mesh = smallest;
        return core_result_ok();
    }
    return lod_invalid("unable to construct a coherent preview LOD");
}
