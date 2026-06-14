#include "core_mesh_preview.h"

#include <stdio.h>
#include <string.h>

static int expect_ok(CoreResult r, const char *label) {
    if (r.code == CORE_OK) return 0;
    fprintf(stderr, "%s failed: %s\n", label, r.message);
    return 1;
}

static int make_tetra(CoreMeshAssetRuntimeDocument *doc) {
    CoreResult r;
    core_mesh_asset_runtime_document_init(doc);
    r = core_mesh_asset_runtime_contract_set_asset_id(&doc->contract, "asset_tetra");
    if (r.code != CORE_OK) return expect_ok(r, "set asset");
    r = core_mesh_asset_runtime_contract_set_source_asset_id(&doc->contract, "source_tetra");
    if (r.code != CORE_OK) return expect_ok(r, "set source asset");
    doc->contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    doc->contract.local_bounds.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    doc->contract.local_bounds.max = (CoreObjectVec3){1.0, 1.0, 1.0};
    doc->contract.vertex_count = 4u;
    doc->contract.triangle_count = 4u;
    r = core_mesh_asset_runtime_document_set_vertex_count(doc, 4u);
    if (r.code != CORE_OK) return expect_ok(r, "set vertices");
    r = core_mesh_asset_runtime_document_set_triangle_count(doc, 4u);
    if (r.code != CORE_OK) return expect_ok(r, "set triangles");
    r = core_mesh_asset_runtime_document_set_surface_group_count(doc, 1u);
    if (r.code != CORE_OK) return expect_ok(r, "set groups");
    doc->vertices[0].position = (CoreObjectVec3){0.0, 0.0, 0.0};
    doc->vertices[1].position = (CoreObjectVec3){1.0, 0.0, 0.0};
    doc->vertices[2].position = (CoreObjectVec3){0.0, 1.0, 0.0};
    doc->vertices[3].position = (CoreObjectVec3){0.0, 0.0, 1.0};
    doc->triangles[0] = (CoreMeshAssetRuntimeTriangle){0u, 1u, 2u, "surface"};
    doc->triangles[1] = (CoreMeshAssetRuntimeTriangle){0u, 1u, 3u, "surface"};
    doc->triangles[2] = (CoreMeshAssetRuntimeTriangle){0u, 2u, 3u, "surface"};
    doc->triangles[3] = (CoreMeshAssetRuntimeTriangle){1u, 2u, 3u, "surface"};
    snprintf(doc->surface_groups[0].group_id, sizeof(doc->surface_groups[0].group_id), "surface");
    doc->surface_groups[0].triangle_start = 0u;
    doc->surface_groups[0].triangle_count = 4u;
    return expect_ok(core_mesh_asset_runtime_document_validate(doc), "validate tetra");
}

int main(void) {
    CoreMeshAssetRuntimeDocument doc;
    CoreMeshPreviewRuntimePayload payload;
    CoreMeshPreviewRuntimePayload loaded;
    char preview_path[256];
    const char *runtime_path = "/tmp/core_mesh_preview_test.runtime.json";
    int failed = 0;

    core_mesh_preview_runtime_payload_init(&payload);
    core_mesh_preview_runtime_payload_init(&loaded);
    if (make_tetra(&doc) != 0) return 1;

    failed |= expect_ok(core_mesh_preview_build_runtime_payload(&doc,
                                                               runtime_path,
                                                               3u,
                                                               &payload),
                        "build preview");
    if (payload.edge_count != 3u ||
        payload.source_triangle_count != 4u ||
        strcmp(payload.asset_id, "asset_tetra") != 0) {
        fprintf(stderr, "unexpected preview payload metadata\n");
        failed = 1;
    }

    failed |= expect_ok(core_mesh_preview_path_from_runtime(runtime_path,
                                                           preview_path,
                                                           sizeof(preview_path)),
                        "preview path");
    failed |= expect_ok(core_mesh_preview_save_file(&payload, preview_path), "save preview");
    failed |= expect_ok(core_mesh_preview_load_file(preview_path, &loaded), "load preview");
    if (loaded.edge_count != payload.edge_count ||
        loaded.source_vertex_count != payload.source_vertex_count ||
        strcmp(loaded.asset_id, payload.asset_id) != 0) {
        fprintf(stderr, "loaded preview did not match saved payload\n");
        failed = 1;
    }

    core_mesh_preview_runtime_payload_free(&loaded);
    core_mesh_preview_runtime_payload_free(&payload);
    core_mesh_asset_runtime_document_free(&doc);
    return failed;
}
