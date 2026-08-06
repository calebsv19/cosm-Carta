#include "core_mesh_asset.h"

#include <math.h>
#include <string.h>

static CoreResult core_mesh_asset_invalid_arg(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static bool core_mesh_asset_text_equals(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static bool core_mesh_asset_vec3_is_finite(CoreObjectVec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

static double core_mesh_asset_vec3_length_sq(CoreObjectVec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static CoreResult core_mesh_asset_copy_id(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') {
        return core_mesh_asset_invalid_arg("id must be non-empty");
    }
    len = strlen(src);
    if (len >= dst_size) {
        return core_mesh_asset_invalid_arg("id too long");
    }
    memcpy(dst, src, len + 1u);
    return core_result_ok();
}

static CoreResult core_mesh_asset_validate_frame(const CoreMeshAssetFrame3 *frame) {
    if (!frame) {
        return core_mesh_asset_invalid_arg("frame is null");
    }
    if (!core_mesh_asset_vec3_is_finite(frame->origin) ||
        !core_mesh_asset_vec3_is_finite(frame->axis_u) ||
        !core_mesh_asset_vec3_is_finite(frame->axis_v) ||
        !core_mesh_asset_vec3_is_finite(frame->normal)) {
        return core_mesh_asset_invalid_arg("frame vectors must be finite");
    }
    if (core_mesh_asset_vec3_length_sq(frame->axis_u) <= 0.0 ||
        core_mesh_asset_vec3_length_sq(frame->axis_v) <= 0.0 ||
        core_mesh_asset_vec3_length_sq(frame->normal) <= 0.0) {
        return core_mesh_asset_invalid_arg("frame axes must be non-zero");
    }
    return core_result_ok();
}

const char *core_mesh_asset_schema_variant_name(CoreMeshAssetSchemaVariant variant) {
    switch (variant) {
        case CORE_MESH_ASSET_SCHEMA_VARIANT_AUTHORING_V1:
            return "mesh_asset_authoring_v1";
        case CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1:
            return "mesh_asset_runtime_v1";
        case CORE_MESH_ASSET_SCHEMA_VARIANT_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_asset_schema_variant_parse(const char *text,
                                                CoreMeshAssetSchemaVariant *out_variant) {
    if (out_variant) {
        *out_variant = CORE_MESH_ASSET_SCHEMA_VARIANT_UNKNOWN;
    }
    if (!text || !out_variant) {
        return core_mesh_asset_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_text_equals(text, "mesh_asset_authoring_v1")) {
        *out_variant = CORE_MESH_ASSET_SCHEMA_VARIANT_AUTHORING_V1;
        return core_result_ok();
    }
    if (core_mesh_asset_text_equals(text, "mesh_asset_runtime_v1")) {
        *out_variant = CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1;
        return core_result_ok();
    }
    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown mesh asset schema variant" };
        return r;
    }
}

const char *core_mesh_asset_type_name(CoreMeshAssetType type) {
    switch (type) {
        case CORE_MESH_ASSET_TYPE_SOLID_MESH:
            return "solid_mesh";
        case CORE_MESH_ASSET_TYPE_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_asset_type_parse(const char *text, CoreMeshAssetType *out_type) {
    if (out_type) {
        *out_type = CORE_MESH_ASSET_TYPE_UNKNOWN;
    }
    if (!text || !out_type) {
        return core_mesh_asset_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_text_equals(text, "solid_mesh")) {
        *out_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
        return core_result_ok();
    }
    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown mesh asset type" };
        return r;
    }
}

const char *core_mesh_asset_primitive_seed_kind_name(CoreMeshAssetPrimitiveSeedKind kind) {
    switch (kind) {
        case CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE:
            return "plane";
        case CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM:
            return "rect_prism";
        case CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_asset_primitive_seed_kind_parse(const char *text,
                                                     CoreMeshAssetPrimitiveSeedKind *out_kind) {
    if (out_kind) {
        *out_kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_UNKNOWN;
    }
    if (!text || !out_kind) {
        return core_mesh_asset_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_text_equals(text, "plane")) {
        *out_kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE;
        return core_result_ok();
    }
    if (core_mesh_asset_text_equals(text, "rect_prism")) {
        *out_kind = CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM;
        return core_result_ok();
    }
    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown mesh asset primitive seed kind" };
        return r;
    }
}

const char *core_mesh_asset_source_mode_name(CoreMeshAssetSourceMode mode) {
    switch (mode) {
        case CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION:
            return "profile_extrusion";
        case CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED:
            return "primitive_seed";
        case CORE_MESH_ASSET_SOURCE_MODE_REVOLVE:
            return "revolve";
        case CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH:
            return "imported_mesh";
        case CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_asset_source_mode_parse(const char *text, CoreMeshAssetSourceMode *out_mode) {
    if (out_mode) {
        *out_mode = CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN;
    }
    if (!text || !out_mode) {
        return core_mesh_asset_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_text_equals(text, "profile_extrusion")) {
        *out_mode = CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION;
        return core_result_ok();
    }
    if (core_mesh_asset_text_equals(text, "primitive_seed")) {
        *out_mode = CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED;
        return core_result_ok();
    }
    if (core_mesh_asset_text_equals(text, "revolve")) {
        *out_mode = CORE_MESH_ASSET_SOURCE_MODE_REVOLVE;
        return core_result_ok();
    }
    if (core_mesh_asset_text_equals(text, "imported_mesh")) {
        *out_mode = CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH;
        return core_result_ok();
    }
    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown mesh asset source mode" };
        return r;
    }
}

const char *core_mesh_asset_imported_mesh_source_format_name(
    CoreMeshAssetImportedMeshSourceFormat format) {
    switch (format) {
        case CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL:
            return "stl";
        case CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_asset_imported_mesh_source_format_parse(
    const char *text,
    CoreMeshAssetImportedMeshSourceFormat *out_format) {
    if (out_format) {
        *out_format = CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_UNKNOWN;
    }
    if (!text || !out_format) {
        return core_mesh_asset_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_text_equals(text, "stl")) {
        *out_format = CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL;
        return core_result_ok();
    }
    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown imported mesh source format" };
        return r;
    }
}

const char *core_mesh_asset_runtime_normal_provenance_name(
    CoreMeshAssetRuntimeNormalProvenance provenance) {
    switch (provenance) {
        case CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_NONE: return "none";
        case CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_SOURCE: return "source";
        case CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH:
            return "generated_smooth";
        case CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE:
            return "generated_crease_aware";
        default: return "unknown";
    }
}

CoreResult core_mesh_asset_runtime_normal_provenance_parse(
    const char *text,
    CoreMeshAssetRuntimeNormalProvenance *out_provenance) {
    if (!out_provenance) {
        return core_mesh_asset_invalid_arg("out_provenance is null");
    }
    *out_provenance = CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_NONE;
    if (!text) {
        return core_mesh_asset_invalid_arg("normal provenance is null");
    }
    if (strcmp(text, "none") == 0) {
        return core_result_ok();
    }
    if (strcmp(text, "source") == 0) {
        *out_provenance = CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_SOURCE;
        return core_result_ok();
    }
    if (strcmp(text, "generated_smooth") == 0) {
        *out_provenance = CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
        return core_result_ok();
    }
    if (strcmp(text, "generated_crease_aware") == 0) {
        *out_provenance = CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE;
        return core_result_ok();
    }
    return core_mesh_asset_invalid_arg("unknown runtime normal provenance");
}

const char *core_mesh_asset_imported_normal_mode_name(CoreMeshAssetImportedNormalMode mode) {
    switch (mode) {
        case CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_NONE: return "none";
        case CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_SMOOTH: return "smooth";
        case CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_CREASE_AWARE: return "crease_aware";
        default: return "unknown";
    }
}

CoreResult core_mesh_asset_imported_normal_mode_parse(
    const char *text,
    CoreMeshAssetImportedNormalMode *out_mode) {
    if (!out_mode) {
        return core_mesh_asset_invalid_arg("out_mode is null");
    }
    *out_mode = CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_NONE;
    if (!text) {
        return core_mesh_asset_invalid_arg("normal mode is null");
    }
    if (strcmp(text, "none") == 0) return core_result_ok();
    if (strcmp(text, "smooth") == 0) {
        *out_mode = CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_SMOOTH;
        return core_result_ok();
    }
    if (strcmp(text, "crease_aware") == 0) {
        *out_mode = CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_CREASE_AWARE;
        return core_result_ok();
    }
    return core_mesh_asset_invalid_arg("unknown imported normal mode");
}

void core_mesh_asset_imported_mesh_source_init(CoreMeshAssetImportedMeshSource *source) {
    if (!source) {
        return;
    }
    memset(source, 0, sizeof(*source));
    source->source_unit_kind = CORE_UNIT_METER;
    source->source_to_asset_scale = 1.0;
    memcpy(source->orientation_policy, "source_axes", sizeof("source_axes"));
    memcpy(source->default_surface_group_id, "imported_surface", sizeof("imported_surface"));
    source->weld_vertices = true;
    source->weld_tolerance = 0.000001;
    source->preserve_source_normals = false;
    source->normal_mode = CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_NONE;
    source->crease_angle_degrees = 180.0;
}

CoreResult core_mesh_asset_imported_mesh_source_validate(
    const CoreMeshAssetImportedMeshSource *source) {
    if (!source) {
        return core_mesh_asset_invalid_arg("imported mesh source is null");
    }
    if (source->import_id[0] == '\0') {
        return core_mesh_asset_invalid_arg("import_id is required");
    }
    if (source->source_format != CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL) {
        return core_mesh_asset_invalid_arg("known imported mesh source_format is required");
    }
    if (source->source_uri[0] == '\0') {
        return core_mesh_asset_invalid_arg("source_uri is required");
    }
    if (source->source_unit_kind == CORE_UNIT_UNKNOWN) {
        return core_mesh_asset_invalid_arg("known source_unit_kind is required");
    }
    if (source->orientation_policy[0] == '\0') {
        return core_mesh_asset_invalid_arg("orientation_policy is required");
    }
    if (source->default_surface_group_id[0] == '\0') {
        return core_mesh_asset_invalid_arg("default_surface_group_id is required");
    }
    if (!isfinite(source->source_to_asset_scale) || source->source_to_asset_scale <= 0.0) {
        return core_mesh_asset_invalid_arg("source_to_asset_scale must be positive and finite");
    }
    if (!isfinite(source->weld_tolerance) || source->weld_tolerance < 0.0) {
        return core_mesh_asset_invalid_arg("weld_tolerance must be finite and non-negative");
    }
    if (source->normal_mode != CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_NONE &&
        source->normal_mode != CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_SMOOTH &&
        source->normal_mode != CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_CREASE_AWARE) {
        return core_mesh_asset_invalid_arg("known normal_mode is required");
    }
    if (!isfinite(source->crease_angle_degrees) || source->crease_angle_degrees <= 0.0 ||
        source->crease_angle_degrees > 180.0) {
        return core_mesh_asset_invalid_arg(
            "crease_angle_degrees must be finite and in (0, 180]");
    }
    return core_result_ok();
}

void core_mesh_asset_authoring_contract_init(CoreMeshAssetAuthoringContract *contract) {
    if (!contract) {
        return;
    }
    memset(contract, 0, sizeof(*contract));
    contract->unit_kind = CORE_UNIT_METER;
    contract->world_scale = 1.0;
    contract->asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    contract->source_mode = CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION;
    contract->pivot.axis_u.x = 1.0;
    contract->pivot.axis_v.y = 1.0;
    contract->pivot.normal.z = 1.0;
    contract->topology_closed_volume_expected = true;
    contract->topology_manifold_expected = true;
}

CoreResult core_mesh_asset_authoring_contract_set_asset_id(CoreMeshAssetAuthoringContract *contract,
                                                           const char *asset_id) {
    if (!contract) {
        return core_mesh_asset_invalid_arg("contract is null");
    }
    return core_mesh_asset_copy_id(contract->asset_id, sizeof(contract->asset_id), asset_id);
}

CoreResult core_mesh_asset_authoring_contract_validate(
    const CoreMeshAssetAuthoringContract *contract) {
    CoreResult r;
    if (!contract) {
        return core_mesh_asset_invalid_arg("contract is null");
    }
    if (contract->asset_id[0] == '\0') {
        return core_mesh_asset_invalid_arg("asset_id is required");
    }
    if (contract->asset_type != CORE_MESH_ASSET_TYPE_SOLID_MESH) {
        return core_mesh_asset_invalid_arg("known asset_type is required");
    }
    if (contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION &&
        contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED &&
        contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_REVOLVE &&
        contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        return core_mesh_asset_invalid_arg("known source_mode is required");
    }
    if (contract->unit_kind == CORE_UNIT_UNKNOWN) {
        return core_mesh_asset_invalid_arg("known unit_kind is required");
    }
    r = core_units_validate_world_scale(contract->world_scale);
    if (r.code != CORE_OK) {
        return r;
    }
    return core_mesh_asset_validate_frame(&contract->pivot);
}

void core_mesh_asset_runtime_contract_init(CoreMeshAssetRuntimeContract *contract) {
    if (!contract) {
        return;
    }
    memset(contract, 0, sizeof(*contract));
    contract->asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    contract->pivot.axis_u.x = 1.0;
    contract->pivot.axis_v.y = 1.0;
    contract->pivot.normal.z = 1.0;
    contract->topology_closed_volume = true;
    contract->topology_manifold_expected = true;
}

CoreResult core_mesh_asset_runtime_contract_set_asset_id(CoreMeshAssetRuntimeContract *contract,
                                                         const char *asset_id) {
    if (!contract) {
        return core_mesh_asset_invalid_arg("contract is null");
    }
    return core_mesh_asset_copy_id(contract->asset_id, sizeof(contract->asset_id), asset_id);
}

CoreResult core_mesh_asset_runtime_contract_set_source_asset_id(
    CoreMeshAssetRuntimeContract *contract,
    const char *asset_id) {
    if (!contract) {
        return core_mesh_asset_invalid_arg("contract is null");
    }
    return core_mesh_asset_copy_id(contract->source_asset_id,
                                   sizeof(contract->source_asset_id),
                                   asset_id);
}

CoreResult core_mesh_asset_runtime_contract_validate(const CoreMeshAssetRuntimeContract *contract) {
    if (!contract) {
        return core_mesh_asset_invalid_arg("contract is null");
    }
    if (contract->asset_id[0] == '\0' || contract->source_asset_id[0] == '\0') {
        return core_mesh_asset_invalid_arg("asset_id and source_asset_id are required");
    }
    if (contract->asset_type != CORE_MESH_ASSET_TYPE_SOLID_MESH) {
        return core_mesh_asset_invalid_arg("known asset_type is required");
    }
    if (contract->vertex_count == 0u || contract->triangle_count == 0u) {
        return core_mesh_asset_invalid_arg("runtime mesh counts must be positive");
    }
    if (!core_mesh_asset_vec3_is_finite(contract->local_bounds.min) ||
        !core_mesh_asset_vec3_is_finite(contract->local_bounds.max)) {
        return core_mesh_asset_invalid_arg("local bounds must be finite");
    }
    if (contract->local_bounds.min.x > contract->local_bounds.max.x ||
        contract->local_bounds.min.y > contract->local_bounds.max.y ||
        contract->local_bounds.min.z > contract->local_bounds.max.z) {
        return core_mesh_asset_invalid_arg("local bounds min must be <= max");
    }
    return core_mesh_asset_validate_frame(&contract->pivot);
}
