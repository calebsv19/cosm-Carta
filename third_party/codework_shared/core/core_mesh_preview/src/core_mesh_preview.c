#include "core_mesh_preview.h"

#include "core_io.h"

#include "cjson/cJSON.h"

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CORE_MESH_PREVIEW_SHARP_EDGE_DOT_THRESHOLD 0.8191520443

typedef struct CoreMeshPreviewTempEdge {
    size_t a;
    size_t b;
    CoreObjectVec3 normal;
} CoreMeshPreviewTempEdge;

typedef struct CoreMeshPreviewEdgeKey {
    size_t a;
    size_t b;
} CoreMeshPreviewEdgeKey;

static CoreResult mesh_preview_ok(void) {
    return core_result_ok();
}

static CoreResult mesh_preview_invalid(const char *message) {
    CoreResult r = {CORE_ERR_INVALID_ARG, message};
    return r;
}

static CoreResult mesh_preview_io(const char *message) {
    CoreResult r = {CORE_ERR_IO, message};
    return r;
}

static CoreResult mesh_preview_nomem(const char *message) {
    CoreResult r = {CORE_ERR_OUT_OF_MEMORY, message};
    return r;
}

static bool mesh_preview_text_equals(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

const char *core_mesh_preview_mode_name(CoreMeshPreviewMode mode) {
    switch (mode) {
        case CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1: return "feature_edges_v1";
        case CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1: return "triangle_samples_v1";
        case CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1: return "point_cloud_v1";
        case CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1: return "bounds_proxy_v1";
        case CORE_MESH_PREVIEW_MODE_UNKNOWN:
        default: return "unknown";
    }
}

CoreResult core_mesh_preview_mode_parse(const char *text, CoreMeshPreviewMode *out_mode) {
    if (!out_mode) return mesh_preview_invalid("missing preview mode output");
    *out_mode = CORE_MESH_PREVIEW_MODE_UNKNOWN;
    if (mesh_preview_text_equals(text, "feature_edges_v1")) {
        *out_mode = CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "triangle_samples_v1")) {
        *out_mode = CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "point_cloud_v1")) {
        *out_mode = CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "bounds_proxy_v1")) {
        *out_mode = CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1;
        return mesh_preview_ok();
    }
    return mesh_preview_invalid("unknown mesh preview mode");
}

const char *core_mesh_preview_sample_strategy_name(CoreMeshPreviewSampleStrategy strategy) {
    switch (strategy) {
        case CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1:
            return "feature_edge_stride_v1";
        case CORE_MESH_PREVIEW_SAMPLE_STRATEGY_LEGACY_RUNTIME_TRIANGLE_STRIDE_V1:
            return "legacy_runtime_triangle_stride_v1";
        case CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1:
            return "triangle_stride_v1";
        case CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1:
            return "point_vertex_stride_v1";
        case CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1:
            return "bounds_proxy_v1";
        case CORE_MESH_PREVIEW_SAMPLE_STRATEGY_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_preview_sample_strategy_parse(
    const char *text,
    CoreMeshPreviewSampleStrategy *out_strategy) {
    if (!out_strategy) return mesh_preview_invalid("missing preview sample strategy output");
    *out_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_UNKNOWN;
    if (mesh_preview_text_equals(text, "feature_edge_stride_v1")) {
        *out_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "legacy_runtime_triangle_stride_v1")) {
        *out_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_LEGACY_RUNTIME_TRIANGLE_STRIDE_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "triangle_stride_v1")) {
        *out_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "point_vertex_stride_v1")) {
        *out_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1;
        return mesh_preview_ok();
    }
    if (mesh_preview_text_equals(text, "bounds_proxy_v1")) {
        *out_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1;
        return mesh_preview_ok();
    }
    return mesh_preview_invalid("unknown mesh preview sample strategy");
}

void core_mesh_preview_runtime_payload_init(CoreMeshPreviewRuntimePayload *payload) {
    if (!payload) return;
    memset(payload, 0, sizeof(*payload));
    payload->mode = CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1;
    payload->sample_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1;
    payload->source_to_asset_scale = 1.0;
    snprintf(payload->runtime_asset_schema_variant,
             sizeof(payload->runtime_asset_schema_variant),
             "mesh_asset_runtime_v1");
}

void core_mesh_preview_runtime_payload_free(CoreMeshPreviewRuntimePayload *payload) {
    if (!payload) return;
    free(payload->edges);
    free(payload->triangles);
    free(payload->points);
    core_mesh_preview_runtime_payload_init(payload);
}

void core_mesh_preview_runtime_metadata_init(CoreMeshPreviewRuntimeMetadata *metadata) {
    if (!metadata) return;
    memset(metadata, 0, sizeof(*metadata));
    metadata->schema_version = CORE_MESH_PREVIEW_SCHEMA_VERSION_3;
    snprintf(metadata->schema_variant,
             sizeof(metadata->schema_variant),
             "core_mesh_preview_runtime_v1");
    snprintf(metadata->runtime_asset_schema_variant,
             sizeof(metadata->runtime_asset_schema_variant),
             "mesh_asset_runtime_v1");
    metadata->mode = CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1;
    metadata->sample_strategy = CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1;
    metadata->source_to_asset_scale = 1.0;
}

void core_mesh_preview_file_probe_init(CoreMeshPreviewFileProbe *probe) {
    if (!probe) return;
    memset(probe, 0, sizeof(*probe));
    core_mesh_preview_runtime_metadata_init(&probe->metadata);
}

CoreResult core_mesh_preview_runtime_payload_set_edge_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t edge_count) {
    CoreMeshPreviewEdge *edges = NULL;
    if (!payload) return mesh_preview_invalid("missing mesh preview payload");
    free(payload->edges);
    payload->edges = NULL;
    payload->edge_count = 0u;
    payload->preview_edge_count = 0u;
    if (edge_count == 0u) return mesh_preview_invalid("mesh preview edge count is zero");
    if (edge_count > SIZE_MAX / sizeof(CoreMeshPreviewEdge)) {
        return mesh_preview_invalid("mesh preview edge count overflows allocation");
    }
    edges = (CoreMeshPreviewEdge *)calloc(edge_count, sizeof(CoreMeshPreviewEdge));
    if (!edges) return mesh_preview_nomem("failed to allocate mesh preview edges");
    payload->edges = edges;
    payload->edge_count = edge_count;
    payload->preview_edge_count = edge_count;
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_runtime_payload_set_triangle_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t triangle_count) {
    CoreMeshPreviewTriangle *triangles = NULL;
    if (!payload) return mesh_preview_invalid("missing mesh preview payload");
    free(payload->triangles);
    payload->triangles = NULL;
    payload->preview_triangle_count = 0u;
    if (triangle_count == 0u) return mesh_preview_invalid("mesh preview triangle count is zero");
    if (triangle_count > SIZE_MAX / sizeof(CoreMeshPreviewTriangle) ||
        triangle_count > SIZE_MAX / 3u) {
        return mesh_preview_invalid("mesh preview triangle count overflows allocation");
    }
    triangles = (CoreMeshPreviewTriangle *)calloc(triangle_count, sizeof(CoreMeshPreviewTriangle));
    if (!triangles) return mesh_preview_nomem("failed to allocate mesh preview triangles");
    payload->triangles = triangles;
    payload->preview_triangle_count = triangle_count;
    payload->preview_vertex_count = triangle_count * 3u;
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_runtime_payload_set_point_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t point_count) {
    CoreMeshPreviewPoint *points = NULL;
    if (!payload) return mesh_preview_invalid("missing mesh preview payload");
    free(payload->points);
    payload->points = NULL;
    payload->preview_vertex_count = 0u;
    if (point_count == 0u) return mesh_preview_invalid("mesh preview point count is zero");
    if (point_count > SIZE_MAX / sizeof(CoreMeshPreviewPoint)) {
        return mesh_preview_invalid("mesh preview point count overflows allocation");
    }
    points = (CoreMeshPreviewPoint *)calloc(point_count, sizeof(CoreMeshPreviewPoint));
    if (!points) return mesh_preview_nomem("failed to allocate mesh preview points");
    payload->points = points;
    payload->preview_vertex_count = point_count;
    return mesh_preview_ok();
}

static bool mesh_preview_vec3_finite(CoreObjectVec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

static CoreObjectVec3 mesh_preview_bounds_center(CoreMeshAssetBounds3 bounds) {
    return (CoreObjectVec3){
        (bounds.min.x + bounds.max.x) * 0.5,
        (bounds.min.y + bounds.max.y) * 0.5,
        (bounds.min.z + bounds.max.z) * 0.5};
}

static CoreObjectVec3 mesh_preview_bounds_extent(CoreMeshAssetBounds3 bounds) {
    return (CoreObjectVec3){
        bounds.max.x - bounds.min.x,
        bounds.max.y - bounds.min.y,
        bounds.max.z - bounds.min.z};
}

static double mesh_preview_max3(double a, double b, double c) {
    double m = a > b ? a : b;
    return m > c ? m : c;
}

static void mesh_preview_populate_derived_bounds(CoreMeshPreviewRuntimePayload *payload) {
    CoreObjectVec3 extent;
    if (!payload) return;
    payload->bounds_center = mesh_preview_bounds_center(payload->local_bounds);
    payload->bounds_extent = mesh_preview_bounds_extent(payload->local_bounds);
    extent = payload->bounds_extent;
    payload->max_span = mesh_preview_max3(extent.x, extent.y, extent.z);
    payload->bounding_sphere_center = payload->bounds_center;
    payload->bounding_sphere_radius =
        sqrt((extent.x * extent.x) + (extent.y * extent.y) + (extent.z * extent.z)) * 0.5;
}

CoreResult core_mesh_preview_runtime_payload_validate(
    const CoreMeshPreviewRuntimePayload *payload) {
    if (!payload) return mesh_preview_invalid("missing mesh preview payload");
    if (!payload->asset_id[0]) return mesh_preview_invalid("mesh preview missing asset_id");
    if (payload->source_vertex_count == 0u || payload->source_triangle_count == 0u) {
        return mesh_preview_invalid("mesh preview source counts are invalid");
    }
    if (payload->mode != CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 &&
        payload->mode != CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 &&
        payload->mode != CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 &&
        payload->mode != CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1) {
        return mesh_preview_invalid("mesh preview mode is invalid");
    }
    if (payload->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1 &&
        payload->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_LEGACY_RUNTIME_TRIANGLE_STRIDE_V1 &&
        payload->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1 &&
        payload->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1 &&
        payload->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1) {
        return mesh_preview_invalid("mesh preview sample strategy is invalid");
    }
    if (payload->mode == CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 &&
        (payload->edge_count == 0u || !payload->edges)) {
        return mesh_preview_invalid("mesh preview has no drawable edges");
    }
    if (payload->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 &&
        (payload->preview_triangle_count == 0u || !payload->triangles)) {
        return mesh_preview_invalid("mesh preview has no drawable triangles");
    }
    if (payload->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 &&
        (payload->preview_vertex_count == 0u || !payload->points)) {
        return mesh_preview_invalid("mesh preview has no drawable points");
    }
    if (payload->preview_edge_count != payload->edge_count) {
        return mesh_preview_invalid("mesh preview edge counters are inconsistent");
    }
    if (payload->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 &&
        payload->preview_triangle_count > payload->max_budget) {
        return mesh_preview_invalid("mesh preview triangle budget is inconsistent");
    }
    if (payload->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 &&
        payload->preview_vertex_count > payload->max_budget) {
        return mesh_preview_invalid("mesh preview point budget is inconsistent");
    }
    if (payload->preview_vertex_count < payload->preview_edge_count ||
        payload->preview_triangle_count > payload->source_triangle_count ||
        payload->max_budget < payload->preview_edge_count) {
        return mesh_preview_invalid("mesh preview count metadata is inconsistent");
    }
    if (!mesh_preview_vec3_finite(payload->local_bounds.min) ||
        !mesh_preview_vec3_finite(payload->local_bounds.max) ||
        payload->local_bounds.min.x > payload->local_bounds.max.x ||
        payload->local_bounds.min.y > payload->local_bounds.max.y ||
        payload->local_bounds.min.z > payload->local_bounds.max.z) {
        return mesh_preview_invalid("mesh preview local bounds are invalid");
    }
    if (!mesh_preview_vec3_finite(payload->bounds_center) ||
        !mesh_preview_vec3_finite(payload->bounds_extent) ||
        !mesh_preview_vec3_finite(payload->bounding_sphere_center) ||
        !isfinite(payload->max_span) ||
        payload->max_span < 0.0 ||
        !isfinite(payload->bounding_sphere_radius) ||
        payload->bounding_sphere_radius < 0.0 ||
        !isfinite(payload->coverage_ratio) ||
        payload->coverage_ratio < 0.0 ||
        payload->coverage_ratio > 1.0) {
        return mesh_preview_invalid("mesh preview derived bounds metadata is invalid");
    }
    for (size_t i = 0u; i < payload->edge_count; ++i) {
        if (!mesh_preview_vec3_finite(payload->edges[i].a) ||
            !mesh_preview_vec3_finite(payload->edges[i].b)) {
            return mesh_preview_invalid("mesh preview edge contains non-finite coordinates");
        }
    }
    for (size_t i = 0u; i < payload->preview_triangle_count; ++i) {
        if (!mesh_preview_vec3_finite(payload->triangles[i].a) ||
            !mesh_preview_vec3_finite(payload->triangles[i].b) ||
            !mesh_preview_vec3_finite(payload->triangles[i].c)) {
            return mesh_preview_invalid("mesh preview triangle contains non-finite coordinates");
        }
    }
    if (payload->points) {
        for (size_t i = 0u; i < payload->preview_vertex_count; ++i) {
            if (!mesh_preview_vec3_finite(payload->points[i].position)) {
                return mesh_preview_invalid("mesh preview point contains non-finite coordinates");
            }
        }
    }
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_runtime_metadata_validate(
    const CoreMeshPreviewRuntimeMetadata *metadata) {
    if (!metadata) return mesh_preview_invalid("missing mesh preview metadata");
    if (!metadata->asset_id[0]) return mesh_preview_invalid("mesh preview metadata missing asset_id");
    if (metadata->source_vertex_count == 0u || metadata->source_triangle_count == 0u) {
        return mesh_preview_invalid("mesh preview metadata source counts are invalid");
    }
    if (metadata->mode != CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 &&
        metadata->mode != CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 &&
        metadata->mode != CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 &&
        metadata->mode != CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1) {
        return mesh_preview_invalid("mesh preview metadata mode is invalid");
    }
    if (metadata->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1 &&
        metadata->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_LEGACY_RUNTIME_TRIANGLE_STRIDE_V1 &&
        metadata->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1 &&
        metadata->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1 &&
        metadata->sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1) {
        return mesh_preview_invalid("mesh preview metadata sample strategy is invalid");
    }
    if (metadata->preview_edge_count != metadata->edge_count) {
        return mesh_preview_invalid("mesh preview metadata edge counters are inconsistent");
    }
    if (metadata->preview_vertex_count < metadata->preview_edge_count ||
        metadata->preview_triangle_count > metadata->source_triangle_count ||
        metadata->max_budget < metadata->preview_edge_count) {
        return mesh_preview_invalid("mesh preview metadata counts are inconsistent");
    }
    if (metadata->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 &&
        metadata->preview_triangle_count > metadata->max_budget) {
        return mesh_preview_invalid("mesh preview metadata triangle budget is inconsistent");
    }
    if (metadata->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 &&
        metadata->preview_vertex_count > metadata->max_budget) {
        return mesh_preview_invalid("mesh preview metadata point budget is inconsistent");
    }
    if (!mesh_preview_vec3_finite(metadata->local_bounds.min) ||
        !mesh_preview_vec3_finite(metadata->local_bounds.max) ||
        metadata->local_bounds.min.x > metadata->local_bounds.max.x ||
        metadata->local_bounds.min.y > metadata->local_bounds.max.y ||
        metadata->local_bounds.min.z > metadata->local_bounds.max.z) {
        return mesh_preview_invalid("mesh preview metadata local bounds are invalid");
    }
    if (!mesh_preview_vec3_finite(metadata->bounds_center) ||
        !mesh_preview_vec3_finite(metadata->bounds_extent) ||
        !mesh_preview_vec3_finite(metadata->bounding_sphere_center) ||
        !isfinite(metadata->max_span) ||
        metadata->max_span < 0.0 ||
        !isfinite(metadata->bounding_sphere_radius) ||
        metadata->bounding_sphere_radius < 0.0 ||
        !isfinite(metadata->coverage_ratio) ||
        metadata->coverage_ratio < 0.0 ||
        metadata->coverage_ratio > 1.0) {
        return mesh_preview_invalid("mesh preview metadata derived bounds are invalid");
    }
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_path_from_runtime(const char *runtime_path,
                                               char *out_path,
                                               size_t out_path_size) {
    const char *runtime_suffix = ".runtime.json";
    const char *preview_suffix = ".preview.json";
    size_t runtime_len = 0u;
    size_t suffix_len = strlen(runtime_suffix);
    if (!runtime_path || !runtime_path[0] || !out_path || out_path_size == 0u) {
        return mesh_preview_invalid("missing mesh preview path input");
    }
    runtime_len = strlen(runtime_path);
    if (runtime_len > suffix_len &&
        strcmp(runtime_path + runtime_len - suffix_len, runtime_suffix) == 0) {
        size_t base_len = runtime_len - suffix_len;
        if (base_len + strlen(preview_suffix) + 1u > out_path_size) {
            return mesh_preview_invalid("mesh preview path buffer is too small");
        }
        memcpy(out_path, runtime_path, base_len);
        memcpy(out_path + base_len, preview_suffix, strlen(preview_suffix) + 1u);
        return mesh_preview_ok();
    }
    if (snprintf(out_path, out_path_size, "%s.preview.json", runtime_path) >= (int)out_path_size) {
        return mesh_preview_invalid("mesh preview path buffer is too small");
    }
    return mesh_preview_ok();
}

static CoreObjectVec3 mesh_preview_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CoreObjectVec3 mesh_preview_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)};
}

static double mesh_preview_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

static CoreObjectVec3 mesh_preview_normalize(CoreObjectVec3 v) {
    const double len_sq = mesh_preview_dot(v, v);
    if (len_sq <= 0.0 || !isfinite(len_sq)) return (CoreObjectVec3){0.0, 0.0, 0.0};
    {
        const double inv_len = 1.0 / sqrt(len_sq);
        return (CoreObjectVec3){v.x * inv_len, v.y * inv_len, v.z * inv_len};
    }
}

static CoreObjectVec3 mesh_preview_triangle_normal(
    const CoreMeshAssetRuntimeDocument *document,
    const CoreMeshAssetRuntimeTriangle *tri) {
    const CoreObjectVec3 a = document->vertices[tri->a].position;
    const CoreObjectVec3 b = document->vertices[tri->b].position;
    const CoreObjectVec3 c = document->vertices[tri->c].position;
    return mesh_preview_normalize(mesh_preview_cross(mesh_preview_sub(b, a),
                                                     mesh_preview_sub(c, a)));
}

static int mesh_preview_compare_temp_edges(const void *lhs, const void *rhs) {
    const CoreMeshPreviewTempEdge *a = (const CoreMeshPreviewTempEdge *)lhs;
    const CoreMeshPreviewTempEdge *b = (const CoreMeshPreviewTempEdge *)rhs;
    if (a->a < b->a) return -1;
    if (a->a > b->a) return 1;
    if (a->b < b->b) return -1;
    if (a->b > b->b) return 1;
    return 0;
}

static void mesh_preview_add_temp_edge(CoreMeshPreviewTempEdge *edges,
                                       size_t *edge_count,
                                       size_t a,
                                       size_t b,
                                       CoreObjectVec3 normal) {
    CoreMeshPreviewTempEdge *edge = &edges[(*edge_count)++];
    if (a < b) {
        edge->a = a;
        edge->b = b;
    } else {
        edge->a = b;
        edge->b = a;
    }
    edge->normal = normal;
}

static CoreResult mesh_preview_build_feature_edges(
    const CoreMeshAssetRuntimeDocument *document,
    CoreMeshPreviewEdgeKey **out_edges,
    size_t *out_edge_count) {
    CoreMeshPreviewTempEdge *temp_edges = NULL;
    CoreMeshPreviewEdgeKey *candidates = NULL;
    size_t temp_edge_count = 0u;
    size_t candidate_count = 0u;
    if (out_edges) *out_edges = NULL;
    if (out_edge_count) *out_edge_count = 0u;
    if (!document || !out_edges || !out_edge_count) {
        return mesh_preview_invalid("missing mesh preview feature-edge input");
    }
    if (document->triangle_count == 0u ||
        document->triangle_count > SIZE_MAX / (3u * sizeof(CoreMeshPreviewTempEdge))) {
        return mesh_preview_invalid("invalid mesh preview triangle count");
    }
    temp_edges = (CoreMeshPreviewTempEdge *)malloc(document->triangle_count * 3u *
                                                  sizeof(CoreMeshPreviewTempEdge));
    candidates = (CoreMeshPreviewEdgeKey *)malloc(document->triangle_count * 3u *
                                                  sizeof(CoreMeshPreviewEdgeKey));
    if (!temp_edges || !candidates) {
        free(temp_edges);
        free(candidates);
        return mesh_preview_nomem("failed to allocate mesh preview edge set");
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[i];
        const CoreObjectVec3 normal = mesh_preview_triangle_normal(document, tri);
        mesh_preview_add_temp_edge(temp_edges, &temp_edge_count, tri->a, tri->b, normal);
        mesh_preview_add_temp_edge(temp_edges, &temp_edge_count, tri->b, tri->c, normal);
        mesh_preview_add_temp_edge(temp_edges, &temp_edge_count, tri->c, tri->a, normal);
    }
    qsort(temp_edges,
          temp_edge_count,
          sizeof(temp_edges[0]),
          mesh_preview_compare_temp_edges);
    for (size_t i = 0u; i < temp_edge_count;) {
        size_t j = i + 1u;
        bool feature = false;
        while (j < temp_edge_count &&
               temp_edges[j].a == temp_edges[i].a &&
               temp_edges[j].b == temp_edges[i].b) {
            ++j;
        }
        if (j - i != 2u) {
            feature = true;
        } else {
            const double dot = mesh_preview_dot(temp_edges[i].normal, temp_edges[i + 1u].normal);
            feature = !isfinite(dot) || dot <= CORE_MESH_PREVIEW_SHARP_EDGE_DOT_THRESHOLD;
        }
        if (feature) candidates[candidate_count++] = (CoreMeshPreviewEdgeKey){temp_edges[i].a, temp_edges[i].b};
        i = j;
    }
    if (candidate_count == 0u) {
        for (size_t i = 0u; i < temp_edge_count;) {
            size_t j = i + 1u;
            while (j < temp_edge_count &&
                   temp_edges[j].a == temp_edges[i].a &&
                   temp_edges[j].b == temp_edges[i].b) {
                ++j;
            }
            candidates[candidate_count++] = (CoreMeshPreviewEdgeKey){temp_edges[i].a, temp_edges[i].b};
            i = j;
        }
    }
    free(temp_edges);
    *out_edges = candidates;
    *out_edge_count = candidate_count;
    return mesh_preview_ok();
}

static size_t mesh_preview_stride_for_budget(size_t source_count, size_t max_budget) {
    if (source_count == 0u || max_budget == 0u || source_count <= max_budget) return 1u;
    {
        size_t stride = (size_t)ceil((double)source_count / (double)max_budget);
        return stride == 0u ? 1u : stride;
    }
}

static size_t mesh_preview_count_with_stride(size_t source_count, size_t stride) {
    size_t count = 0u;
    if (stride == 0u) stride = 1u;
    for (size_t i = 0u; i < source_count; i += stride) ++count;
    return count;
}

static void mesh_preview_populate_common_payload(
    CoreMeshPreviewRuntimePayload *payload,
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_budget,
    CoreMeshPreviewMode mode,
    CoreMeshPreviewSampleStrategy sample_strategy) {
    snprintf(payload->asset_id, sizeof(payload->asset_id), "%s", document->contract.asset_id);
    snprintf(payload->source_asset_id,
             sizeof(payload->source_asset_id),
             "%s",
             document->contract.source_asset_id);
    if (runtime_path) {
        snprintf(payload->runtime_path, sizeof(payload->runtime_path), "%s", runtime_path);
    }
    payload->local_bounds = document->contract.local_bounds;
    mesh_preview_populate_derived_bounds(payload);
    payload->source_vertex_count = document->vertex_count;
    payload->source_triangle_count = document->triangle_count;
    payload->max_budget = max_budget;
    payload->mode = mode;
    payload->sample_strategy = sample_strategy;
}

static CoreResult mesh_preview_build_feature_edge_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    CoreMeshPreviewRuntimePayload *out_payload) {
    CoreMeshPreviewEdgeKey *feature_edges = NULL;
    size_t feature_edge_count = 0u;
    size_t edge_count = 0u;
    size_t stride = 1u;
    CoreResult r;
    if (max_edges == 0u) max_edges = CORE_MESH_PREVIEW_DEFAULT_MAX_EDGES;
    r = mesh_preview_build_feature_edges(document, &feature_edges, &feature_edge_count);
    if (r.code != CORE_OK) return r;
    if (feature_edge_count == 0u) {
        free(feature_edges);
        return mesh_preview_invalid("mesh preview produced no feature edges");
    }
    if (feature_edge_count > max_edges) {
        stride = mesh_preview_stride_for_budget(feature_edge_count, max_edges);
    }
    edge_count = mesh_preview_count_with_stride(feature_edge_count, stride);

    core_mesh_preview_runtime_payload_free(out_payload);
    mesh_preview_populate_common_payload(out_payload,
                                         document,
                                         runtime_path,
                                         max_edges,
                                         CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1,
                                         CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1);
    out_payload->source_feature_edge_count = feature_edge_count;
    out_payload->preview_vertex_count = edge_count * 2u;
    out_payload->preview_edge_count = edge_count;
    out_payload->preview_triangle_count = 0u;
    out_payload->coverage_ratio = feature_edge_count > 0u ?
        ((double)edge_count / (double)feature_edge_count) : 0.0;
    r = core_mesh_preview_runtime_payload_set_edge_count(out_payload, edge_count);
    if (r.code != CORE_OK) {
        free(feature_edges);
        return r;
    }
    edge_count = 0u;
    for (size_t i = 0u; i < feature_edge_count; i += stride) {
        const CoreMeshPreviewEdgeKey edge = feature_edges[i];
        out_payload->edges[edge_count++] =
            (CoreMeshPreviewEdge){document->vertices[edge.a].position,
                                  document->vertices[edge.b].position};
    }
    free(feature_edges);
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

static CoreResult mesh_preview_build_triangle_sample_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_triangles,
    CoreMeshPreviewRuntimePayload *out_payload) {
    size_t stride = 1u;
    size_t triangle_count = 0u;
    CoreResult r;
    if (max_triangles == 0u) max_triangles = CORE_MESH_PREVIEW_DEFAULT_MAX_EDGES;
    stride = mesh_preview_stride_for_budget(document->triangle_count, max_triangles);
    triangle_count = mesh_preview_count_with_stride(document->triangle_count, stride);
    core_mesh_preview_runtime_payload_free(out_payload);
    mesh_preview_populate_common_payload(out_payload,
                                         document,
                                         runtime_path,
                                         max_triangles,
                                         CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1,
                                         CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1);
    out_payload->coverage_ratio =
        document->triangle_count > 0u ? ((double)triangle_count / (double)document->triangle_count) : 0.0;
    r = core_mesh_preview_runtime_payload_set_triangle_count(out_payload, triangle_count);
    if (r.code != CORE_OK) return r;
    triangle_count = 0u;
    for (size_t i = 0u; i < document->triangle_count; i += stride) {
        const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[i];
        out_payload->triangles[triangle_count++] =
            (CoreMeshPreviewTriangle){document->vertices[tri->a].position,
                                      document->vertices[tri->b].position,
                                      document->vertices[tri->c].position};
    }
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

static CoreResult mesh_preview_build_point_cloud_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_points,
    CoreMeshPreviewRuntimePayload *out_payload) {
    size_t stride = 1u;
    size_t point_count = 0u;
    CoreResult r;
    if (max_points == 0u) max_points = CORE_MESH_PREVIEW_DEFAULT_MAX_EDGES;
    stride = mesh_preview_stride_for_budget(document->vertex_count, max_points);
    point_count = mesh_preview_count_with_stride(document->vertex_count, stride);
    core_mesh_preview_runtime_payload_free(out_payload);
    mesh_preview_populate_common_payload(out_payload,
                                         document,
                                         runtime_path,
                                         max_points,
                                         CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1,
                                         CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1);
    out_payload->coverage_ratio =
        document->vertex_count > 0u ? ((double)point_count / (double)document->vertex_count) : 0.0;
    r = core_mesh_preview_runtime_payload_set_point_count(out_payload, point_count);
    if (r.code != CORE_OK) return r;
    point_count = 0u;
    for (size_t i = 0u; i < document->vertex_count; i += stride) {
        out_payload->points[point_count++].position = document->vertices[i].position;
    }
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

static CoreResult mesh_preview_build_bounds_proxy_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    CoreMeshPreviewRuntimePayload *out_payload) {
    core_mesh_preview_runtime_payload_free(out_payload);
    mesh_preview_populate_common_payload(out_payload,
                                         document,
                                         runtime_path,
                                         0u,
                                         CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1,
                                         CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1);
    out_payload->coverage_ratio = 1.0;
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

CoreResult core_mesh_preview_build_runtime_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    CoreMeshPreviewRuntimePayload *out_payload) {
    return core_mesh_preview_build_runtime_payload_with_mode(document,
                                                            runtime_path,
                                                            CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1,
                                                            max_edges,
                                                            out_payload);
}

CoreResult core_mesh_preview_build_runtime_payload_with_mode(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    CoreMeshPreviewMode mode,
    size_t max_budget,
    CoreMeshPreviewRuntimePayload *out_payload) {
    CoreResult r;
    if (!document || !out_payload) return mesh_preview_invalid("missing mesh preview build input");
    r = core_mesh_asset_runtime_document_validate(document);
    if (r.code != CORE_OK) return r;
    switch (mode) {
        case CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1:
            return mesh_preview_build_feature_edge_payload(document,
                                                           runtime_path,
                                                           max_budget,
                                                           out_payload);
        case CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1:
            return mesh_preview_build_triangle_sample_payload(document,
                                                              runtime_path,
                                                              max_budget,
                                                              out_payload);
        case CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1:
            return mesh_preview_build_point_cloud_payload(document,
                                                          runtime_path,
                                                          max_budget,
                                                          out_payload);
        case CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1:
            return mesh_preview_build_bounds_proxy_payload(document, runtime_path, out_payload);
        case CORE_MESH_PREVIEW_MODE_UNKNOWN:
        default:
            return mesh_preview_invalid("unknown mesh preview build mode");
    }
}

CoreResult core_mesh_preview_build_from_runtime_file(
    const char *runtime_path,
    CoreMeshPreviewMode mode,
    size_t max_budget,
    CoreMeshPreviewRuntimePayload *out_payload) {
    CoreMeshAssetRuntimeDocument document;
    CoreResult r;
    if (!runtime_path || !runtime_path[0] || !out_payload) {
        return mesh_preview_invalid("missing mesh preview runtime-file build input");
    }
    core_mesh_asset_runtime_document_init(&document);
    r = core_mesh_asset_runtime_document_load_file(runtime_path, &document);
    if (r.code == CORE_OK) {
        r = core_mesh_preview_build_runtime_payload_with_mode(&document,
                                                              runtime_path,
                                                              mode,
                                                              max_budget,
                                                              out_payload);
    }
    core_mesh_asset_runtime_document_free(&document);
    return r;
}

static bool mesh_preview_write_json_string(FILE *f, const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    if (fputc('"', f) == EOF) return false;
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
            case '\\': if (fputs("\\\\", f) == EOF) return false; break;
            case '"': if (fputs("\\\"", f) == EOF) return false; break;
            case '\b': if (fputs("\\b", f) == EOF) return false; break;
            case '\f': if (fputs("\\f", f) == EOF) return false; break;
            case '\n': if (fputs("\\n", f) == EOF) return false; break;
            case '\r': if (fputs("\\r", f) == EOF) return false; break;
            case '\t': if (fputs("\\t", f) == EOF) return false; break;
            default:
                if (ch < 0x20u) {
                    if (fprintf(f, "\\u%04x", (unsigned int)ch) < 0) return false;
                } else if (fputc((int)ch, f) == EOF) {
                    return false;
                }
                break;
        }
    }
    return fputc('"', f) != EOF;
}

static bool mesh_preview_write_vec3(FILE *f, CoreObjectVec3 v) {
    return fprintf(f, "{\"x\":%.17g,\"y\":%.17g,\"z\":%.17g}", v.x, v.y, v.z) >= 0;
}

CoreResult core_mesh_preview_save_file(const CoreMeshPreviewRuntimePayload *payload,
                                       const char *preview_path) {
    FILE *f = NULL;
    CoreResult r = core_mesh_preview_runtime_payload_validate(payload);
    if (r.code != CORE_OK) return r;
    if (!preview_path || !preview_path[0]) return mesh_preview_invalid("missing mesh preview output path");
    f = fopen(preview_path, "wb");
    if (!f) return mesh_preview_io("failed to open mesh preview output file");
    bool ok = fprintf(f, "{\n\t\"schema_family\":\"core_mesh_preview\",\n") >= 0;
    ok = ok && fprintf(f, "\t\"schema_variant\":\"core_mesh_preview_runtime_v1\",\n") >= 0;
    ok = ok && fprintf(f, "\t\"schema_version\":3,\n\t\"asset_id\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->asset_id) &&
         fprintf(f, ",\n\t\"source_asset_id\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->source_asset_id) &&
         fprintf(f, ",\n\t\"runtime_path\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->runtime_path) &&
         fprintf(f, ",\n\t\"runtime_asset_schema_variant\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->runtime_asset_schema_variant) &&
         fprintf(f, ",\n\t\"source_format\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->source_format) &&
         fprintf(f, ",\n\t\"source_unit_kind\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->source_unit_kind) &&
         fprintf(f,
                 ",\n\t\"has_source_to_asset_scale\":%s,\n\t\"source_to_asset_scale\":%.17g,\n\t\"source_vertex_count\":%zu,\n\t\"source_triangle_count\":%zu,\n",
                 payload->has_source_to_asset_scale ? "true" : "false",
                 payload->source_to_asset_scale,
                 payload->source_vertex_count,
                 payload->source_triangle_count) >= 0;
    ok = ok && fprintf(f, "\t\"local_bounds\":{\"min\":") >= 0 &&
         mesh_preview_write_vec3(f, payload->local_bounds.min) &&
         fprintf(f, ",\"max\":") >= 0 &&
         mesh_preview_write_vec3(f, payload->local_bounds.max) &&
         fprintf(f, "},\n\t\"bounds_center\":") >= 0 &&
         mesh_preview_write_vec3(f, payload->bounds_center) &&
         fprintf(f, ",\n\t\"bounds_extent\":") >= 0 &&
         mesh_preview_write_vec3(f, payload->bounds_extent) &&
         fprintf(f, ",\n\t\"max_span\":%.17g,\n\t\"bounding_sphere_center\":",
                 payload->max_span) >= 0 &&
         mesh_preview_write_vec3(f, payload->bounding_sphere_center) &&
         fprintf(f,
                 ",\n\t\"bounding_sphere_radius\":%.17g,\n\t\"preview_mode\":\"%s\",\n\t\"sample_strategy\":\"%s\",\n\t\"max_budget\":%zu,\n\t\"coverage_ratio\":%.17g,\n\t\"source_feature_edge_count\":%zu,\n\t\"preview_vertex_count\":%zu,\n\t\"preview_edge_count\":%zu,\n\t\"preview_triangle_count\":%zu,\n\t\"edge_count\":%zu,\n\t\"sampled_triangle_count\":%zu,\n\t\"edges\":[\n",
                 payload->bounding_sphere_radius,
                 core_mesh_preview_mode_name(payload->mode),
                 core_mesh_preview_sample_strategy_name(payload->sample_strategy),
                 payload->max_budget,
                 payload->coverage_ratio,
                 payload->source_feature_edge_count,
                 payload->preview_vertex_count,
                 payload->preview_edge_count,
                 payload->preview_triangle_count,
                 payload->edge_count,
                 payload->preview_edge_count) >= 0;
    for (size_t i = 0u; ok && i < payload->edge_count; ++i) {
        ok = fprintf(f, "\t\t{\"a\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->edges[i].a) &&
             fprintf(f, ",\"b\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->edges[i].b) &&
             fprintf(f, "}%s\n", (i + 1u < payload->edge_count) ? "," : "") >= 0;
    }
    ok = ok && fprintf(f, "\t],\n\t\"triangles\":[\n") >= 0;
    for (size_t i = 0u; ok && i < payload->preview_triangle_count; ++i) {
        ok = fprintf(f, "\t\t{\"a\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->triangles[i].a) &&
             fprintf(f, ",\"b\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->triangles[i].b) &&
             fprintf(f, ",\"c\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->triangles[i].c) &&
             fprintf(f, "}%s\n",
                     (i + 1u < payload->preview_triangle_count) ? "," : "") >= 0;
    }
    ok = ok && fprintf(f, "\t],\n\t\"points\":[\n") >= 0;
    const size_t point_count = payload->points ? payload->preview_vertex_count : 0u;
    for (size_t i = 0u; ok && i < point_count; ++i) {
        ok = fprintf(f, "\t\t{\"position\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->points[i].position) &&
             fprintf(f, "}%s\n", (i + 1u < point_count) ? "," : "") >= 0;
    }
    ok = ok && fprintf(f, "\t]\n}\n") >= 0;
    if (!ok || fclose(f) != 0) return mesh_preview_io("failed to write mesh preview output file");
    return mesh_preview_ok();
}

static bool mesh_preview_vec3_from_json(const cJSON *node, CoreObjectVec3 *out) {
    const cJSON *x = NULL;
    const cJSON *y = NULL;
    const cJSON *z = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    *out = (CoreObjectVec3){x->valuedouble, y->valuedouble, z->valuedouble};
    return true;
}

static bool mesh_preview_optional_vec3_from_json(const cJSON *node, CoreObjectVec3 *out) {
    if (!cJSON_IsObject(node) || !out) return false;
    return mesh_preview_vec3_from_json(node, out);
}

static size_t mesh_preview_json_size_or_zero(const cJSON *node) {
    return cJSON_IsNumber(node) && node->valuedouble >= 0.0 ? (size_t)node->valuedouble : 0u;
}

static double mesh_preview_json_double_or_default(const cJSON *node, double fallback) {
    return cJSON_IsNumber(node) && isfinite(node->valuedouble) ? node->valuedouble : fallback;
}

static CoreResult mesh_preview_read_text_file(const char *path, char **out_text) {
    CoreBuffer buffer = {0};
    CoreResult r;
    char *text = NULL;
    if (!path || !path[0] || !out_text) return mesh_preview_invalid("missing mesh preview input path");
    *out_text = NULL;
    r = core_io_read_all(path, &buffer);
    if (r.code != CORE_OK || !buffer.data || buffer.size == 0u) {
        return mesh_preview_io("failed to read mesh preview input file");
    }
    text = (char *)malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        return mesh_preview_nomem("failed to allocate mesh preview input buffer");
    }
    memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);
    *out_text = text;
    return mesh_preview_ok();
}

static bool mesh_preview_file_size(const char *path, uint64_t *out_size) {
    struct stat st;
    if (out_size) *out_size = 0u;
    if (!path || !path[0] || stat(path, &st) != 0 || st.st_size < 0) return false;
    if (out_size) *out_size = (uint64_t)st.st_size;
    return true;
}

static void mesh_preview_metadata_populate_derived_bounds(
    CoreMeshPreviewRuntimeMetadata *metadata) {
    CoreObjectVec3 extent;
    if (!metadata) return;
    metadata->bounds_center = mesh_preview_bounds_center(metadata->local_bounds);
    metadata->bounds_extent = mesh_preview_bounds_extent(metadata->local_bounds);
    extent = metadata->bounds_extent;
    metadata->max_span = mesh_preview_max3(extent.x, extent.y, extent.z);
    metadata->bounding_sphere_center = metadata->bounds_center;
    metadata->bounding_sphere_radius =
        sqrt((extent.x * extent.x) + (extent.y * extent.y) + (extent.z * extent.z)) * 0.5;
}

static bool mesh_preview_schema_supported(const cJSON *schema) {
    return cJSON_IsString(schema) &&
           (mesh_preview_text_equals(schema->valuestring, "core_mesh_preview_runtime_v1") ||
            mesh_preview_text_equals(schema->valuestring, "line_drawing_mesh_runtime_preview_v1"));
}

static CoreResult mesh_preview_metadata_from_json_root(
    const cJSON *root,
    CoreMeshPreviewRuntimeMetadata *out_metadata) {
    const cJSON *schema = NULL;
    const cJSON *schema_version = NULL;
    const cJSON *asset_id = NULL;
    const cJSON *source_asset_id = NULL;
    const cJSON *runtime_path = NULL;
    const cJSON *runtime_asset_schema_variant = NULL;
    const cJSON *source_format = NULL;
    const cJSON *source_unit_kind = NULL;
    const cJSON *has_source_to_asset_scale = NULL;
    const cJSON *source_to_asset_scale = NULL;
    const cJSON *source_vertex_count = NULL;
    const cJSON *source_triangle_count = NULL;
    const cJSON *source_feature_edge_count = NULL;
    const cJSON *preview_vertex_count = NULL;
    const cJSON *preview_edge_count = NULL;
    const cJSON *preview_triangle_count = NULL;
    const cJSON *max_budget = NULL;
    const cJSON *coverage_ratio = NULL;
    const cJSON *bounds = NULL;
    const cJSON *mode = NULL;
    const cJSON *sample_strategy = NULL;
    const cJSON *edges = NULL;
    const cJSON *triangles = NULL;
    const cJSON *points = NULL;
    CoreResult r;

    if (!cJSON_IsObject(root) || !out_metadata) {
        return mesh_preview_invalid("missing mesh preview metadata JSON");
    }
    core_mesh_preview_runtime_metadata_init(out_metadata);
    schema = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    if (!mesh_preview_schema_supported(schema)) {
        return mesh_preview_invalid("mesh preview file has unsupported schema");
    }
    snprintf(out_metadata->schema_variant,
             sizeof(out_metadata->schema_variant),
             "%s",
             schema->valuestring);
    schema_version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (cJSON_IsNumber(schema_version)) {
        out_metadata->schema_version = (int)schema_version->valuedouble;
    }

    asset_id = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    source_asset_id = cJSON_GetObjectItemCaseSensitive(root, "source_asset_id");
    runtime_path = cJSON_GetObjectItemCaseSensitive(root, "runtime_path");
    runtime_asset_schema_variant =
        cJSON_GetObjectItemCaseSensitive(root, "runtime_asset_schema_variant");
    source_format = cJSON_GetObjectItemCaseSensitive(root, "source_format");
    source_unit_kind = cJSON_GetObjectItemCaseSensitive(root, "source_unit_kind");
    has_source_to_asset_scale =
        cJSON_GetObjectItemCaseSensitive(root, "has_source_to_asset_scale");
    source_to_asset_scale =
        cJSON_GetObjectItemCaseSensitive(root, "source_to_asset_scale");
    source_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "source_vertex_count");
    source_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "source_triangle_count");
    if (!cJSON_IsNumber(source_vertex_count)) {
        source_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "vertex_count");
    }
    if (!cJSON_IsNumber(source_triangle_count)) {
        source_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "triangle_count");
    }
    if (!cJSON_IsString(asset_id) || !asset_id->valuestring || !asset_id->valuestring[0] ||
        !cJSON_IsNumber(source_vertex_count) || !cJSON_IsNumber(source_triangle_count)) {
        return mesh_preview_invalid("mesh preview file has invalid metadata");
    }
    snprintf(out_metadata->asset_id, sizeof(out_metadata->asset_id), "%s", asset_id->valuestring);
    if (cJSON_IsString(source_asset_id) && source_asset_id->valuestring) {
        snprintf(out_metadata->source_asset_id,
                 sizeof(out_metadata->source_asset_id),
                 "%s",
                 source_asset_id->valuestring);
    }
    if (cJSON_IsString(runtime_path) && runtime_path->valuestring) {
        snprintf(out_metadata->runtime_path,
                 sizeof(out_metadata->runtime_path),
                 "%s",
                 runtime_path->valuestring);
    }
    if (cJSON_IsString(runtime_asset_schema_variant) &&
        runtime_asset_schema_variant->valuestring) {
        snprintf(out_metadata->runtime_asset_schema_variant,
                 sizeof(out_metadata->runtime_asset_schema_variant),
                 "%s",
                 runtime_asset_schema_variant->valuestring);
    }
    if (cJSON_IsString(source_format) && source_format->valuestring) {
        snprintf(out_metadata->source_format,
                 sizeof(out_metadata->source_format),
                 "%s",
                 source_format->valuestring);
    }
    if (cJSON_IsString(source_unit_kind) && source_unit_kind->valuestring) {
        snprintf(out_metadata->source_unit_kind,
                 sizeof(out_metadata->source_unit_kind),
                 "%s",
                 source_unit_kind->valuestring);
    }
    out_metadata->has_source_to_asset_scale = cJSON_IsTrue(has_source_to_asset_scale);
    out_metadata->source_to_asset_scale =
        mesh_preview_json_double_or_default(source_to_asset_scale,
                                            out_metadata->source_to_asset_scale);
    out_metadata->source_vertex_count = (size_t)source_vertex_count->valuedouble;
    out_metadata->source_triangle_count = (size_t)source_triangle_count->valuedouble;
    source_feature_edge_count =
        cJSON_GetObjectItemCaseSensitive(root, "source_feature_edge_count");
    preview_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "preview_vertex_count");
    preview_edge_count = cJSON_GetObjectItemCaseSensitive(root, "preview_edge_count");
    preview_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "preview_triangle_count");
    max_budget = cJSON_GetObjectItemCaseSensitive(root, "max_budget");
    coverage_ratio = cJSON_GetObjectItemCaseSensitive(root, "coverage_ratio");
    out_metadata->source_feature_edge_count =
        cJSON_IsNumber(source_feature_edge_count) ?
            (size_t)source_feature_edge_count->valuedouble : 0u;
    out_metadata->preview_vertex_count = mesh_preview_json_size_or_zero(preview_vertex_count);
    out_metadata->preview_edge_count = mesh_preview_json_size_or_zero(preview_edge_count);
    out_metadata->preview_triangle_count = mesh_preview_json_size_or_zero(preview_triangle_count);
    out_metadata->max_budget = mesh_preview_json_size_or_zero(max_budget);
    out_metadata->coverage_ratio = mesh_preview_json_double_or_default(coverage_ratio, 0.0);

    bounds = cJSON_GetObjectItemCaseSensitive(root, "local_bounds");
    if (!cJSON_IsObject(bounds) ||
        !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(bounds, "min"),
                                     &out_metadata->local_bounds.min) ||
        !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(bounds, "max"),
                                     &out_metadata->local_bounds.max)) {
        return mesh_preview_invalid("mesh preview file has invalid bounds");
    }
    mesh_preview_metadata_populate_derived_bounds(out_metadata);
    (void)mesh_preview_optional_vec3_from_json(cJSON_GetObjectItemCaseSensitive(root, "bounds_center"),
                                               &out_metadata->bounds_center);
    (void)mesh_preview_optional_vec3_from_json(cJSON_GetObjectItemCaseSensitive(root, "bounds_extent"),
                                               &out_metadata->bounds_extent);
    out_metadata->max_span =
        mesh_preview_json_double_or_default(cJSON_GetObjectItemCaseSensitive(root, "max_span"),
                                            out_metadata->max_span);
    (void)mesh_preview_optional_vec3_from_json(
        cJSON_GetObjectItemCaseSensitive(root, "bounding_sphere_center"),
        &out_metadata->bounding_sphere_center);
    out_metadata->bounding_sphere_radius = mesh_preview_json_double_or_default(
        cJSON_GetObjectItemCaseSensitive(root, "bounding_sphere_radius"),
        out_metadata->bounding_sphere_radius);

    mode = cJSON_GetObjectItemCaseSensitive(root, "preview_mode");
    if (cJSON_IsString(mode)) {
        r = core_mesh_preview_mode_parse(mode->valuestring, &out_metadata->mode);
        if (r.code != CORE_OK) return r;
    }
    sample_strategy = cJSON_GetObjectItemCaseSensitive(root, "sample_strategy");
    if (cJSON_IsString(sample_strategy)) {
        r = core_mesh_preview_sample_strategy_parse(sample_strategy->valuestring,
                                                    &out_metadata->sample_strategy);
        if (r.code != CORE_OK) return r;
    }

    edges = cJSON_GetObjectItemCaseSensitive(root, "edges");
    if (cJSON_IsArray(edges) && cJSON_GetArraySize(edges) > 0) {
        out_metadata->edge_count = (size_t)cJSON_GetArraySize(edges);
    }
    if (out_metadata->source_feature_edge_count == 0u) {
        out_metadata->source_feature_edge_count = out_metadata->edge_count;
    }
    if (out_metadata->preview_edge_count == 0u) {
        out_metadata->preview_edge_count = out_metadata->edge_count;
    }
    if (out_metadata->preview_vertex_count == 0u && out_metadata->edge_count > 0u) {
        out_metadata->preview_vertex_count = out_metadata->edge_count * 2u;
    }

    triangles = cJSON_GetObjectItemCaseSensitive(root, "triangles");
    if (cJSON_IsArray(triangles) && cJSON_GetArraySize(triangles) > 0) {
        out_metadata->preview_triangle_count = (size_t)cJSON_GetArraySize(triangles);
        if (out_metadata->preview_vertex_count == 0u) {
            out_metadata->preview_vertex_count = out_metadata->preview_triangle_count * 3u;
        }
    }
    points = cJSON_GetObjectItemCaseSensitive(root, "points");
    if (cJSON_IsArray(points) && cJSON_GetArraySize(points) > 0) {
        out_metadata->preview_vertex_count = (size_t)cJSON_GetArraySize(points);
    }
    if (out_metadata->max_budget == 0u) {
        if (out_metadata->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1) {
            out_metadata->max_budget = out_metadata->preview_triangle_count;
        } else if (out_metadata->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1) {
            out_metadata->max_budget = out_metadata->preview_vertex_count;
        } else {
            out_metadata->max_budget = out_metadata->preview_edge_count;
        }
    }
    if (!isfinite(out_metadata->coverage_ratio) || out_metadata->coverage_ratio <= 0.0) {
        if (out_metadata->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1) {
            out_metadata->coverage_ratio = out_metadata->source_triangle_count > 0u ?
                ((double)out_metadata->preview_triangle_count /
                 (double)out_metadata->source_triangle_count) : 0.0;
        } else if (out_metadata->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1) {
            out_metadata->coverage_ratio = out_metadata->source_vertex_count > 0u ?
                ((double)out_metadata->preview_vertex_count /
                 (double)out_metadata->source_vertex_count) : 0.0;
        } else if (out_metadata->mode == CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1) {
            out_metadata->coverage_ratio = 1.0;
        } else {
            out_metadata->coverage_ratio = out_metadata->source_feature_edge_count > 0u ?
                ((double)out_metadata->preview_edge_count /
                 (double)out_metadata->source_feature_edge_count) : 0.0;
        }
    }
    out_metadata->has_drawable_payload =
        (out_metadata->mode == CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 &&
         out_metadata->edge_count > 0u) ||
        (out_metadata->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 &&
         out_metadata->preview_triangle_count > 0u) ||
        (out_metadata->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 &&
         out_metadata->preview_vertex_count > 0u) ||
        out_metadata->mode == CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1;
    return core_mesh_preview_runtime_metadata_validate(out_metadata);
}

CoreResult core_mesh_preview_load_metadata_only(
    const char *preview_path,
    CoreMeshPreviewRuntimeMetadata *out_metadata) {
    char *text = NULL;
    cJSON *root = NULL;
    CoreResult r;
    if (!out_metadata) return mesh_preview_invalid("missing mesh preview metadata output");
    r = mesh_preview_read_text_file(preview_path, &text);
    if (r.code != CORE_OK) return r;
    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("failed to parse mesh preview file");
    }
    r = mesh_preview_metadata_from_json_root(root, out_metadata);
    cJSON_Delete(root);
    return r;
}

CoreResult core_mesh_preview_probe_file(const char *preview_path,
                                        CoreMeshPreviewFileProbe *out_probe) {
    char *text = NULL;
    cJSON *root = NULL;
    const cJSON *schema = NULL;
    CoreResult r;
    if (!preview_path || !preview_path[0] || !out_probe) {
        return mesh_preview_invalid("missing mesh preview probe input");
    }
    core_mesh_preview_file_probe_init(out_probe);
    out_probe->exists = mesh_preview_file_size(preview_path, &out_probe->file_size_bytes);
    if (!out_probe->exists) {
        return mesh_preview_io(errno == ENOENT ? "mesh preview file does not exist" :
                                                "failed to stat mesh preview file");
    }
    r = mesh_preview_read_text_file(preview_path, &text);
    if (r.code != CORE_OK) return r;
    out_probe->readable = true;
    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("failed to parse mesh preview file");
    }
    schema = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    out_probe->schema_supported = mesh_preview_schema_supported(schema);
    if (!out_probe->schema_supported) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has unsupported schema");
    }
    r = mesh_preview_metadata_from_json_root(root, &out_probe->metadata);
    out_probe->metadata_valid = r.code == CORE_OK;
    cJSON_Delete(root);
    return r;
}

CoreResult core_mesh_preview_load_file(const char *preview_path,
                                       CoreMeshPreviewRuntimePayload *out_payload) {
    char *text = NULL;
    cJSON *root = NULL;
    const cJSON *schema = NULL;
    const cJSON *edges = NULL;
    const cJSON *triangles = NULL;
    const cJSON *points = NULL;
    const cJSON *bounds = NULL;
    const cJSON *mode = NULL;
    const cJSON *sample_strategy = NULL;
    int edge_array_count = 0;
    int triangle_array_count = 0;
    int point_array_count = 0;
    size_t edge_count = 0u;
    size_t triangle_count = 0u;
    size_t point_count = 0u;
    CoreResult r;
    if (!out_payload) return mesh_preview_invalid("missing mesh preview output payload");
    r = mesh_preview_read_text_file(preview_path, &text);
    if (r.code != CORE_OK) return r;
    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("failed to parse mesh preview file");
    }
    schema = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    if (!cJSON_IsString(schema) ||
        (!mesh_preview_text_equals(schema->valuestring, "core_mesh_preview_runtime_v1") &&
         !mesh_preview_text_equals(schema->valuestring, "line_drawing_mesh_runtime_preview_v1"))) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has unsupported schema");
    }

    core_mesh_preview_runtime_payload_free(out_payload);
    const cJSON *asset_id = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    const cJSON *source_asset_id = cJSON_GetObjectItemCaseSensitive(root, "source_asset_id");
    const cJSON *runtime_path = cJSON_GetObjectItemCaseSensitive(root, "runtime_path");
    const cJSON *runtime_asset_schema_variant =
        cJSON_GetObjectItemCaseSensitive(root, "runtime_asset_schema_variant");
    const cJSON *source_format = cJSON_GetObjectItemCaseSensitive(root, "source_format");
    const cJSON *source_unit_kind = cJSON_GetObjectItemCaseSensitive(root, "source_unit_kind");
    const cJSON *has_source_to_asset_scale =
        cJSON_GetObjectItemCaseSensitive(root, "has_source_to_asset_scale");
    const cJSON *source_to_asset_scale =
        cJSON_GetObjectItemCaseSensitive(root, "source_to_asset_scale");
    const cJSON *source_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "source_vertex_count");
    const cJSON *source_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "source_triangle_count");
    if (!cJSON_IsNumber(source_vertex_count)) {
        source_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "vertex_count");
    }
    if (!cJSON_IsNumber(source_triangle_count)) {
        source_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "triangle_count");
    }
    const cJSON *source_feature_edge_count =
        cJSON_GetObjectItemCaseSensitive(root, "source_feature_edge_count");
    const cJSON *preview_vertex_count =
        cJSON_GetObjectItemCaseSensitive(root, "preview_vertex_count");
    const cJSON *preview_edge_count =
        cJSON_GetObjectItemCaseSensitive(root, "preview_edge_count");
    const cJSON *preview_triangle_count =
        cJSON_GetObjectItemCaseSensitive(root, "preview_triangle_count");
    const cJSON *max_budget = cJSON_GetObjectItemCaseSensitive(root, "max_budget");
    const cJSON *coverage_ratio = cJSON_GetObjectItemCaseSensitive(root, "coverage_ratio");
    const cJSON *sampled_triangle_count =
        cJSON_GetObjectItemCaseSensitive(root, "sampled_triangle_count");
    if (!cJSON_IsString(asset_id) || !asset_id->valuestring || !asset_id->valuestring[0] ||
        !cJSON_IsNumber(source_vertex_count) || !cJSON_IsNumber(source_triangle_count)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has invalid metadata");
    }
    snprintf(out_payload->asset_id, sizeof(out_payload->asset_id), "%s", asset_id->valuestring);
    if (cJSON_IsString(source_asset_id) && source_asset_id->valuestring) {
        snprintf(out_payload->source_asset_id,
                 sizeof(out_payload->source_asset_id),
                 "%s",
                 source_asset_id->valuestring);
    }
    if (cJSON_IsString(runtime_path) && runtime_path->valuestring) {
        snprintf(out_payload->runtime_path,
                 sizeof(out_payload->runtime_path),
                 "%s",
                 runtime_path->valuestring);
    }
    if (cJSON_IsString(runtime_asset_schema_variant) &&
        runtime_asset_schema_variant->valuestring) {
        snprintf(out_payload->runtime_asset_schema_variant,
                 sizeof(out_payload->runtime_asset_schema_variant),
                 "%s",
                 runtime_asset_schema_variant->valuestring);
    }
    if (cJSON_IsString(source_format) && source_format->valuestring) {
        snprintf(out_payload->source_format,
                 sizeof(out_payload->source_format),
                 "%s",
                 source_format->valuestring);
    }
    if (cJSON_IsString(source_unit_kind) && source_unit_kind->valuestring) {
        snprintf(out_payload->source_unit_kind,
                 sizeof(out_payload->source_unit_kind),
                 "%s",
                 source_unit_kind->valuestring);
    }
    out_payload->has_source_to_asset_scale = cJSON_IsTrue(has_source_to_asset_scale);
    out_payload->source_to_asset_scale =
        mesh_preview_json_double_or_default(source_to_asset_scale,
                                            out_payload->source_to_asset_scale);
    out_payload->source_vertex_count = (size_t)source_vertex_count->valuedouble;
    out_payload->source_triangle_count = (size_t)source_triangle_count->valuedouble;
    out_payload->source_feature_edge_count =
        cJSON_IsNumber(source_feature_edge_count) ? (size_t)source_feature_edge_count->valuedouble : 0u;
    out_payload->preview_vertex_count = mesh_preview_json_size_or_zero(preview_vertex_count);
    out_payload->preview_edge_count = mesh_preview_json_size_or_zero(preview_edge_count);
    out_payload->preview_triangle_count = mesh_preview_json_size_or_zero(preview_triangle_count);
    out_payload->max_budget = mesh_preview_json_size_or_zero(max_budget);
    out_payload->coverage_ratio = mesh_preview_json_double_or_default(coverage_ratio, 0.0);
    bounds = cJSON_GetObjectItemCaseSensitive(root, "local_bounds");
    if (!cJSON_IsObject(bounds) ||
        !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(bounds, "min"),
                                     &out_payload->local_bounds.min) ||
        !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(bounds, "max"),
                                     &out_payload->local_bounds.max)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has invalid bounds");
    }
    mesh_preview_populate_derived_bounds(out_payload);
    (void)mesh_preview_optional_vec3_from_json(cJSON_GetObjectItemCaseSensitive(root, "bounds_center"),
                                               &out_payload->bounds_center);
    (void)mesh_preview_optional_vec3_from_json(cJSON_GetObjectItemCaseSensitive(root, "bounds_extent"),
                                               &out_payload->bounds_extent);
    out_payload->max_span =
        mesh_preview_json_double_or_default(cJSON_GetObjectItemCaseSensitive(root, "max_span"),
                                            out_payload->max_span);
    (void)mesh_preview_optional_vec3_from_json(
        cJSON_GetObjectItemCaseSensitive(root, "bounding_sphere_center"),
        &out_payload->bounding_sphere_center);
    out_payload->bounding_sphere_radius = mesh_preview_json_double_or_default(
        cJSON_GetObjectItemCaseSensitive(root, "bounding_sphere_radius"),
        out_payload->bounding_sphere_radius);
    mode = cJSON_GetObjectItemCaseSensitive(root, "preview_mode");
    if (cJSON_IsString(mode)) {
        r = core_mesh_preview_mode_parse(mode->valuestring, &out_payload->mode);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            return r;
        }
    }
    sample_strategy = cJSON_GetObjectItemCaseSensitive(root, "sample_strategy");
    if (cJSON_IsString(sample_strategy)) {
        r = core_mesh_preview_sample_strategy_parse(sample_strategy->valuestring,
                                                    &out_payload->sample_strategy);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            return r;
        }
    }
    edges = cJSON_GetObjectItemCaseSensitive(root, "edges");
    if (cJSON_IsArray(edges)) {
        edge_array_count = cJSON_GetArraySize(edges);
        if (edge_array_count > 0) {
            r = core_mesh_preview_runtime_payload_set_edge_count(out_payload,
                                                                 (size_t)edge_array_count);
            if (r.code != CORE_OK) {
                cJSON_Delete(root);
                return r;
            }
            for (int i = 0; i < edge_array_count; ++i) {
                const cJSON *edge = cJSON_GetArrayItem(edges, i);
                CoreMeshPreviewEdge parsed = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
                if (!mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(edge, "a"),
                                                 &parsed.a) ||
                    !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(edge, "b"),
                                                 &parsed.b)) {
                    continue;
                }
                out_payload->edges[edge_count++] = parsed;
            }
            out_payload->edge_count = edge_count;
        }
    }

    triangles = cJSON_GetObjectItemCaseSensitive(root, "triangles");
    if (cJSON_IsArray(triangles)) {
        triangle_array_count = cJSON_GetArraySize(triangles);
        if (triangle_array_count > 0) {
            r = core_mesh_preview_runtime_payload_set_triangle_count(
                out_payload,
                (size_t)triangle_array_count);
            if (r.code != CORE_OK) {
                cJSON_Delete(root);
                return r;
            }
            for (int i = 0; i < triangle_array_count; ++i) {
                const cJSON *triangle = cJSON_GetArrayItem(triangles, i);
                CoreMeshPreviewTriangle parsed = {
                    {0.0, 0.0, 0.0},
                    {0.0, 0.0, 0.0},
                    {0.0, 0.0, 0.0}};
                if (!mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(triangle, "a"),
                                                 &parsed.a) ||
                    !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(triangle, "b"),
                                                 &parsed.b) ||
                    !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(triangle, "c"),
                                                 &parsed.c)) {
                    continue;
                }
                out_payload->triangles[triangle_count++] = parsed;
            }
            out_payload->preview_triangle_count = triangle_count;
            out_payload->preview_vertex_count = triangle_count * 3u;
        }
    }

    points = cJSON_GetObjectItemCaseSensitive(root, "points");
    if (cJSON_IsArray(points)) {
        point_array_count = cJSON_GetArraySize(points);
        if (point_array_count > 0) {
            r = core_mesh_preview_runtime_payload_set_point_count(out_payload,
                                                                  (size_t)point_array_count);
            if (r.code != CORE_OK) {
                cJSON_Delete(root);
                return r;
            }
            for (int i = 0; i < point_array_count; ++i) {
                const cJSON *point = cJSON_GetArrayItem(points, i);
                CoreMeshPreviewPoint parsed = {{0.0, 0.0, 0.0}};
                if (!mesh_preview_vec3_from_json(
                        cJSON_GetObjectItemCaseSensitive(point, "position"),
                        &parsed.position)) {
                    continue;
                }
                out_payload->points[point_count++] = parsed;
            }
            out_payload->preview_vertex_count = point_count;
        }
    }

    if (out_payload->source_feature_edge_count == 0u) out_payload->source_feature_edge_count = edge_count;
    if (out_payload->preview_edge_count == 0u) {
        out_payload->preview_edge_count = edge_count;
    }
    if (out_payload->preview_vertex_count == 0u) {
        out_payload->preview_vertex_count = edge_count * 2u;
    }
    if (out_payload->max_budget == 0u) {
        if (out_payload->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1) {
            out_payload->max_budget = out_payload->preview_triangle_count;
        } else if (out_payload->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1) {
            out_payload->max_budget = out_payload->preview_vertex_count;
        } else {
            out_payload->max_budget = out_payload->preview_edge_count;
        }
    }
    if (!isfinite(out_payload->coverage_ratio) || out_payload->coverage_ratio <= 0.0) {
        if (out_payload->mode == CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1) {
            out_payload->coverage_ratio = out_payload->source_triangle_count > 0u ?
                ((double)out_payload->preview_triangle_count /
                 (double)out_payload->source_triangle_count) : 0.0;
        } else if (out_payload->mode == CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1) {
            out_payload->coverage_ratio = out_payload->source_vertex_count > 0u ?
                ((double)out_payload->preview_vertex_count /
                 (double)out_payload->source_vertex_count) : 0.0;
        } else if (out_payload->mode == CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1) {
            out_payload->coverage_ratio = 1.0;
        } else {
            out_payload->coverage_ratio = out_payload->source_feature_edge_count > 0u ?
                ((double)out_payload->preview_edge_count /
                 (double)out_payload->source_feature_edge_count) : 0.0;
        }
    }
    if (out_payload->preview_triangle_count == 0u && cJSON_IsNumber(sampled_triangle_count)) {
        out_payload->preview_triangle_count = 0u;
    }
    cJSON_Delete(root);
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

CoreResult core_mesh_preview_save_for_runtime_document(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    char *out_preview_path,
    size_t out_preview_path_size) {
    CoreMeshPreviewRuntimePayload payload;
    char preview_path[512];
    CoreResult r;
    core_mesh_preview_runtime_payload_init(&payload);
    r = core_mesh_preview_path_from_runtime(runtime_path, preview_path, sizeof(preview_path));
    if (r.code != CORE_OK) return r;
    r = core_mesh_preview_build_runtime_payload(document, runtime_path, max_edges, &payload);
    if (r.code == CORE_OK) r = core_mesh_preview_save_file(&payload, preview_path);
    core_mesh_preview_runtime_payload_free(&payload);
    if (r.code == CORE_OK && out_preview_path && out_preview_path_size > 0u) {
        snprintf(out_preview_path, out_preview_path_size, "%s", preview_path);
    }
    return r;
}

CoreResult core_mesh_preview_save_for_runtime_file(
    const char *runtime_path,
    CoreMeshPreviewMode mode,
    size_t max_budget,
    char *out_preview_path,
    size_t out_preview_path_size) {
    CoreMeshPreviewRuntimePayload payload;
    char preview_path[512];
    CoreResult r;
    core_mesh_preview_runtime_payload_init(&payload);
    r = core_mesh_preview_path_from_runtime(runtime_path, preview_path, sizeof(preview_path));
    if (r.code != CORE_OK) return r;
    r = core_mesh_preview_build_from_runtime_file(runtime_path, mode, max_budget, &payload);
    if (r.code == CORE_OK) r = core_mesh_preview_save_file(&payload, preview_path);
    core_mesh_preview_runtime_payload_free(&payload);
    if (r.code == CORE_OK && out_preview_path && out_preview_path_size > 0u) {
        snprintf(out_preview_path, out_preview_path_size, "%s", preview_path);
    }
    return r;
}
