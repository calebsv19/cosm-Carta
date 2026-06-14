#include "core_mesh_compile.h"

#include <string.h>

static CoreResult core_mesh_compile_invalid_arg(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static bool core_mesh_compile_text_equals(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static CoreResult core_mesh_compile_copy_id(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') {
        return core_mesh_compile_invalid_arg("id must be non-empty");
    }
    len = strlen(src);
    if (len >= dst_size) {
        return core_mesh_compile_invalid_arg("id too long");
    }
    memcpy(dst, src, len + 1u);
    return core_result_ok();
}

const char *core_mesh_compile_geometry_ref_kind_name(CoreMeshCompileGeometryRefKind kind) {
    switch (kind) {
        case CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET:
            return "mesh_asset";
        case CORE_MESH_COMPILE_GEOMETRY_REF_KIND_UNKNOWN:
        default:
            return "unknown";
    }
}

CoreResult core_mesh_compile_geometry_ref_kind_parse(const char *text,
                                                     CoreMeshCompileGeometryRefKind *out_kind) {
    if (out_kind) {
        *out_kind = CORE_MESH_COMPILE_GEOMETRY_REF_KIND_UNKNOWN;
    }
    if (!text || !out_kind) {
        return core_mesh_compile_invalid_arg("invalid argument");
    }
    if (core_mesh_compile_text_equals(text, "mesh_asset")) {
        *out_kind = CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET;
        return core_result_ok();
    }
    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown geometry_ref kind" };
        return r;
    }
}

void core_mesh_compile_instance_contract_init(CoreMeshCompileInstanceContract *contract) {
    if (!contract) {
        return;
    }
    memset(contract, 0, sizeof(*contract));
    core_object_init(&contract->object);
}

CoreResult core_mesh_compile_instance_contract_prepare(CoreMeshCompileInstanceContract *contract,
                                                       const char *object_id,
                                                       const char *asset_id) {
    CoreResult r;
    if (!contract) {
        return core_mesh_compile_invalid_arg("contract is null");
    }
    core_mesh_compile_instance_contract_init(contract);
    r = core_object_set_identity(&contract->object, object_id, "mesh_asset_instance");
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_object_promote_to_full_3d(&contract->object);
    if (r.code != CORE_OK) {
        return r;
    }
    contract->geometry_ref_kind = CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET;
    contract->object.flags.visible = true;
    contract->object.flags.selectable = true;
    contract->object.flags.locked = false;
    return core_mesh_compile_copy_id(contract->asset_id, sizeof(contract->asset_id), asset_id);
}

CoreResult core_mesh_compile_instance_contract_validate(
    const CoreMeshCompileInstanceContract *contract) {
    CoreResult r;
    if (!contract) {
        return core_mesh_compile_invalid_arg("contract is null");
    }
    r = core_object_validate(&contract->object);
    if (r.code != CORE_OK) {
        return r;
    }
    if (!core_mesh_compile_text_equals(contract->object.object_type, "mesh_asset_instance")) {
        return core_mesh_compile_invalid_arg("object_type must be mesh_asset_instance");
    }
    if (contract->object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) {
        return core_mesh_compile_invalid_arg("mesh asset instances must be full_3d");
    }
    if (contract->geometry_ref_kind != CORE_MESH_COMPILE_GEOMETRY_REF_KIND_MESH_ASSET) {
        return core_mesh_compile_invalid_arg("geometry_ref.kind must be mesh_asset");
    }
    if (contract->asset_id[0] == '\0') {
        return core_mesh_compile_invalid_arg("asset_id is required");
    }
    if (contract->has_material_ref_id && contract->material_ref_id[0] == '\0') {
        return core_mesh_compile_invalid_arg("material_ref_id must be non-empty when present");
    }
    return core_result_ok();
}

void core_mesh_compile_authoring_contract_init(CoreMeshCompileAuthoringContract *contract) {
    if (!contract) {
        return;
    }
    memset(contract, 0, sizeof(*contract));
}

CoreResult core_mesh_compile_authoring_contract_prepare(
    CoreMeshCompileAuthoringContract *contract,
    const CoreMeshAssetAuthoringDocument *document,
    const char *runtime_asset_id) {
    CoreResult r;
    if (!contract || !document) {
        return core_mesh_compile_invalid_arg("invalid argument");
    }
    r = core_mesh_asset_authoring_document_validate(document);
    if (r.code != CORE_OK) {
        return r;
    }
    core_mesh_compile_authoring_contract_init(contract);
    r = core_mesh_compile_copy_id(contract->source_asset_id,
                                  sizeof(contract->source_asset_id),
                                  document->contract.asset_id);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_compile_copy_id(contract->runtime_asset_id,
                                  sizeof(contract->runtime_asset_id),
                                  runtime_asset_id);
    if (r.code != CORE_OK) {
        return r;
    }
    contract->source_mode = document->contract.source_mode;
    contract->emits_runtime_mesh_asset = true;
    contract->preserves_surface_group_ids = true;
    contract->requires_imported_mesh_source =
        document->contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH;
    return core_mesh_compile_authoring_contract_validate(contract);
}

CoreResult core_mesh_compile_authoring_contract_validate(
    const CoreMeshCompileAuthoringContract *contract) {
    if (!contract) {
        return core_mesh_compile_invalid_arg("contract is null");
    }
    if (contract->source_asset_id[0] == '\0') {
        return core_mesh_compile_invalid_arg("source_asset_id is required");
    }
    if (contract->runtime_asset_id[0] == '\0') {
        return core_mesh_compile_invalid_arg("runtime_asset_id is required");
    }
    if (contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION &&
        contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED &&
        contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_REVOLVE &&
        contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        return core_mesh_compile_invalid_arg("known source_mode is required");
    }
    if (!contract->emits_runtime_mesh_asset) {
        return core_mesh_compile_invalid_arg("compile contract must emit runtime mesh asset");
    }
    if (contract->source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH &&
        !contract->requires_imported_mesh_source) {
        return core_mesh_compile_invalid_arg(
            "imported_mesh compile contract requires imported mesh source");
    }
    if (contract->source_mode != CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH &&
        contract->requires_imported_mesh_source) {
        return core_mesh_compile_invalid_arg(
            "requires_imported_mesh_source is only valid for imported_mesh");
    }
    return core_result_ok();
}
