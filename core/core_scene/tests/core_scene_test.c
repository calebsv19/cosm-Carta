#include "core_scene.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { printf("fail:%d\n", __LINE__); return 1; } } while (0)

static int write_text_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (fwrite(text, 1, strlen(text), f) != strlen(text)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

static int test_parse_failures_and_root_contract_edges(void) {
    CoreSceneSpaceMode mode = CORE_SCENE_SPACE_MODE_3D;
    CoreSceneObjectKind kind = CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE;
    CoreSceneRootContract root;

    CHECK(core_scene_space_mode_parse(NULL, &mode).code == CORE_ERR_INVALID_ARG);
    CHECK(mode == CORE_SCENE_SPACE_MODE_UNKNOWN);
    CHECK(core_scene_space_mode_parse("4d", &mode).code == CORE_ERR_NOT_FOUND);
    CHECK(mode == CORE_SCENE_SPACE_MODE_UNKNOWN);
    CHECK(core_scene_space_mode_parse("2d", NULL).code == CORE_ERR_INVALID_ARG);

    CHECK(core_scene_object_kind_parse(NULL, &kind).code == CORE_ERR_INVALID_ARG);
    CHECK(kind == CORE_SCENE_OBJECT_KIND_UNKNOWN);
    CHECK(core_scene_object_kind_parse("capsule_primitive", &kind).code == CORE_ERR_NOT_FOUND);
    CHECK(kind == CORE_SCENE_OBJECT_KIND_UNKNOWN);
    CHECK(core_scene_object_kind_parse("curve_path", NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_scene_geometry_ref_kind_parse(NULL, NULL).code == CORE_ERR_INVALID_ARG);

    CHECK(strcmp(core_scene_space_mode_name(CORE_SCENE_SPACE_MODE_UNKNOWN), "unknown") == 0);
    CHECK(strcmp(core_scene_object_kind_name(CORE_SCENE_OBJECT_KIND_UNKNOWN), "unknown") == 0);
    CHECK(strcmp(core_scene_geometry_ref_kind_name(CORE_SCENE_GEOMETRY_REF_KIND_UNKNOWN), "unknown") == 0);

    core_scene_root_contract_init(&root);
    CHECK(core_scene_root_contract_validate(NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_scene_root_contract_validate(&root).code != CORE_OK);
    CHECK(core_scene_root_contract_set_scene_id(NULL, "scene").code == CORE_ERR_INVALID_ARG);
    CHECK(core_scene_root_contract_set_scene_id(&root, NULL).code == CORE_ERR_INVALID_ARG);
    CHECK(core_scene_root_contract_set_scene_id(&root, "").code == CORE_ERR_INVALID_ARG);

    {
        char long_id[80];
        memset(long_id, 'x', sizeof(long_id) - 1);
        long_id[sizeof(long_id) - 1] = '\0';
        CHECK(core_scene_root_contract_set_scene_id(&root, long_id).code == CORE_ERR_INVALID_ARG);
    }

    CHECK(core_scene_root_contract_set_scene_id(&root, "scene_contract").code == CORE_OK);
    root.space_mode_intent = (CoreSceneSpaceMode) 99;
    CHECK(core_scene_root_contract_validate(&root).code == CORE_ERR_INVALID_ARG);
    root.space_mode_intent = CORE_SCENE_SPACE_MODE_3D;
    root.space_mode_default = (CoreSceneSpaceMode) 99;
    CHECK(core_scene_root_contract_validate(&root).code == CORE_ERR_INVALID_ARG);
    root.space_mode_default = CORE_SCENE_SPACE_MODE_3D;
    root.unit_kind = CORE_UNIT_UNKNOWN;
    CHECK(core_scene_root_contract_validate(&root).code == CORE_ERR_INVALID_ARG);
    root.unit_kind = (CoreUnitKind) 99;
    CHECK(core_scene_root_contract_validate(&root).code == CORE_ERR_INVALID_ARG);
    root.unit_kind = CORE_UNIT_METER;
    root.world_scale = 0.0;
    CHECK(core_scene_root_contract_validate(&root).code == CORE_ERR_INVALID_ARG);
    root.world_scale = NAN;
    CHECK(core_scene_root_contract_validate(&root).code == CORE_ERR_INVALID_ARG);

    return 0;
}

static int test_frame_and_object_contract_edges(void) {
    CoreSceneObjectContract contract;

    if (core_scene_object_contract_validate(NULL).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_object_contract_prepare(NULL, "obj", CORE_SCENE_OBJECT_KIND_CURVE_PATH).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_object_contract_prepare(&contract, NULL, CORE_SCENE_OBJECT_KIND_CURVE_PATH).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_object_contract_prepare(&contract, "obj", CORE_SCENE_OBJECT_KIND_UNKNOWN).code != CORE_ERR_INVALID_ARG) return 1;

    core_scene_object_contract_init(&contract);
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;

    if (core_scene_object_contract_prepare(&contract, "obj_curve", CORE_SCENE_OBJECT_KIND_CURVE_PATH).code != CORE_OK) return 1;
    contract.has_plane_primitive = true;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;

    if (core_scene_object_contract_prepare(&contract, "obj_plane", CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE).code != CORE_OK) return 1;
    if (contract.object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) return 1;
    contract.has_plane_primitive = false;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.has_plane_primitive = true;
    contract.has_rect_prism_primitive = true;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.has_rect_prism_primitive = false;
    contract.object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    contract.plane_primitive.width = 0.0;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.plane_primitive.width = 1.0;
    contract.plane_primitive.frame.origin.x = NAN;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.plane_primitive.frame.origin.x = 0.0;
    contract.plane_primitive.frame.axis_u.x = 0.0;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.plane_primitive.frame.axis_u.x = 1.0;
    contract.plane_primitive.frame.axis_v.x = 1.0;
    contract.plane_primitive.frame.axis_v.y = 0.0;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.plane_primitive.frame.axis_v.x = 0.0;
    contract.plane_primitive.frame.axis_v.y = 1.0;
    contract.plane_primitive.frame.normal.z = 2.0;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.plane_primitive.frame.normal.z = -1.0;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;

    if (core_scene_object_contract_prepare(&contract, "obj_prism", CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE).code != CORE_OK) return 1;
    if (contract.object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) return 1;
    contract.has_rect_prism_primitive = false;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.has_rect_prism_primitive = true;
    contract.has_plane_primitive = true;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.has_plane_primitive = false;
    contract.object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;
    contract.object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    contract.rect_prism_primitive.depth = 0.0;
    if (core_scene_object_contract_validate(&contract).code != CORE_ERR_INVALID_ARG) return 1;

    return 0;
}

static int test_path_and_bundle_edges(void) {
    char out[256];
    char tiny[4];
    CoreResult r;

    if (core_scene_dirname(NULL, out, sizeof(out)).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_dirname("/tmp/foo", NULL, sizeof(out)).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_dirname("/tmp/foo", out, 0).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_dirname("scene_bundle.json", tiny, sizeof(tiny)).code != CORE_OK) return 1;
    if (strcmp(tiny, ".") != 0) return 1;
    if (core_scene_dirname("/", out, sizeof(out)).code != CORE_OK) return 1;
    if (strcmp(out, "/") != 0) return 1;
    if (core_scene_dirname("/scene_bundle.json", out, sizeof(out)).code != CORE_OK) return 1;
    if (strcmp(out, "/") != 0) return 1;
    if (core_scene_dirname("/tmp/foo/scene_bundle.json", tiny, 2).code != CORE_ERR_INVALID_ARG) return 1;

    if (core_scene_resolve_path(NULL, "x", out, sizeof(out)).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_resolve_path(".", NULL, out, sizeof(out)).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_resolve_path(".", "x", NULL, sizeof(out)).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_resolve_path(".", "x", out, 0).code != CORE_ERR_INVALID_ARG) return 1;
    if (core_scene_resolve_path("", "manifest.json", out, sizeof(out)).code != CORE_OK) return 1;
    if (strcmp(out, "manifest.json") != 0) return 1;
    if (core_scene_resolve_path("/tmp/foo", "/abs/file.pack", out, sizeof(out)).code != CORE_OK) return 1;
    if (strcmp(out, "/abs/file.pack") != 0) return 1;
    if (core_scene_resolve_path("/tmp/foo", "../ray/camera.json", out, sizeof(out)).code != CORE_OK) return 1;
    if (strcmp(out, "/tmp/foo/../ray/camera.json") != 0) return 1;
    if (core_scene_resolve_path("/tmp/foo", "bar/manifest.json", tiny, sizeof(tiny)).code != CORE_ERR_INVALID_ARG) return 1;

    if (!core_scene_path_is_scene_bundle("scene_bundle.json")) return 1;
    if (!core_scene_path_is_scene_bundle("test.scene.json")) return 1;
    if (core_scene_path_is_scene_bundle("test.json")) return 1;
    if (core_scene_detect_source_type(NULL) != CORE_SCENE_SOURCE_UNKNOWN) return 1;
    if (core_scene_detect_source_type("x.unknown") != CORE_SCENE_SOURCE_UNKNOWN) return 1;

    if (core_scene_bundle_resolve(NULL, (CoreSceneBundleInfo *)&out).code != CORE_ERR_INVALID_ARG) return 1;

    {
        CoreSceneBundleInfo info;
        if (core_scene_bundle_resolve("/tmp/not_a_bundle.json", &info).code != CORE_ERR_FORMAT) return 1;
        if (core_scene_bundle_resolve("/tmp/missing.scene.json", &info).code != CORE_ERR_IO) return 1;
    }

    {
        const char *path = "/tmp/core_scene_invalid_type.scene.json";
        CoreSceneBundleInfo info;
        if (!write_text_file(path, "{\"bundle_type\":\"bad_bundle\",\"fluid_source\":{\"path\":\"manifest.json\"}}")) return 1;
        r = core_scene_bundle_resolve(path, &info);
        remove(path);
        if (r.code != CORE_ERR_FORMAT) return 1;
    }

    {
        const char *path = "/tmp/core_scene_missing_path.scene.json";
        CoreSceneBundleInfo info;
        if (!write_text_file(path, "{\"bundle_type\":\"physics_scene_bundle_v1\"}")) return 1;
        r = core_scene_bundle_resolve(path, &info);
        remove(path);
        if (r.code != CORE_ERR_FORMAT) return 1;
    }

    {
        const char *path = "/tmp/core_scene_bad_source.scene.json";
        CoreSceneBundleInfo info;
        if (!write_text_file(path, "{\"bundle_type\":\"physics_scene_bundle_v1\",\"fluid_source\":{\"path\":\"fluid.xyz\"}}")) return 1;
        r = core_scene_bundle_resolve(path, &info);
        remove(path);
        if (r.code != CORE_ERR_FORMAT) return 1;
    }

    {
        const char *path = "/tmp/core_scene_manifest_fallback.scene.json";
        CoreSceneBundleInfo info;
        if (!write_text_file(path, "{\"bundle_type\":\"physics_scene_bundle_v1\",\"manifest_path\":\"manifest.json\"}")) return 1;
        r = core_scene_bundle_resolve(path, &info);
        remove(path);
        if (r.code != CORE_OK) return 1;
        if (info.fluid_source_type != CORE_SCENE_SOURCE_MANIFEST) return 1;
        if (strcmp(info.fluid_source_path, "/tmp/manifest.json") != 0) return 1;
    }

    {
        const char *path = "/tmp/core_scene_fluid_fallback.scene.json";
        CoreSceneBundleInfo info;
        if (!write_text_file(path, "{\"bundle_type\":\"physics_scene_bundle_v1\",\"fluid_path\":\"field.vf2d\",\"camera_path\":\"camera.json\",\"asset_mapping_profile\":\"ray_v1\"}")) return 1;
        r = core_scene_bundle_resolve(path, &info);
        remove(path);
        if (r.code != CORE_OK) return 1;
        if (info.fluid_source_type != CORE_SCENE_SOURCE_VF2D) return 1;
        if (strcmp(info.fluid_source_path, "/tmp/field.vf2d") != 0) return 1;
        if (!info.has_camera_path || strcmp(info.camera_path, "/tmp/camera.json") != 0) return 1;
        if (!info.has_asset_mapping_profile || strcmp(info.asset_mapping_profile, "ray_v1") != 0) return 1;
    }

    return 0;
}

static int test_typed_scene_contract_helpers(void) {
    CoreSceneRootContract root;
    CoreSceneObjectContract plane_object;
    CoreSceneObjectContract prism_object;
    CoreSceneObjectContract mesh_object;
    CoreSceneSpaceMode mode = CORE_SCENE_SPACE_MODE_UNKNOWN;
    CoreSceneObjectKind kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    CoreSceneGeometryRefKind geometry_kind = CORE_SCENE_GEOMETRY_REF_KIND_UNKNOWN;
    CoreResult r;

    if (strcmp(core_scene_space_mode_name(CORE_SCENE_SPACE_MODE_2D), "2d") != 0) return 1;
    if (strcmp(core_scene_space_mode_name(CORE_SCENE_SPACE_MODE_3D), "3d") != 0) return 1;
    if (core_scene_space_mode_parse("3d", &mode).code != CORE_OK) return 1;
    if (mode != CORE_SCENE_SPACE_MODE_3D) return 1;

    if (strcmp(core_scene_object_kind_name(CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE),
               "plane_primitive") != 0) return 1;
    if (core_scene_object_kind_parse("rect_prism_primitive", &kind).code != CORE_OK) return 1;
    if (kind != CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) return 1;
    if (core_scene_object_kind_parse("mesh_asset_instance", &kind).code != CORE_OK) return 1;
    if (kind != CORE_SCENE_OBJECT_KIND_MESH_ASSET_INSTANCE) return 1;
    if (core_scene_geometry_ref_kind_parse("mesh_asset", &geometry_kind).code != CORE_OK) return 1;
    if (geometry_kind != CORE_SCENE_GEOMETRY_REF_KIND_MESH_ASSET) return 1;
    if (strcmp(core_scene_geometry_ref_kind_name(CORE_SCENE_GEOMETRY_REF_KIND_SHAPE_ASSET),
               "shape_asset") != 0) return 1;

    core_scene_root_contract_init(&root);
    r = core_scene_root_contract_set_scene_id(&root, "scene_contract");
    if (r.code != CORE_OK) return 1;
    root.space_mode_intent = CORE_SCENE_SPACE_MODE_3D;
    root.space_mode_default = CORE_SCENE_SPACE_MODE_3D;
    root.unit_kind = CORE_UNIT_METER;
    root.world_scale = 1.0;
    if (core_scene_root_contract_validate(&root).code != CORE_OK) return 1;

    core_scene_object_contract_prepare(&plane_object, "obj_plane", CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE);
    plane_object.object.locked_plane = CORE_OBJECT_PLANE_YZ;
    plane_object.has_plane_primitive = true;
    plane_object.plane_primitive.width = 4.0;
    plane_object.plane_primitive.height = 2.0;
    plane_object.plane_primitive.frame.axis_u.x = 0.0;
    plane_object.plane_primitive.frame.axis_u.y = 1.0;
    plane_object.plane_primitive.frame.axis_u.z = 0.0;
    plane_object.plane_primitive.frame.axis_v.x = 0.0;
    plane_object.plane_primitive.frame.axis_v.y = 0.0;
    plane_object.plane_primitive.frame.axis_v.z = 1.0;
    plane_object.plane_primitive.frame.normal.x = 1.0;
    plane_object.plane_primitive.frame.normal.y = 0.0;
    plane_object.plane_primitive.frame.normal.z = 0.0;
    if (core_scene_object_contract_validate(&plane_object).code != CORE_OK) return 1;

    core_scene_object_contract_prepare(&prism_object, "obj_prism", CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE);
    prism_object.has_rect_prism_primitive = true;
    prism_object.rect_prism_primitive.width = 3.0;
    prism_object.rect_prism_primitive.height = 2.0;
    prism_object.rect_prism_primitive.depth = 1.5;
    if (core_scene_object_contract_validate(&prism_object).code != CORE_OK) return 1;

    prism_object.rect_prism_primitive.depth = 0.0;
    if (core_scene_object_contract_validate(&prism_object).code == CORE_OK) return 1;

    core_scene_object_contract_prepare(&mesh_object, "obj_mesh", CORE_SCENE_OBJECT_KIND_MESH_ASSET_INSTANCE);
    if (mesh_object.object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) return 1;
    if (core_scene_object_contract_validate(&mesh_object).code != CORE_OK) return 1;
    mesh_object.has_plane_primitive = true;
    if (core_scene_object_contract_validate(&mesh_object).code == CORE_OK) return 1;

    return 0;
}

int main(void) {
    if (test_parse_failures_and_root_contract_edges() != 0) return 1;
    if (test_frame_and_object_contract_edges() != 0) return 1;
    if (test_path_and_bundle_edges() != 0) return 1;

    char dir[256];
    CoreResult r = core_scene_dirname("/tmp/foo/scene_bundle.json", dir, sizeof(dir));
    if (r.code != CORE_OK || strcmp(dir, "/tmp/foo") != 0) return 1;

    char resolved[256];
    r = core_scene_resolve_path("/tmp/foo", "bar/manifest.json", resolved, sizeof(resolved));
    if (r.code != CORE_OK || strcmp(resolved, "/tmp/foo/bar/manifest.json") != 0) return 1;

    if (!core_scene_path_is_scene_bundle("scene_bundle.json")) return 1;
    if (core_scene_detect_source_type("x.pack") != CORE_SCENE_SOURCE_PACK) return 1;
    if (core_scene_detect_source_type("x.vf2d") != CORE_SCENE_SOURCE_VF2D) return 1;
    if (core_scene_detect_source_type("x/manifest.json") != CORE_SCENE_SOURCE_MANIFEST) return 1;

    const char *bundle_path = "/tmp/core_scene_bundle_test.scene.json";
    FILE *f = fopen(bundle_path, "wb");
    if (!f) return 1;
    const char *json =
        "{"
        "\"bundle_type\":\"physics_scene_bundle_v1\","
        "\"bundle_version\":1,"
        "\"profile\":\"physics\","
        "\"fluid_source\":{\"kind\":\"manifest\",\"path\":\"manifest.json\"},"
        "\"scene_metadata\":{"
            "\"camera_path\":\"../ray/camera.json\","
            "\"light_path\":\"../ray/light.json\","
            "\"asset_mapping_profile\":\"physics_to_ray_v1\""
        "}"
        "}";
    if (fwrite(json, 1, strlen(json), f) != strlen(json)) {
        fclose(f);
        return 1;
    }
    fclose(f);

    CoreSceneBundleInfo info;
    r = core_scene_bundle_resolve(bundle_path, &info);
    remove(bundle_path);
    if (r.code != CORE_OK) return 1;
    if (strcmp(info.bundle_type, "physics_scene_bundle_v1") != 0) return 1;
    if (info.bundle_version != 1) return 1;
    if (strcmp(info.profile, "physics") != 0) return 1;
    if (info.fluid_source_type != CORE_SCENE_SOURCE_MANIFEST) return 1;
    if (strcmp(info.fluid_source_path, "/tmp/manifest.json") != 0) return 1;
    if (!info.has_camera_path || strcmp(info.camera_path, "/tmp/../ray/camera.json") != 0) return 1;
    if (!info.has_light_path || strcmp(info.light_path, "/tmp/../ray/light.json") != 0) return 1;
    if (!info.has_asset_mapping_profile || strcmp(info.asset_mapping_profile, "physics_to_ray_v1") != 0) return 1;
    if (test_typed_scene_contract_helpers() != 0) return 1;

    printf("core_scene tests passed\n");
    return 0;
}
