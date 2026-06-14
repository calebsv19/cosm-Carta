#include "core_mesh_asset.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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

static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}

static int runtime_sphere_fixture_path(const char *filename, char *out, size_t out_size) {
    const char *root = getenv("CODEWORK_ROOT");
    static const char *candidates[] = {
        "../../../ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets",
        "../../../../../ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets"
    };
    size_t i;
    if (!filename || !out || out_size == 0u) return 0;
    if (root && root[0]) {
        if (snprintf(out,
                     out_size,
                     "%s/ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/%s",
                     root,
                     filename) < (int)out_size &&
            file_exists(out)) {
            return 1;
        }
    }
    for (i = 0u; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (snprintf(out, out_size, "%s/%s", candidates[i], filename) < (int)out_size &&
            file_exists(out)) {
            return 1;
        }
    }
    return 0;
}

static int test_parse_helpers(void) {
    CoreMeshAssetSchemaVariant variant = CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1;
    CoreMeshAssetType type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    CoreMeshAssetPrimitiveSeedKind primitive_kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM;
    CoreMeshAssetSourceMode mode = CORE_MESH_ASSET_SOURCE_MODE_REVOLVE;
    CoreMeshAssetImportedMeshSourceFormat format =
        CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL;

    CHECK(core_mesh_asset_schema_variant_parse(NULL, &variant).code == CORE_ERR_INVALID_ARG);
    CHECK(variant == CORE_MESH_ASSET_SCHEMA_VARIANT_UNKNOWN);
    CHECK(core_mesh_asset_schema_variant_parse("mesh_asset_authoring_v1", &variant).code == CORE_OK);
    CHECK(variant == CORE_MESH_ASSET_SCHEMA_VARIANT_AUTHORING_V1);
    CHECK(strcmp(core_mesh_asset_schema_variant_name(CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1),
                 "mesh_asset_runtime_v1") == 0);

    CHECK(core_mesh_asset_type_parse(NULL, &type).code == CORE_ERR_INVALID_ARG);
    CHECK(type == CORE_MESH_ASSET_TYPE_UNKNOWN);
    CHECK(core_mesh_asset_type_parse("solid_mesh", &type).code == CORE_OK);
    CHECK(type == CORE_MESH_ASSET_TYPE_SOLID_MESH);

    CHECK(core_mesh_asset_primitive_seed_kind_parse(NULL, &primitive_kind).code ==
          CORE_ERR_INVALID_ARG);
    CHECK(primitive_kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_UNKNOWN);
    CHECK(core_mesh_asset_primitive_seed_kind_parse("plane", &primitive_kind).code == CORE_OK);
    CHECK(primitive_kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE);
    CHECK(strcmp(core_mesh_asset_primitive_seed_kind_name(
                     CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM),
                 "rect_prism") == 0);

    CHECK(core_mesh_asset_source_mode_parse(NULL, &mode).code == CORE_ERR_INVALID_ARG);
    CHECK(mode == CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN);
    CHECK(core_mesh_asset_source_mode_parse("profile_extrusion", &mode).code == CORE_OK);
    CHECK(mode == CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION);
    CHECK(core_mesh_asset_source_mode_parse("imported_mesh", &mode).code == CORE_OK);
    CHECK(mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH);
    CHECK(strcmp(core_mesh_asset_source_mode_name(CORE_MESH_ASSET_SOURCE_MODE_REVOLVE),
                 "revolve") == 0);

    CHECK(core_mesh_asset_imported_mesh_source_format_parse(NULL, &format).code ==
          CORE_ERR_INVALID_ARG);
    CHECK(format == CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_UNKNOWN);
    CHECK(core_mesh_asset_imported_mesh_source_format_parse("stl", &format).code == CORE_OK);
    CHECK(format == CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL);
    CHECK(strcmp(core_mesh_asset_imported_mesh_source_format_name(format), "stl") == 0);

    return 0;
}

static int test_authoring_contract_validation(void) {
    CoreMeshAssetAuthoringContract contract;
    core_mesh_asset_authoring_contract_init(&contract);

    CHECK(core_mesh_asset_authoring_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_contract_set_asset_id(&contract, "asset_bookshelf_01").code == CORE_OK);
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_OK);

    contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION;
    contract.world_scale = 0.0;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.world_scale = 1.0;
    contract.pivot.axis_u.x = 0.0;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.pivot.axis_u.x = 1.0;
    contract.pivot.normal.z = NAN;
    CHECK(core_mesh_asset_authoring_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);

    return 0;
}

static int test_authoring_document_validation(void) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshAssetPrimitiveSeed *plane = NULL;
    CoreMeshAssetPrimitiveSeed *rect_prism = NULL;

    core_mesh_asset_authoring_document_init(&document);
    CHECK(core_mesh_asset_authoring_document_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_contract_set_asset_id(&document.contract, "asset_demo").code ==
          CORE_OK);
    document.contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_authoring_document_set_primitive_seed_count(&document, 2u).code ==
          CORE_OK);

    plane = &document.primitive_seeds[0];
    rect_prism = &document.primitive_seeds[1];

    CHECK(core_object_set_identity(&plane->object, "primitive_1", "plane_primitive").code ==
          CORE_OK);
    CHECK(core_mesh_asset_authoring_contract_validate(&document.contract).code == CORE_OK);
    plane->kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE;
    strcpy(plane->primitive_id, "primitive_1");
    plane->object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    plane->object.locked_plane = CORE_OBJECT_PLANE_XY;
    plane->object.transform.scale.x = 1.0;
    plane->object.transform.scale.y = 1.0;
    plane->object.transform.scale.z = 1.0;
    plane->plane.width = 2.0;
    plane->plane.height = 1.5;
    plane->plane.frame.origin.x = 0.0;
    plane->plane.frame.origin.y = 0.0;
    plane->plane.frame.origin.z = 0.0;
    plane->plane.frame.axis_u.x = 1.0;
    plane->plane.frame.axis_v.y = 1.0;
    plane->plane.frame.normal.z = 1.0;
    plane->object.transform.position = plane->plane.frame.origin;
    plane->object.flags.visible = true;
    plane->object.flags.selectable = true;

    CHECK(core_object_set_identity(&rect_prism->object,
                                   "primitive_2",
                                   "rect_prism_primitive").code == CORE_OK);
    rect_prism->kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM;
    strcpy(rect_prism->primitive_id, "primitive_2");
    rect_prism->object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    rect_prism->object.locked_plane = CORE_OBJECT_PLANE_XY;
    rect_prism->object.transform.scale.x = 1.0;
    rect_prism->object.transform.scale.y = 1.0;
    rect_prism->object.transform.scale.z = 1.0;
    rect_prism->rect_prism.width = 1.0;
    rect_prism->rect_prism.height = 0.5;
    rect_prism->rect_prism.depth = 0.75;
    rect_prism->rect_prism.frame.origin.x = 0.5;
    rect_prism->rect_prism.frame.origin.y = 0.0;
    rect_prism->rect_prism.frame.origin.z = 0.25;
    rect_prism->rect_prism.frame.axis_u.x = 1.0;
    rect_prism->rect_prism.frame.axis_v.y = 1.0;
    rect_prism->rect_prism.frame.normal.z = 1.0;
    rect_prism->object.transform.position = rect_prism->rect_prism.frame.origin;
    rect_prism->object.flags.visible = true;
    rect_prism->object.flags.selectable = true;

    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_OK);
    rect_prism->rect_prism.depth = -1.0;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_ERR_INVALID_ARG);
    core_mesh_asset_authoring_document_free(&document);
    return 0;
}

static int test_imported_mesh_authoring_document_validation(void) {
    CoreMeshAssetAuthoringDocument document;

    core_mesh_asset_authoring_document_init(&document);
    CHECK(core_mesh_asset_authoring_contract_set_asset_id(&document.contract,
                                                          "asset_imported_bracket_01").code ==
          CORE_OK);
    document.contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_ERR_INVALID_ARG);

    document.has_imported_mesh_source = true;
    strcpy(document.imported_mesh_source.import_id, "import_bracket_stl_01");
    document.imported_mesh_source.source_format =
        CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL;
    strcpy(document.imported_mesh_source.source_uri, "imports/bracket.stl");
    document.imported_mesh_source.source_unit_kind = CORE_UNIT_MILLIMETER;
    document.imported_mesh_source.source_to_asset_scale = 0.001;
    strcpy(document.imported_mesh_source.orientation_policy, "source_axes");
    strcpy(document.imported_mesh_source.default_surface_group_id, "imported_surface");
    document.imported_mesh_source.weld_vertices = true;
    document.imported_mesh_source.weld_tolerance = 0.000001;
    document.imported_mesh_source.preserve_source_normals = false;
    document.imported_mesh_source.topology_closed_volume_observed = true;
    document.imported_mesh_source.topology_manifold_observed = true;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_OK);

    document.imported_mesh_source.source_to_asset_scale = 0.0;
    CHECK(core_mesh_asset_authoring_document_validate(&document).code == CORE_ERR_INVALID_ARG);

    core_mesh_asset_authoring_document_free(&document);
    return 0;
}

static int test_runtime_contract_validation(void) {
    CoreMeshAssetRuntimeContract contract;
    core_mesh_asset_runtime_contract_init(&contract);

    CHECK(core_mesh_asset_runtime_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_runtime_contract_set_asset_id(&contract, "asset_bookshelf_01").code == CORE_OK);
    CHECK(core_mesh_asset_runtime_contract_set_source_asset_id(&contract, "asset_bookshelf_01").code == CORE_OK);
    contract.vertex_count = 48u;
    contract.triangle_count = 24u;
    contract.local_bounds.min.x = -1.0;
    contract.local_bounds.min.y = -0.5;
    contract.local_bounds.min.z = 0.0;
    contract.local_bounds.max.x = 1.0;
    contract.local_bounds.max.y = 0.5;
    contract.local_bounds.max.z = 2.0;
    CHECK(core_mesh_asset_runtime_contract_validate(&contract).code == CORE_OK);

    contract.triangle_count = 0u;
    CHECK(core_mesh_asset_runtime_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);
    contract.triangle_count = 24u;
    contract.local_bounds.min.z = 3.0;
    CHECK(core_mesh_asset_runtime_contract_validate(&contract).code == CORE_ERR_INVALID_ARG);

    return 0;
}

static int test_runtime_document_validation(void) {
    CoreMeshAssetRuntimeDocument document;
    core_mesh_asset_runtime_document_init(&document);

    CHECK(core_mesh_asset_runtime_document_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_mesh_asset_runtime_contract_set_asset_id(&document.contract, "asset_triangle").code ==
          CORE_OK);
    CHECK(core_mesh_asset_runtime_contract_set_source_asset_id(&document.contract,
                                                               "asset_triangle_src").code ==
          CORE_OK);
    document.contract.local_bounds.min.x = 0.0;
    document.contract.local_bounds.min.y = 0.0;
    document.contract.local_bounds.min.z = 0.0;
    document.contract.local_bounds.max.x = 1.0;
    document.contract.local_bounds.max.y = 1.0;
    document.contract.local_bounds.max.z = 1.0;
    CHECK(core_mesh_asset_runtime_document_set_vertex_count(&document, 3u).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_set_triangle_count(&document, 1u).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_set_surface_group_count(&document, 1u).code == CORE_OK);
    document.vertices[0].position.x = 0.0;
    document.vertices[0].position.y = 0.0;
    document.vertices[0].position.z = 0.0;
    document.vertices[1].position.x = 1.0;
    document.vertices[1].position.y = 0.0;
    document.vertices[1].position.z = 0.0;
    document.vertices[2].position.x = 0.0;
    document.vertices[2].position.y = 1.0;
    document.vertices[2].position.z = 0.0;
    document.triangles[0].a = 0u;
    document.triangles[0].b = 1u;
    document.triangles[0].c = 2u;
    strcpy(document.triangles[0].surface_group_id, "shell");
    strcpy(document.surface_groups[0].group_id, "shell");
    document.surface_groups[0].triangle_start = 0u;
    document.surface_groups[0].triangle_count = 1u;
    CHECK(core_mesh_asset_runtime_document_validate(&document).code == CORE_OK);

    document.triangles[0].c = 3u;
    CHECK(core_mesh_asset_runtime_document_validate(&document).code == CORE_ERR_INVALID_ARG);
    document.triangles[0].c = 2u;
    strcpy(document.triangles[0].surface_group_id, "missing");
    CHECK(core_mesh_asset_runtime_document_validate(&document).code == CORE_ERR_INVALID_ARG);
    strcpy(document.triangles[0].surface_group_id, "shell");
    document.vertices[2].position.y = 0.0;
    CHECK(core_mesh_asset_runtime_document_validate(&document).code == CORE_ERR_INVALID_ARG);

    core_mesh_asset_runtime_document_free(&document);
    return 0;
}

static int test_runtime_document_file_load(void) {
    CoreMeshAssetRuntimeDocument document;
    core_mesh_asset_runtime_document_init(&document);

    CHECK(core_mesh_asset_runtime_document_load_file(
              "tests/fixtures/mesh_asset_runtime_v1_sample.json",
              &document).code == CORE_OK);
    CHECK(strcmp(document.contract.asset_id, "asset_bookshelf_01") == 0);
    CHECK(document.vertex_count == 4u);
    CHECK(document.triangle_count == 2u);
    CHECK(document.surface_group_count == 1u);
    CHECK(strcmp(document.surface_groups[0].group_id, "shelf_faces") == 0);
    core_mesh_asset_runtime_document_free(&document);
    return 0;
}

static int test_runtime_document_file_roundtrip(void) {
    CoreMeshAssetRuntimeDocument document;
    CoreMeshAssetRuntimeDocument reloaded;
    const char *path = "/private/tmp/core_mesh_asset_runtime_roundtrip.json";

    core_mesh_asset_runtime_document_init(&document);
    core_mesh_asset_runtime_document_init(&reloaded);

    CHECK(core_mesh_asset_runtime_document_load_file(
              "tests/fixtures/mesh_asset_runtime_v1_sample.json",
              &document).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_save_file(&document, path).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_load_file(path, &reloaded).code == CORE_OK);
    CHECK(strcmp(reloaded.contract.asset_id, "asset_bookshelf_01") == 0);
    CHECK(strcmp(reloaded.contract.source_asset_id, "asset_bookshelf_01") == 0);
    CHECK(reloaded.vertex_count == 4u);
    CHECK(reloaded.triangle_count == 2u);
    CHECK(reloaded.surface_group_count == 1u);
    CHECK(strcmp(reloaded.surface_groups[0].group_id, "shelf_faces") == 0);

    core_mesh_asset_runtime_document_free(&document);
    core_mesh_asset_runtime_document_free(&reloaded);
    remove(path);
    return 0;
}

static int test_runtime_document_large_file_save_streams(void) {
    const size_t triangle_count = 90000u;
    const char *path = "/private/tmp/core_mesh_asset_runtime_large_stream.json";
    CoreMeshAssetRuntimeDocument document;
    struct stat st;
    size_t i;

    core_mesh_asset_runtime_document_init(&document);
    CHECK(core_mesh_asset_runtime_contract_set_asset_id(&document.contract,
                                                        "asset_large_stream").code == CORE_OK);
    CHECK(core_mesh_asset_runtime_contract_set_source_asset_id(&document.contract,
                                                               "asset_large_stream").code == CORE_OK);
    document.contract.local_bounds.min.x = 0.0;
    document.contract.local_bounds.min.y = 0.0;
    document.contract.local_bounds.min.z = 0.0;
    document.contract.local_bounds.max.x = 1.0;
    document.contract.local_bounds.max.y = 1.0;
    document.contract.local_bounds.max.z = 1.0;
    CHECK(core_mesh_asset_runtime_document_set_vertex_count(&document, 3u).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_set_triangle_count(&document, triangle_count).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_set_surface_group_count(&document, 1u).code == CORE_OK);
    document.vertices[0].position.x = 0.0;
    document.vertices[0].position.y = 0.0;
    document.vertices[0].position.z = 0.0;
    document.vertices[1].position.x = 1.0;
    document.vertices[1].position.y = 0.0;
    document.vertices[1].position.z = 0.0;
    document.vertices[2].position.x = 0.0;
    document.vertices[2].position.y = 1.0;
    document.vertices[2].position.z = 0.0;
    for (i = 0u; i < triangle_count; ++i) {
        document.triangles[i].a = 0u;
        document.triangles[i].b = 1u;
        document.triangles[i].c = 2u;
        strcpy(document.triangles[i].surface_group_id, "shell");
    }
    strcpy(document.surface_groups[0].group_id, "shell");
    document.surface_groups[0].triangle_start = 0u;
    document.surface_groups[0].triangle_count = triangle_count;

    CHECK(core_mesh_asset_runtime_document_validate(&document).code == CORE_OK);
    CHECK(core_mesh_asset_runtime_document_save_file(&document, path).code == CORE_OK);
    CHECK(stat(path, &st) == 0);
    CHECK(st.st_size > (off_t)(4u * 1024u * 1024u));

    core_mesh_asset_runtime_document_free(&document);
    remove(path);
    return 0;
}

static int test_runtime_document_mrt0_sphere_fixtures(void) {
    static const struct {
        const char *filename;
        const char *asset_id;
        size_t vertices;
        size_t triangles;
    } cases[] = {
        {
            "asset_sphere_8x4.runtime.json",
            "asset_sphere_8x4",
            26u,
            48u
        },
        {
            "asset_sphere_16x8.runtime.json",
            "asset_sphere_16x8",
            114u,
            224u
        },
        {
            "asset_sphere_32x16.runtime.json",
            "asset_sphere_32x16",
            482u,
            960u
        }
    };
    size_t i;
    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CoreMeshAssetRuntimeDocument document;
        char path[1024];
        core_mesh_asset_runtime_document_init(&document);
        CHECK(runtime_sphere_fixture_path(cases[i].filename, path, sizeof(path)));
        CHECK(core_mesh_asset_runtime_document_load_file(path, &document).code == CORE_OK);
        CHECK(strcmp(document.contract.asset_id, cases[i].asset_id) == 0);
        CHECK(document.vertex_count == cases[i].vertices);
        CHECK(document.triangle_count == cases[i].triangles);
        CHECK(document.surface_group_count == 1u);
        CHECK(strcmp(document.surface_groups[0].group_id, "sphere_shell") == 0);
        CHECK(document.surface_groups[0].triangle_count == cases[i].triangles);
        core_mesh_asset_runtime_document_free(&document);
    }
    return 0;
}

static int test_fixture_tokens(void) {
    static const char *authoring_needles[] = {
        "\"schema_variant\"",
        "\"mesh_asset_authoring_v1\"",
        "\"source_mode\"",
        "\"primitive_seed\"",
        "\"primitive_seeds\""
    };
    static const char *imported_authoring_needles[] = {
        "\"source_mode\"",
        "\"imported_mesh\"",
        "\"source_format\"",
        "\"stl\"",
        "\"source_to_asset_scale\""
    };
    static const char *runtime_needles[] = {
        "\"schema_variant\"",
        "\"mesh_asset_runtime_v1\"",
        "\"source_asset_id\"",
        "\"triangle_count\""
    };

    CHECK(file_contains_all("tests/fixtures/mesh_asset_authoring_v1_sample.json",
                            authoring_needles,
                            sizeof(authoring_needles) / sizeof(authoring_needles[0])));
    CHECK(file_contains_all("tests/fixtures/mesh_asset_authoring_v1_imported_stl_sample.json",
                            imported_authoring_needles,
                            sizeof(imported_authoring_needles) /
                                sizeof(imported_authoring_needles[0])));
    CHECK(file_contains_all("tests/fixtures/mesh_asset_runtime_v1_sample.json",
                            runtime_needles,
                            sizeof(runtime_needles) / sizeof(runtime_needles[0])));
    return 0;
}

static int test_authoring_document_file_roundtrip(void) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshAssetAuthoringDocument reloaded;
    const char *path = "/private/tmp/core_mesh_asset_authoring_roundtrip.json";

    core_mesh_asset_authoring_document_init(&document);
    core_mesh_asset_authoring_document_init(&reloaded);

    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_sample.json",
              &document).code == CORE_OK);
    CHECK(document.contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED);
    CHECK(document.primitive_seed_count == 2u);
    CHECK(document.primitive_seeds[0].kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE);
    CHECK(document.primitive_seeds[1].kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM);
    CHECK(core_mesh_asset_authoring_document_save_file(&document, path).code == CORE_OK);
    CHECK(core_mesh_asset_authoring_document_load_file(path, &reloaded).code == CORE_OK);
    CHECK(strcmp(reloaded.contract.asset_id, "asset_bookshelf_01") == 0);
    CHECK(reloaded.primitive_seed_count == 2u);
    CHECK(strcmp(reloaded.primitive_seeds[1].object.object_type, "rect_prism_primitive") == 0);

    core_mesh_asset_authoring_document_free(&document);
    core_mesh_asset_authoring_document_free(&reloaded);
    remove(path);
    return 0;
}

static int test_imported_mesh_authoring_document_file_roundtrip(void) {
    CoreMeshAssetAuthoringDocument document;
    CoreMeshAssetAuthoringDocument reloaded;
    const char *path = "/private/tmp/core_mesh_asset_imported_mesh_roundtrip.json";

    core_mesh_asset_authoring_document_init(&document);
    core_mesh_asset_authoring_document_init(&reloaded);

    CHECK(core_mesh_asset_authoring_document_load_file(
              "tests/fixtures/mesh_asset_authoring_v1_imported_stl_sample.json",
              &document).code == CORE_OK);
    CHECK(document.contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH);
    CHECK(document.has_imported_mesh_source);
    CHECK(document.primitive_seed_count == 0u);
    CHECK(document.primitive_seeds == NULL);
    CHECK(document.imported_mesh_source.source_format ==
          CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL);
    CHECK(strcmp(document.imported_mesh_source.source_uri, "imports/bracket.stl") == 0);
    CHECK(document.imported_mesh_source.source_unit_kind == CORE_UNIT_MILLIMETER);
    CHECK(core_mesh_asset_authoring_document_save_file(&document, path).code == CORE_OK);
    CHECK(core_mesh_asset_authoring_document_load_file(path, &reloaded).code == CORE_OK);
    CHECK(strcmp(reloaded.contract.asset_id, "asset_imported_bracket_01") == 0);
    CHECK(reloaded.contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH);
    CHECK(reloaded.has_imported_mesh_source);
    CHECK(strcmp(reloaded.imported_mesh_source.default_surface_group_id,
                 "imported_surface") == 0);

    core_mesh_asset_authoring_document_free(&document);
    core_mesh_asset_authoring_document_free(&reloaded);
    remove(path);
    return 0;
}

int main(void) {
    if (test_parse_helpers() != 0) return 1;
    if (test_authoring_contract_validation() != 0) return 1;
    if (test_authoring_document_validation() != 0) return 1;
    if (test_imported_mesh_authoring_document_validation() != 0) return 1;
    if (test_runtime_contract_validation() != 0) return 1;
    if (test_runtime_document_validation() != 0) return 1;
    if (test_runtime_document_file_load() != 0) return 1;
    if (test_runtime_document_file_roundtrip() != 0) return 1;
    if (test_runtime_document_large_file_save_streams() != 0) return 1;
    if (test_runtime_document_mrt0_sphere_fixtures() != 0) return 1;
    if (test_fixture_tokens() != 0) return 1;
    if (test_authoring_document_file_roundtrip() != 0) return 1;
    if (test_imported_mesh_authoring_document_file_roundtrip() != 0) return 1;
    return 0;
}
