#include "core_mesh_compile.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { printf("fail:%d\n", __LINE__); return 1; } } while (0)

static int file_contains_all(const char *path, const char **needles, size_t needle_count) {
    FILE *fp;
    char buffer[8192];
    size_t nread;
    size_t i;
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    nread = fread(buffer, 1u, sizeof(buffer) - 1u, fp);
    fclose(fp);
    buffer[nread] = '\0';
    for (i = 0u; i < needle_count; ++i) {
        if (!strstr(buffer, needles[i])) {
            return 0;
        }
    }
    return 1;
}

static int test_geometry_ref_kind_helpers(void) {
    CoreMeshCompileGeometryRefKind kind = CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET;
    CHECK(core_mesh_compile_geometry_ref_kind_parse(NULL, &kind).code == CORE_ERR_INVALID_ARG);
    CHECK(kind == CORE_MESH_COMPILE_GEOMETRY_REF_KIND_UNKNOWN);
    CHECK(core_mesh_compile_geometry_ref_kind_parse("mesh_asset", &kind).code == CORE_OK);
    CHECK(kind == CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET);
    CHECK(strcmp(core_mesh_compile_geometry_ref_kind_name(kind), "mesh_asset") == 0);
    return 0;
}

static int test_instance_contract_validation(void) {
    CoreMeshCompileInstanceContract contract;
    core_mesh_compile_instance_contract_init(&contract);

    CHECK(core_mesh_compile_instance_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_compile_instance_contract_prepare(&contract,
                                                      "obj_bookshelf_a",
                                                      "asset_bookshelf_01").code == CORE_OK);
    CHECK(core_mesh_compile_instance_contract_validate(&contract).code == CORE_OK);

    contract.geometry_ref_kind = CORE_MESH_COMPILE_GEOMETRY_REF_KIND_UNKNOWN;
    CHECK(core_mesh_compile_instance_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.geometry_ref_kind = CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET;
    contract.object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    CHECK(core_mesh_compile_instance_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    contract.has_material_ref_id = true;
    CHECK(core_mesh_compile_instance_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);

    return 0;
}

static int test_authoring_compile_contract_validation(void) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshCompileAuthoringContract contract;

    core_mesh_asset_authoring_document_init(&document);
    core_mesh_compile_authoring_contract_init(&contract);

    CHECK(core_mesh_compile_authoring_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_compile_authoring_contract_prepare(&contract,
                                                       NULL,
                                                       "asset_runtime").code ==
          CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_document_load_file(
              "../core_mesh_asset/tests/fixtures/mesh_asset_authoring_v1_imported_stl_sample.json",
              &document).code == CORE_OK);
    CHECK(core_mesh_compile_authoring_contract_prepare(&contract,
                                                       &document,
                                                       "asset_imported_bracket_01_runtime").code ==
          CORE_OK);
    CHECK(strcmp(contract.source_asset_id, "asset_imported_bracket_01") == 0);
    CHECK(strcmp(contract.runtime_asset_id, "asset_imported_bracket_01_runtime") == 0);
    CHECK(contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH);
    CHECK(contract.emits_runtime_mesh_asset);
    CHECK(contract.preserves_surface_group_ids);
    CHECK(contract.requires_imported_mesh_source);

    contract.requires_imported_mesh_source = false;
    CHECK(core_mesh_compile_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.requires_imported_mesh_source = true;
    contract.emits_runtime_mesh_asset = false;
    CHECK(core_mesh_compile_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);

    core_mesh_asset_authoring_document_free(&document);
    return 0;
}

static int test_imported_stl_to_runtime_document(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument runtime;

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_document(
              &authoring,
              ".",
              "asset_imported_tetrahedron_01_runtime",
              &runtime).code == CORE_OK);
    CHECK(strcmp(runtime.contract.asset_id, "asset_imported_tetrahedron_01_runtime") == 0);
    CHECK(strcmp(runtime.contract.source_asset_id, "asset_imported_tetrahedron_01") == 0);
    CHECK(runtime.vertex_count == 4u);
    CHECK(runtime.triangle_count == 4u);
    CHECK(runtime.surface_group_count == 1u);
    CHECK(strcmp(runtime.surface_groups[0].group_id, "imported_surface") == 0);
    CHECK(runtime.surface_groups[0].triangle_start == 0u);
    CHECK(runtime.surface_groups[0].triangle_count == 4u);
    CHECK(runtime.contract.local_bounds.min.x == 0.0);
    CHECK(runtime.contract.local_bounds.min.y == 0.0);
    CHECK(runtime.contract.local_bounds.min.z == 0.0);
    CHECK(runtime.contract.local_bounds.max.x == 1.0);
    CHECK(runtime.contract.local_bounds.max.y == 1.0);
    CHECK(runtime.contract.local_bounds.max.z == 1.0);
    CHECK(core_mesh_asset_runtime_document_validate(&runtime).code == CORE_OK);

    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&authoring);
    return 0;
}

static int test_imported_stl_to_runtime_file(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument reloaded;
    const char *path = "/private/tmp/core_mesh_compile_imported_stl_runtime.json";

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&reloaded);

    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_file(
              &authoring,
              ".",
              "asset_imported_tetrahedron_01",
              path).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_load_file(path, &reloaded).code == CORE_OK);
    CHECK(strcmp(reloaded.contract.asset_id, "asset_imported_tetrahedron_01") == 0);
    CHECK(strcmp(reloaded.contract.source_asset_id, "asset_imported_tetrahedron_01") == 0);
    CHECK(reloaded.vertex_count == 4u);
    CHECK(reloaded.triangle_count == 4u);
    CHECK(strcmp(reloaded.surface_groups[0].group_id, "imported_surface") == 0);

    core_mesh_asset_runtime_document_free(&reloaded);
    core_mesh_asset_authoring_document_free(&authoring);
    remove(path);
    return 0;
}

static void write_u32_le(FILE *fp, uint32_t value) {
    unsigned char bytes[4];
    bytes[0] = (unsigned char)(value & 0xFFu);
    bytes[1] = (unsigned char)((value >> 8u) & 0xFFu);
    bytes[2] = (unsigned char)((value >> 16u) & 0xFFu);
    bytes[3] = (unsigned char)((value >> 24u) & 0xFFu);
    fwrite(bytes, 1u, sizeof(bytes), fp);
}

static void write_u16_le(FILE *fp, uint16_t value) {
    unsigned char bytes[2];
    bytes[0] = (unsigned char)(value & 0xFFu);
    bytes[1] = (unsigned char)((value >> 8u) & 0xFFu);
    fwrite(bytes, 1u, sizeof(bytes), fp);
}

static void write_f32_le(FILE *fp, float value) {
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(fp, bits);
}

static int write_binary_tetrahedron_stl(const char *path) {
    static const float triangles[4][3][3] = {
        {{0.0f, 0.0f, 0.0f}, {1000.0f, 0.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1000.0f}, {1000.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f}, {0.0f, 1000.0f, 0.0f}, {0.0f, 0.0f, 1000.0f}},
        {{1000.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1000.0f}, {0.0f, 1000.0f, 0.0f}}
    };
    unsigned char header[80] = {0};
    FILE *fp = fopen(path, "wb");
    size_t i;
    if (!fp) return 0;
    memcpy(header, "core_mesh_compile binary tetrahedron", 36u);
    fwrite(header, 1u, sizeof(header), fp);
    write_u32_le(fp, 4u);
    for (i = 0u; i < 4u; ++i) {
        size_t j;
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        for (j = 0u; j < 3u; ++j) {
            write_f32_le(fp, triangles[i][j][0]);
            write_f32_le(fp, triangles[i][j][1]);
            write_f32_le(fp, triangles[i][j][2]);
        }
        write_u16_le(fp, 0u);
    }
    fclose(fp);
    return 1;
}

static int write_binary_repeated_triangle_stl(const char *path, uint32_t triangle_count) {
    unsigned char header[80] = {0};
    FILE *fp = fopen(path, "wb");
    uint32_t i;
    if (!fp) return 0;
    memcpy(header, "core_mesh_compile repeated triangle", 35u);
    fwrite(header, 1u, sizeof(header), fp);
    write_u32_le(fp, triangle_count);
    for (i = 0u; i < triangle_count; ++i) {
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 1000.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 1000.0f);
        write_f32_le(fp, 0.0f);
        write_u16_le(fp, 0u);
    }
    fclose(fp);
    return 1;
}

static int write_binary_dirty_triangle_stl(const char *path, int include_valid_triangle) {
    unsigned char header[80] = {0};
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    memcpy(header, "core_mesh_compile dirty triangle", 32u);
    fwrite(header, 1u, sizeof(header), fp);
    write_u32_le(fp, include_valid_triangle ? 2u : 1u);

    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 1000.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 2000.0f);
    write_f32_le(fp, 0.0f);
    write_f32_le(fp, 0.0f);
    write_u16_le(fp, 0u);

    if (include_valid_triangle) {
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 1000.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 0.0f);
        write_f32_le(fp, 1000.0f);
        write_f32_le(fp, 0.0f);
        write_u16_le(fp, 0u);
    }

    fclose(fp);
    return 1;
}

static int test_imported_binary_stl_to_runtime_document(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument runtime;
    const char *path = "/private/tmp/core_mesh_compile_imported_binary_tetrahedron.stl";

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    CHECK(write_binary_tetrahedron_stl(path));
    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    snprintf(authoring.imported_mesh_source.source_uri,
             sizeof(authoring.imported_mesh_source.source_uri),
             "%s",
             path);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_document(
              &authoring,
              NULL,
              "asset_imported_binary_tetrahedron_runtime",
              &runtime).code == CORE_OK);
    CHECK(strcmp(runtime.contract.asset_id, "asset_imported_binary_tetrahedron_runtime") == 0);
    CHECK(runtime.vertex_count == 4u);
    CHECK(runtime.triangle_count == 4u);
    CHECK(runtime.surface_group_count == 1u);
    CHECK(strcmp(runtime.surface_groups[0].group_id, "imported_surface") == 0);
    CHECK(runtime.contract.local_bounds.min.x == 0.0);
    CHECK(runtime.contract.local_bounds.min.y == 0.0);
    CHECK(runtime.contract.local_bounds.min.z == 0.0);
    CHECK(runtime.contract.local_bounds.max.x == 1.0);
    CHECK(runtime.contract.local_bounds.max.y == 1.0);
    CHECK(runtime.contract.local_bounds.max.z == 1.0);
    CHECK(core_mesh_asset_runtime_document_validate(&runtime).code == CORE_OK);

    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&authoring);
    remove(path);
    return 0;
}

static int test_imported_binary_stl_indexed_weld_to_runtime_document(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument runtime;
    const char *path = "/private/tmp/core_mesh_compile_imported_binary_repeated.stl";

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    CHECK(write_binary_repeated_triangle_stl(path, 4096u));
    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    snprintf(authoring.imported_mesh_source.source_uri,
             sizeof(authoring.imported_mesh_source.source_uri),
             "%s",
             path);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_document(
              &authoring,
              NULL,
              "asset_imported_binary_repeated_runtime",
              &runtime).code == CORE_OK);
    CHECK(runtime.vertex_count == 3u);
    CHECK(runtime.triangle_count == 4096u);
    CHECK(runtime.triangles[4095u].a == 0u);
    CHECK(runtime.triangles[4095u].b == 1u);
    CHECK(runtime.triangles[4095u].c == 2u);
    CHECK(core_mesh_asset_runtime_document_validate(&runtime).code == CORE_OK);

    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&authoring);
    remove(path);
    return 0;
}

static int test_imported_binary_stl_too_many_triangles_guardrail(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument runtime;
    const char *path = "/private/tmp/core_mesh_compile_imported_binary_too_many.stl";

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    CHECK(write_binary_repeated_triangle_stl(path, 250001u));
    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    snprintf(authoring.imported_mesh_source.source_uri,
             sizeof(authoring.imported_mesh_source.source_uri),
             "%s",
             path);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_document(
              &authoring,
              NULL,
              "asset_imported_binary_too_many_runtime",
              &runtime).code == CORE_ERR_INVALID_ARG);
    CHECK(runtime.vertex_count == 0u);
    CHECK(runtime.triangle_count == 0u);

    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&authoring);
    remove(path);
    return 0;
}

static int test_imported_binary_stl_skips_degenerate_triangles(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument runtime;
    const char *path = "/private/tmp/core_mesh_compile_imported_binary_dirty.stl";

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    CHECK(write_binary_dirty_triangle_stl(path, 1));
    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    snprintf(authoring.imported_mesh_source.source_uri,
             sizeof(authoring.imported_mesh_source.source_uri),
             "%s",
             path);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_document(
              &authoring,
              NULL,
              "asset_imported_binary_dirty_runtime",
              &runtime).code == CORE_OK);
    CHECK(runtime.vertex_count == 3u);
    CHECK(runtime.triangle_count == 1u);
    CHECK(core_mesh_asset_runtime_document_validate(&runtime).code == CORE_OK);

    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&authoring);
    remove(path);
    return 0;
}

static int test_imported_binary_stl_rejects_all_degenerate_triangles(void) {
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetRuntimeDocument runtime;
    const char *path = "/private/tmp/core_mesh_compile_imported_binary_all_dirty.stl";

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    CHECK(write_binary_dirty_triangle_stl(path, 0));
    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
              &authoring).code == CORE_OK);
    snprintf(authoring.imported_mesh_source.source_uri,
             sizeof(authoring.imported_mesh_source.source_uri),
             "%s",
             path);
    CHECK(core_mesh_compile_imported_mesh_to_runtime_document(
              &authoring,
              NULL,
              "asset_imported_binary_all_dirty_runtime",
              &runtime).code == CORE_ERR_INVALID_ARG);
    CHECK(runtime.vertex_count == 0u);
    CHECK(runtime.triangle_count == 0u);

    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&authoring);
    remove(path);
    return 0;
}

static int test_fixture_tokens(void) {
    static const char *needles[] = {
        "\"object_type\"",
        "\"mesh_asset_instance\"",
        "\"geometry_ref\"",
        "\"kind\": \"mesh_asset\"",
        "\"id\": \"asset_bookshelf_01\""
    };
    static const char *imported_needles[] = {
        "\"source_mode\"",
        "\"imported_mesh\"",
        "\"source_uri\"",
        "\"tests/fixtures/imports/tetrahedron_ascii.stl\""
    };
    CHECK(file_contains_all("tests/fixtures/mesh_asset_instance_scene_authoring_v1_sample.json",
                            needles,
                            sizeof(needles) / sizeof(needles[0])));
    CHECK(file_contains_all("tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",
                            imported_needles,
                            sizeof(imported_needles) / sizeof(imported_needles[0])));
    return 0;
}

int main(void) {
    if (test_geometry_ref_kind_helpers() != 0) return 1;
    if (test_instance_contract_validation() != 0) return 1;
    if (test_authoring_compile_contract_validation() != 0) return 1;
    if (test_imported_stl_to_runtime_document() != 0) return 1;
    if (test_imported_stl_to_runtime_file() != 0) return 1;
    if (test_imported_binary_stl_to_runtime_document() != 0) return 1;
    if (test_imported_binary_stl_indexed_weld_to_runtime_document() != 0) return 1;
    if (test_imported_binary_stl_too_many_triangles_guardrail() != 0) return 1;
    if (test_imported_binary_stl_skips_degenerate_triangles() != 0) return 1;
    if (test_imported_binary_stl_rejects_all_degenerate_triangles() != 0) return 1;
    if (test_fixture_tokens() != 0) return 1;
    return 0;
}
