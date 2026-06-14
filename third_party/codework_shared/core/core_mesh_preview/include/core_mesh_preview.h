#ifndef CORE_MESH_PREVIEW_H
#define CORE_MESH_PREVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include "core_base.h"
#include "core_mesh_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_MESH_PREVIEW_SCHEMA_VERSION_1 1
#define CORE_MESH_PREVIEW_DEFAULT_MAX_EDGES 3072u

typedef enum CoreMeshPreviewMode {
    CORE_MESH_PREVIEW_MODE_UNKNOWN = 0,
    CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 = 1
} CoreMeshPreviewMode;

typedef struct CoreMeshPreviewEdge {
    CoreObjectVec3 a;
    CoreObjectVec3 b;
} CoreMeshPreviewEdge;

typedef struct CoreMeshPreviewRuntimePayload {
    char asset_id[64];
    char source_asset_id[64];
    char runtime_path[512];
    CoreMeshAssetBounds3 local_bounds;
    size_t source_vertex_count;
    size_t source_triangle_count;
    size_t source_feature_edge_count;
    size_t sampled_triangle_count;
    size_t edge_count;
    CoreMeshPreviewMode mode;
    CoreMeshPreviewEdge *edges;
} CoreMeshPreviewRuntimePayload;

const char *core_mesh_preview_mode_name(CoreMeshPreviewMode mode);
CoreResult core_mesh_preview_mode_parse(const char *text, CoreMeshPreviewMode *out_mode);

void core_mesh_preview_runtime_payload_init(CoreMeshPreviewRuntimePayload *payload);
void core_mesh_preview_runtime_payload_free(CoreMeshPreviewRuntimePayload *payload);
CoreResult core_mesh_preview_runtime_payload_set_edge_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t edge_count);
CoreResult core_mesh_preview_runtime_payload_validate(
    const CoreMeshPreviewRuntimePayload *payload);

CoreResult core_mesh_preview_path_from_runtime(const char *runtime_path,
                                               char *out_path,
                                               size_t out_path_size);
CoreResult core_mesh_preview_build_runtime_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    CoreMeshPreviewRuntimePayload *out_payload);
CoreResult core_mesh_preview_save_file(const CoreMeshPreviewRuntimePayload *payload,
                                       const char *preview_path);
CoreResult core_mesh_preview_load_file(const char *preview_path,
                                       CoreMeshPreviewRuntimePayload *out_payload);
CoreResult core_mesh_preview_save_for_runtime_document(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    char *out_preview_path,
    size_t out_preview_path_size);

#ifdef __cplusplus
}
#endif

#endif
