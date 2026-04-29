#include "core_scene.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int test_typed_scene_contract_helpers(void) {
    CoreSceneRootContract root;
    CoreSceneObjectContract plane_object;
    CoreSceneObjectContract prism_object;
    CoreSceneSpaceMode mode = CORE_SCENE_SPACE_MODE_UNKNOWN;
    CoreSceneObjectKind kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    CoreResult r;

    if (strcmp(core_scene_space_mode_name(CORE_SCENE_SPACE_MODE_2D), "2d") != 0) return 1;
    if (strcmp(core_scene_space_mode_name(CORE_SCENE_SPACE_MODE_3D), "3d") != 0) return 1;
    if (core_scene_space_mode_parse("3d", &mode).code != CORE_OK) return 1;
    if (mode != CORE_SCENE_SPACE_MODE_3D) return 1;

    if (strcmp(core_scene_object_kind_name(CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE),
               "plane_primitive") != 0) return 1;
    if (core_scene_object_kind_parse("rect_prism_primitive", &kind).code != CORE_OK) return 1;
    if (kind != CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) return 1;

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

    return 0;
}

int main(void) {
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
