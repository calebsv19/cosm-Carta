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

static int write_legacy_preview_file(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    if (fprintf(f,
                "{\n"
                "\"schema_variant\":\"line_drawing_mesh_runtime_preview_v1\",\n"
                "\"asset_id\":\"legacy_asset\",\n"
                "\"source_asset_id\":\"legacy_source\",\n"
                "\"runtime_path\":\"/tmp/legacy.runtime.json\",\n"
                "\"vertex_count\":4,\n"
                "\"triangle_count\":4,\n"
                "\"local_bounds\":{\"min\":{\"x\":0,\"y\":0,\"z\":0},"
                "\"max\":{\"x\":1,\"y\":1,\"z\":1}},\n"
                "\"sampled_triangle_count\":1,\n"
                "\"edges\":[{\"a\":{\"x\":0,\"y\":0,\"z\":0},"
                "\"b\":{\"x\":1,\"y\":0,\"z\":0}}]\n"
                "}\n") < 0) {
        fclose(f);
        return 1;
    }
    return fclose(f) != 0;
}

static int make_grid(CoreMeshAssetRuntimeDocument *doc, size_t side) {
    const size_t vertex_count = side * side;
    const size_t triangle_count = (side - 1u) * (side - 1u) * 2u;
    CoreResult result;
    if (!doc || side < 3u) return 1;
    core_mesh_asset_runtime_document_init(doc);
    result = core_mesh_asset_runtime_contract_set_asset_id(&doc->contract, "asset_grid");
    if (result.code != CORE_OK) return expect_ok(result, "set grid asset");
    result = core_mesh_asset_runtime_contract_set_source_asset_id(&doc->contract, "source_grid");
    if (result.code != CORE_OK) return expect_ok(result, "set grid source");
    doc->contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    doc->contract.local_bounds.min = (CoreObjectVec3){0.0, 0.0, 0.0};
    doc->contract.local_bounds.max =
        (CoreObjectVec3){(double)(side - 1u), (double)(side - 1u), 0.0};
    result = core_mesh_asset_runtime_document_set_vertex_count(doc, vertex_count);
    if (result.code != CORE_OK) return expect_ok(result, "set grid vertices");
    result = core_mesh_asset_runtime_document_set_triangle_count(doc, triangle_count);
    if (result.code != CORE_OK) return expect_ok(result, "set grid triangles");
    result = core_mesh_asset_runtime_document_set_surface_group_count(doc, 1u);
    if (result.code != CORE_OK) return expect_ok(result, "set grid groups");
    snprintf(doc->surface_groups[0].group_id,
             sizeof(doc->surface_groups[0].group_id),
             "surface");
    doc->surface_groups[0].triangle_count = triangle_count;
    for (size_t y = 0u; y < side; ++y) {
        for (size_t x = 0u; x < side; ++x) {
            doc->vertices[y * side + x].position =
                (CoreObjectVec3){(double)x, (double)y, 0.0};
        }
    }
    {
        size_t triangle = 0u;
        for (size_t y = 0u; y + 1u < side; ++y) {
            for (size_t x = 0u; x + 1u < side; ++x) {
                const size_t a = y * side + x;
                const size_t b = a + 1u;
                const size_t c = a + side;
                const size_t d = c + 1u;
                doc->triangles[triangle++] =
                    (CoreMeshAssetRuntimeTriangle){a, b, d, "surface"};
                doc->triangles[triangle++] =
                    (CoreMeshAssetRuntimeTriangle){a, d, c, "surface"};
            }
        }
    }
    return expect_ok(core_mesh_asset_runtime_document_validate(doc), "validate grid");
}

static int test_coherent_lod_mesh(void) {
    CoreMeshAssetRuntimeDocument grid;
    CoreMeshAssetRuntimeDocument tetra;
    CoreMeshPreviewLodMesh lod;
    int failed = 0;
    core_mesh_preview_lod_mesh_init(&lod);
    failed |= make_grid(&grid, 33u);
    if (!failed) {
        failed |= expect_ok(core_mesh_preview_build_lod_mesh(&grid, 180u, &lod),
                            "build clustered LOD");
        if (lod.triangle_count == 0u || lod.triangle_count > 180u ||
            lod.triangle_count >= lod.source_triangle_count ||
            lod.vertex_count <= 3u || lod.cluster_resolution < 2) {
            fprintf(stderr, "clustered LOD did not honor coherent budget contract\n");
            failed = 1;
        }
        for (size_t i = 0u; i < lod.triangle_count * 3u; ++i) {
            if (lod.indices[i] >= lod.vertex_count) {
                fprintf(stderr, "clustered LOD emitted an invalid index\n");
                failed = 1;
                break;
            }
        }
    }
    core_mesh_preview_lod_mesh_free(&lod);
    core_mesh_asset_runtime_document_free(&grid);

    failed |= make_tetra(&tetra);
    if (!failed) {
        failed |= expect_ok(core_mesh_preview_build_lod_mesh(&tetra, 100u, &lod),
                            "build exact LOD");
        if (lod.cluster_resolution != 0 || lod.vertex_count != tetra.vertex_count ||
            lod.triangle_count != tetra.triangle_count) {
            fprintf(stderr, "small runtime mesh was not preserved exactly\n");
            failed = 1;
        }
    }
    core_mesh_preview_lod_mesh_free(&lod);
    core_mesh_asset_runtime_document_free(&tetra);
    return failed;
}

int main(void) {
    CoreMeshAssetRuntimeDocument doc;
    CoreMeshPreviewRuntimePayload payload;
    CoreMeshPreviewRuntimePayload loaded;
    CoreMeshPreviewRuntimeMetadata metadata;
    CoreMeshPreviewFileProbe probe;
    char preview_path[256];
    char generated_preview_path[256];
    char legacy_preview_path[256];
    const char *runtime_path = "/tmp/core_mesh_preview_test.runtime.json";
    int failed = 0;

    failed |= test_coherent_lod_mesh();

    core_mesh_preview_runtime_payload_init(&payload);
    core_mesh_preview_runtime_payload_init(&loaded);
    core_mesh_preview_runtime_metadata_init(&metadata);
    core_mesh_preview_file_probe_init(&probe);
    if (make_tetra(&doc) != 0) return 1;
    failed |= expect_ok(core_mesh_asset_runtime_document_save_file(&doc, runtime_path),
                        "save runtime mesh");

    failed |= expect_ok(core_mesh_preview_build_runtime_payload(&doc,
                                                               runtime_path,
                                                               3u,
                                                               &payload),
                        "build preview");
    if (payload.edge_count != 3u ||
        payload.preview_edge_count != 3u ||
        payload.preview_vertex_count != 6u ||
        payload.preview_triangle_count != 0u ||
        payload.max_budget != 3u ||
        payload.source_triangle_count != 4u ||
        payload.source_feature_edge_count == 0u ||
        payload.max_span != 1.0 ||
        payload.bounding_sphere_radius <= 0.0 ||
        payload.sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_FEATURE_EDGE_STRIDE_V1 ||
        strcmp(payload.asset_id, "asset_tetra") != 0) {
        fprintf(stderr, "unexpected preview payload metadata\n");
        failed = 1;
    }

    failed |= expect_ok(core_mesh_preview_path_from_runtime(runtime_path,
                                                           preview_path,
                                                           sizeof(preview_path)),
                        "preview path");
    failed |= expect_ok(core_mesh_preview_save_file(&payload, preview_path), "save preview");
    failed |= expect_ok(core_mesh_preview_load_metadata_only(preview_path, &metadata),
                        "load preview metadata");
    if (metadata.mode != CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 ||
        metadata.preview_edge_count != payload.preview_edge_count ||
        metadata.preview_vertex_count != payload.preview_vertex_count ||
        metadata.source_triangle_count != payload.source_triangle_count ||
        metadata.has_drawable_payload == false ||
        strcmp(metadata.asset_id, payload.asset_id) != 0) {
        fprintf(stderr, "metadata-only preview did not match payload\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_probe_file(preview_path, &probe), "probe preview");
    if (!probe.exists ||
        !probe.readable ||
        !probe.schema_supported ||
        !probe.metadata_valid ||
        probe.file_size_bytes == 0u ||
        probe.metadata.preview_edge_count != payload.preview_edge_count) {
        fprintf(stderr, "preview probe did not report expected metadata\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_load_file(preview_path, &loaded), "load preview");
    if (loaded.edge_count != payload.edge_count ||
        loaded.preview_edge_count != payload.preview_edge_count ||
        loaded.preview_vertex_count != payload.preview_vertex_count ||
        loaded.preview_triangle_count != payload.preview_triangle_count ||
        loaded.max_budget != payload.max_budget ||
        loaded.source_vertex_count != payload.source_vertex_count ||
        loaded.source_feature_edge_count != payload.source_feature_edge_count ||
        loaded.sample_strategy != payload.sample_strategy ||
        loaded.max_span != payload.max_span ||
        strcmp(loaded.asset_id, payload.asset_id) != 0) {
        fprintf(stderr, "loaded preview did not match saved payload\n");
        failed = 1;
    }

    core_mesh_preview_runtime_payload_free(&loaded);
    core_mesh_preview_runtime_payload_free(&payload);
    failed |= expect_ok(core_mesh_preview_build_from_runtime_file(
                            runtime_path,
                            CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1,
                            3u,
                            &payload),
                        "build preview from runtime file");
    if (payload.mode != CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 ||
        payload.preview_vertex_count != 2u ||
        !payload.points ||
        strcmp(payload.runtime_path, runtime_path) != 0) {
        fprintf(stderr, "runtime-file build did not produce point preview\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_save_for_runtime_file(
                            runtime_path,
                            CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1,
                            2u,
                            generated_preview_path,
                            sizeof(generated_preview_path)),
                        "save preview for runtime file");
    if (strcmp(generated_preview_path, preview_path) != 0) {
        fprintf(stderr, "runtime-file save returned unexpected preview path\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_load_metadata_only(generated_preview_path, &metadata),
                        "load generated preview metadata");
    if (metadata.mode != CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 ||
        metadata.preview_triangle_count != 2u ||
        metadata.preview_vertex_count != 6u ||
        !metadata.has_drawable_payload) {
        fprintf(stderr, "generated preview metadata did not match triangle mode\n");
        failed = 1;
    }

    snprintf(legacy_preview_path,
             sizeof(legacy_preview_path),
             "/tmp/core_mesh_preview_legacy_test.preview.json");
    if (write_legacy_preview_file(legacy_preview_path) != 0) {
        fprintf(stderr, "failed to write legacy preview fixture\n");
        failed = 1;
    } else {
        failed |= expect_ok(core_mesh_preview_load_metadata_only(legacy_preview_path, &metadata),
                            "load legacy preview metadata");
        if (metadata.mode != CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1 ||
            metadata.preview_edge_count != 1u ||
            metadata.preview_vertex_count != 2u ||
            strcmp(metadata.asset_id, "legacy_asset") != 0) {
            fprintf(stderr, "legacy preview metadata did not load correctly\n");
            failed = 1;
        }
    }

    core_mesh_preview_runtime_payload_free(&loaded);
    core_mesh_preview_runtime_payload_free(&payload);
    failed |= expect_ok(core_mesh_preview_build_runtime_payload_with_mode(
                            &doc,
                            runtime_path,
                            CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1,
                            2u,
                            &payload),
                        "build triangle preview");
    if (payload.mode != CORE_MESH_PREVIEW_MODE_TRIANGLE_SAMPLES_V1 ||
        payload.sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_TRIANGLE_STRIDE_V1 ||
        payload.preview_triangle_count != 2u ||
        payload.preview_vertex_count != 6u ||
        payload.preview_edge_count != 0u ||
        payload.max_budget != 2u ||
        payload.coverage_ratio <= 0.0 ||
        payload.coverage_ratio > 1.0 ||
        !payload.triangles) {
        fprintf(stderr, "unexpected triangle preview payload metadata\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_save_file(&payload, preview_path),
                        "save triangle preview");
    failed |= expect_ok(core_mesh_preview_load_file(preview_path, &loaded),
                        "load triangle preview");
    if (loaded.mode != payload.mode ||
        loaded.preview_triangle_count != payload.preview_triangle_count ||
        loaded.preview_vertex_count != payload.preview_vertex_count ||
        loaded.preview_edge_count != 0u ||
        !loaded.triangles) {
        fprintf(stderr, "loaded triangle preview did not match saved payload\n");
        failed = 1;
    }

    core_mesh_preview_runtime_payload_free(&loaded);
    core_mesh_preview_runtime_payload_free(&payload);
    failed |= expect_ok(core_mesh_preview_build_runtime_payload_with_mode(
                            &doc,
                            runtime_path,
                            CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1,
                            3u,
                            &payload),
                        "build point preview");
    if (payload.mode != CORE_MESH_PREVIEW_MODE_POINT_CLOUD_V1 ||
        payload.sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_POINT_VERTEX_STRIDE_V1 ||
        payload.preview_vertex_count != 2u ||
        payload.preview_edge_count != 0u ||
        payload.preview_triangle_count != 0u ||
        payload.max_budget != 3u ||
        payload.coverage_ratio <= 0.0 ||
        payload.coverage_ratio > 1.0 ||
        !payload.points) {
        fprintf(stderr, "unexpected point preview payload metadata\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_save_file(&payload, preview_path),
                        "save point preview");
    failed |= expect_ok(core_mesh_preview_load_file(preview_path, &loaded),
                        "load point preview");
    if (loaded.mode != payload.mode ||
        loaded.preview_vertex_count != payload.preview_vertex_count ||
        loaded.preview_edge_count != 0u ||
        loaded.preview_triangle_count != 0u ||
        !loaded.points) {
        fprintf(stderr, "loaded point preview did not match saved payload\n");
        failed = 1;
    }

    core_mesh_preview_runtime_payload_free(&loaded);
    core_mesh_preview_runtime_payload_free(&payload);
    failed |= expect_ok(core_mesh_preview_build_runtime_payload_with_mode(
                            &doc,
                            runtime_path,
                            CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1,
                            0u,
                            &payload),
                        "build bounds preview");
    if (payload.mode != CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1 ||
        payload.sample_strategy != CORE_MESH_PREVIEW_SAMPLE_STRATEGY_BOUNDS_PROXY_V1 ||
        payload.preview_vertex_count != 0u ||
        payload.preview_edge_count != 0u ||
        payload.preview_triangle_count != 0u ||
        payload.max_budget != 0u ||
        payload.coverage_ratio != 1.0 ||
        payload.max_span != 1.0) {
        fprintf(stderr, "unexpected bounds preview payload metadata\n");
        failed = 1;
    }
    failed |= expect_ok(core_mesh_preview_save_file(&payload, preview_path),
                        "save bounds preview");
    failed |= expect_ok(core_mesh_preview_load_file(preview_path, &loaded),
                        "load bounds preview");
    if (loaded.mode != payload.mode ||
        loaded.preview_vertex_count != 0u ||
        loaded.preview_edge_count != 0u ||
        loaded.preview_triangle_count != 0u ||
        loaded.coverage_ratio != 1.0) {
        fprintf(stderr, "loaded bounds preview did not match saved payload\n");
        failed = 1;
    }

    core_mesh_preview_runtime_payload_free(&loaded);
    core_mesh_preview_runtime_payload_free(&payload);
    core_mesh_asset_runtime_document_free(&doc);
    (void)remove(runtime_path);
    (void)remove(preview_path);
    (void)remove(legacy_preview_path);
    return failed;
}
