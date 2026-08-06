#ifndef CORE_MESH_PREVIEW_H
#define CORE_MESH_PREVIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_base.h"
#include "core_mesh_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_MESH_PREVIEW_SCHEMA_VERSION_1 1
#define CORE_MESH_PREVIEW_SCHEMA_VERSION_2 2
#define CORE_MESH_PREVIEW_SCHEMA_VERSION_3 3
#define CORE_MESH_PREVIEW_DEFAULT_MAX_EDGES 3072u

typedef enum CoreMeshPreviewMode {
    CORE_MESH_PREVIEW_MODE_UNKNOWN = 0,
    CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 = 1,
    CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 = 2,
    CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 = 3,
    CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1 = 4
} CoreMeshPreviewMode;

typedef enum CoreMeshPreviewSampleStrategy {
    CORE_MESH_PREVIEW_SAMPLE_STRATEGY_UNKNOWN = 0,
    CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1 = 1,
    CORE_MESH_PREVIEW_SAMPLE_STRATEGY_LEGACY_RUNTIME_TRIANGLE_STRIDE_V1 = 2,
    CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1 = 3,
    CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1 = 4,
    CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1 = 5
} CoreMeshPreviewSampleStrategy;

typedef struct CoreMeshPreviewEdge {
    CoreObjectVec3 a;
    CoreObjectVec3 b;
} CoreMeshPreviewEdge;

typedef struct CoreMeshPreviewTriangle {
    CoreObjectVec3 a;
    CoreObjectVec3 b;
    CoreObjectVec3 c;
} CoreMeshPreviewTriangle;

typedef struct CoreMeshPreviewPoint {
    CoreObjectVec3 position;
} CoreMeshPreviewPoint;

// Renderer-neutral coherent indexed surface derived from one runtime mesh.
// Hosts choose the triangle budget and retain ownership of projection,
// shading, GPU upload, interaction policy, and cache lifetime.
typedef struct CoreMeshPreviewLodMesh {
    CoreObjectVec3 *vertices;
    uint32_t *indices;
    size_t vertex_count;
    size_t triangle_count;
    size_t source_vertex_count;
    size_t source_triangle_count;
    int cluster_resolution;
} CoreMeshPreviewLodMesh;

typedef struct CoreMeshPreviewRuntimePayload {
    char asset_id[64];
    char source_asset_id[64];
    char runtime_path[512];
    char runtime_asset_schema_variant[64];
    char source_format[32];
    char source_unit_kind[32];
    bool has_source_to_asset_scale;
    double source_to_asset_scale;
    CoreMeshAssetBounds3 local_bounds;
    CoreObjectVec3 bounds_center;
    CoreObjectVec3 bounds_extent;
    double max_span;
    CoreObjectVec3 bounding_sphere_center;
    double bounding_sphere_radius;
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t source_feature_edge_count;
    size_t preview_vertex_count;
    size_t preview_edge_count;
    size_t preview_triangle_count;
    size_t max_budget;
    double coverage_ratio;
    size_t edge_count;
    CoreMeshPreviewMode mode;
    CoreMeshPreviewSampleStrategy sample_strategy;
    CoreMeshPreviewEdge *edges;
    CoreMeshPreviewTriangle *triangles;
    CoreMeshPreviewPoint *points;
} CoreMeshPreviewRuntimePayload;

typedef struct CoreMeshPreviewRuntimeMetadata {
    int schema_version;
    char schema_variant[64];
    char asset_id[64];
    char source_asset_id[64];
    char runtime_path[512];
    char runtime_asset_schema_variant[64];
    char source_format[32];
    char source_unit_kind[32];
    bool has_source_to_asset_scale;
    double source_to_asset_scale;
    CoreMeshAssetBounds3 local_bounds;
    CoreObjectVec3 bounds_center;
    CoreObjectVec3 bounds_extent;
    double max_span;
    CoreObjectVec3 bounding_sphere_center;
    double bounding_sphere_radius;
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t source_feature_edge_count;
    size_t preview_vertex_count;
    size_t preview_edge_count;
    size_t preview_triangle_count;
    size_t max_budget;
    double coverage_ratio;
    size_t edge_count;
    CoreMeshPreviewMode mode;
    CoreMeshPreviewSampleStrategy sample_strategy;
    bool has_drawable_payload;
} CoreMeshPreviewRuntimeMetadata;

typedef struct CoreMeshPreviewFileProbe {
    bool exists;
    bool readable;
    bool schema_supported;
    bool metadata_valid;
    uint64_t file_size_bytes;
    CoreMeshPreviewRuntimeMetadata metadata;
} CoreMeshPreviewFileProbe;

const char *core_mesh_preview_mode_name(CoreMeshPreviewMode mode);
CoreResult core_mesh_preview_mode_parse(const char *text, CoreMeshPreviewMode *out_mode);
const char *core_mesh_preview_sample_strategy_name(CoreMeshPreviewSampleStrategy strategy);
CoreResult core_mesh_preview_sample_strategy_parse(
    const char *text,
    CoreMeshPreviewSampleStrategy *out_strategy);

void core_mesh_preview_runtime_payload_init(CoreMeshPreviewRuntimePayload *payload);
void core_mesh_preview_runtime_payload_free(CoreMeshPreviewRuntimePayload *payload);
CoreResult core_mesh_preview_runtime_payload_set_edge_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t edge_count);
CoreResult core_mesh_preview_runtime_payload_set_triangle_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t triangle_count);
CoreResult core_mesh_preview_runtime_payload_set_point_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t point_count);
CoreResult core_mesh_preview_runtime_payload_validate(
    const CoreMeshPreviewRuntimePayload *payload);
void core_mesh_preview_runtime_metadata_init(CoreMeshPreviewRuntimeMetadata *metadata);
CoreResult core_mesh_preview_runtime_metadata_validate(
    const CoreMeshPreviewRuntimeMetadata *metadata);
void core_mesh_preview_file_probe_init(CoreMeshPreviewFileProbe *probe);

void core_mesh_preview_lod_mesh_init(CoreMeshPreviewLodMesh *mesh);
void core_mesh_preview_lod_mesh_free(CoreMeshPreviewLodMesh *mesh);
CoreResult core_mesh_preview_build_lod_mesh(
    const CoreMeshAssetRuntimeDocument *document,
    size_t target_triangles,
    CoreMeshPreviewLodMesh *out_mesh);

CoreResult core_mesh_preview_path_from_runtime(const char *runtime_path,
                                               char *out_path,
                                               size_t out_path_size);
CoreResult core_mesh_preview_build_runtime_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    CoreMeshPreviewRuntimePayload *out_payload);
CoreResult core_mesh_preview_build_runtime_payload_with_mode(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    CoreMeshPreviewMode mode,
    size_t max_budget,
    CoreMeshPreviewRuntimePayload *out_payload);
CoreResult core_mesh_preview_build_from_runtime_file(
    const char *runtime_path,
    CoreMeshPreviewMode mode,
    size_t max_budget,
    CoreMeshPreviewRuntimePayload *out_payload);
CoreResult core_mesh_preview_save_file(const CoreMeshPreviewRuntimePayload *payload,
                                       const char *preview_path);
CoreResult core_mesh_preview_load_file(const char *preview_path,
                                       CoreMeshPreviewRuntimePayload *out_payload);
CoreResult core_mesh_preview_load_metadata_only(
    const char *preview_path,
    CoreMeshPreviewRuntimeMetadata *out_metadata);
CoreResult core_mesh_preview_probe_file(const char *preview_path,
                                        CoreMeshPreviewFileProbe *out_probe);
CoreResult core_mesh_preview_save_for_runtime_document(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    char *out_preview_path,
    size_t out_preview_path_size);
CoreResult core_mesh_preview_save_for_runtime_file(
    const char *runtime_path,
    CoreMeshPreviewMode mode,
    size_t max_budget,
    char *out_preview_path,
    size_t out_preview_path_size);

#ifdef __cplusplus
}
#endif

#endif
