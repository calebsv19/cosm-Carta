#include "core_scene.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

    printf("core_scene tests passed\n");
    return 0;
}
