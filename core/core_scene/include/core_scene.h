#ifndef CORE_SCENE_H
#define CORE_SCENE_H

#include <stdbool.h>
#include <stddef.h>

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CoreSceneSourceType {
    CORE_SCENE_SOURCE_UNKNOWN = 0,
    CORE_SCENE_SOURCE_MANIFEST = 1,
    CORE_SCENE_SOURCE_PACK = 2,
    CORE_SCENE_SOURCE_VF2D = 3
} CoreSceneSourceType;

typedef struct CoreSceneBundleInfo {
    char bundle_type[128];
    int bundle_version;
    char profile[64];

    char fluid_source_kind[32];
    char fluid_source_path[4096];
    CoreSceneSourceType fluid_source_type;

    bool has_camera_path;
    char camera_path[4096];
    bool has_light_path;
    char light_path[4096];
    bool has_asset_mapping_profile;
    char asset_mapping_profile[128];
} CoreSceneBundleInfo;

CoreResult core_scene_dirname(const char *path, char *out_dir, size_t out_dir_size);
CoreResult core_scene_resolve_path(const char *base_dir, const char *input_path, char *out_path, size_t out_path_size);
CoreSceneSourceType core_scene_detect_source_type(const char *path);
bool core_scene_path_is_scene_bundle(const char *path);
CoreResult core_scene_bundle_resolve(const char *bundle_path, CoreSceneBundleInfo *out_info);

#ifdef __cplusplus
}
#endif

#endif
