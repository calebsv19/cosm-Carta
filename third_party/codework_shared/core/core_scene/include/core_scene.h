#ifndef CORE_SCENE_H
#define CORE_SCENE_H

#include <stdbool.h>
#include <stddef.h>

#include "core_base.h"
#include "core_object.h"
#include "core_units.h"

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

typedef enum CoreSceneSpaceMode {
    CORE_SCENE_SPACE_MODE_UNKNOWN = 0,
    CORE_SCENE_SPACE_MODE_2D = 1,
    CORE_SCENE_SPACE_MODE_3D = 2
} CoreSceneSpaceMode;

typedef enum CoreSceneObjectKind {
    CORE_SCENE_OBJECT_KIND_UNKNOWN = 0,
    CORE_SCENE_OBJECT_KIND_CURVE_PATH = 1,
    CORE_SCENE_OBJECT_KIND_POINT_SET = 2,
    CORE_SCENE_OBJECT_KIND_EDGE_SET = 3,
    CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE = 4,
    CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE = 5
} CoreSceneObjectKind;

typedef struct CoreSceneFrame3 {
    CoreObjectVec3 origin;
    CoreObjectVec3 axis_u;
    CoreObjectVec3 axis_v;
    CoreObjectVec3 normal;
} CoreSceneFrame3;

typedef struct CoreScenePlanePrimitive {
    double width;
    double height;
    bool lock_to_construction_plane;
    bool lock_to_bounds;
    CoreSceneFrame3 frame;
} CoreScenePlanePrimitive;

typedef struct CoreSceneRectPrismPrimitive {
    double width;
    double height;
    double depth;
    bool lock_to_construction_plane;
    bool lock_to_bounds;
    CoreSceneFrame3 frame;
} CoreSceneRectPrismPrimitive;

typedef struct CoreSceneRootContract {
    char scene_id[64];
    CoreSceneSpaceMode space_mode_intent;
    CoreSceneSpaceMode space_mode_default;
    CoreUnitKind unit_kind;
    double world_scale;
} CoreSceneRootContract;

typedef struct CoreSceneObjectContract {
    CoreObject object;
    CoreSceneObjectKind kind;
    bool has_plane_primitive;
    CoreScenePlanePrimitive plane_primitive;
    bool has_rect_prism_primitive;
    CoreSceneRectPrismPrimitive rect_prism_primitive;
} CoreSceneObjectContract;

CoreResult core_scene_dirname(const char *path, char *out_dir, size_t out_dir_size);
CoreResult core_scene_resolve_path(const char *base_dir, const char *input_path, char *out_path, size_t out_path_size);
CoreSceneSourceType core_scene_detect_source_type(const char *path);
bool core_scene_path_is_scene_bundle(const char *path);
CoreResult core_scene_bundle_resolve(const char *bundle_path, CoreSceneBundleInfo *out_info);

const char *core_scene_space_mode_name(CoreSceneSpaceMode mode);
CoreResult core_scene_space_mode_parse(const char *text, CoreSceneSpaceMode *out_mode);
const char *core_scene_object_kind_name(CoreSceneObjectKind kind);
CoreResult core_scene_object_kind_parse(const char *text, CoreSceneObjectKind *out_kind);

void core_scene_root_contract_init(CoreSceneRootContract *contract);
CoreResult core_scene_root_contract_set_scene_id(CoreSceneRootContract *contract, const char *scene_id);
CoreResult core_scene_root_contract_validate(const CoreSceneRootContract *contract);

void core_scene_plane_primitive_init(CoreScenePlanePrimitive *primitive);
CoreResult core_scene_plane_primitive_validate(const CoreScenePlanePrimitive *primitive);
void core_scene_rect_prism_primitive_init(CoreSceneRectPrismPrimitive *primitive);
CoreResult core_scene_rect_prism_primitive_validate(const CoreSceneRectPrismPrimitive *primitive);

void core_scene_object_contract_init(CoreSceneObjectContract *contract);
CoreResult core_scene_object_contract_prepare(CoreSceneObjectContract *contract,
                                              const char *object_id,
                                              CoreSceneObjectKind kind);
CoreResult core_scene_object_contract_validate(const CoreSceneObjectContract *contract);

#ifdef __cplusplus
}
#endif

#endif
