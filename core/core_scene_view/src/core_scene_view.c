#include "core_scene_view.h"

#include "cjson/cJSON.h"

#include <string.h>

static CoreResult csv_result(CoreError code, const char *message) {
    CoreResult r = { code, message };
    return r;
}

static bool csv_text_equals(const char *lhs, const char *rhs) {
    return lhs && rhs && strcmp(lhs, rhs) == 0;
}

const char *core_scene_view_preview_quality_name(CoreSceneViewPreviewQuality quality) {
    switch (quality) {
        case CORE_SCENE_VIEW_PREVIEW_OUTLINE: return "outline";
        case CORE_SCENE_VIEW_PREVIEW_FLAT_SOLID: return "flat_solid";
        case CORE_SCENE_VIEW_PREVIEW_MATERIAL: return "material_preview";
        case CORE_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY: return "diagnostic_overlay";
        default: return "unknown";
    }
}

CoreResult core_scene_view_preview_quality_parse(const char *text,
                                                 CoreSceneViewPreviewQuality *out_quality) {
    if (out_quality) *out_quality = CORE_SCENE_VIEW_PREVIEW_OUTLINE;
    if (!text || !out_quality) return csv_result(CORE_ERR_INVALID_ARG, "invalid preview quality args");
    if (csv_text_equals(text, "outline")) {
        *out_quality = CORE_SCENE_VIEW_PREVIEW_OUTLINE;
        return core_result_ok();
    }
    if (csv_text_equals(text, "flat_solid")) {
        *out_quality = CORE_SCENE_VIEW_PREVIEW_FLAT_SOLID;
        return core_result_ok();
    }
    if (csv_text_equals(text, "material_preview")) {
        *out_quality = CORE_SCENE_VIEW_PREVIEW_MATERIAL;
        return core_result_ok();
    }
    if (csv_text_equals(text, "diagnostic_overlay")) {
        *out_quality = CORE_SCENE_VIEW_PREVIEW_DIAGNOSTIC_OVERLAY;
        return core_result_ok();
    }
    return csv_result(CORE_ERR_NOT_FOUND, "unknown preview quality");
}

const char *core_scene_view_degraded_reason_name(CoreSceneViewDegradedReason reason) {
    switch (reason) {
        case CORE_SCENE_VIEW_DEGRADED_NONE: return "none";
        case CORE_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE:
            return "no_primitive_seed_state";
        case CORE_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND: return "object_not_found";
        case CORE_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED: return "triangle_cap_reached";
        case CORE_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE:
            return "projection_unavailable";
        default: return "unknown";
    }
}

CoreResult core_scene_view_degraded_reason_parse(const char *text,
                                                 CoreSceneViewDegradedReason *out_reason) {
    if (out_reason) *out_reason = CORE_SCENE_VIEW_DEGRADED_NONE;
    if (!text || !out_reason) return csv_result(CORE_ERR_INVALID_ARG, "invalid degraded reason args");
    if (csv_text_equals(text, "none")) {
        *out_reason = CORE_SCENE_VIEW_DEGRADED_NONE;
        return core_result_ok();
    }
    if (csv_text_equals(text, "no_primitive_seed_state")) {
        *out_reason = CORE_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE;
        return core_result_ok();
    }
    if (csv_text_equals(text, "object_not_found")) {
        *out_reason = CORE_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND;
        return core_result_ok();
    }
    if (csv_text_equals(text, "triangle_cap_reached")) {
        *out_reason = CORE_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED;
        return core_result_ok();
    }
    if (csv_text_equals(text, "projection_unavailable")) {
        *out_reason = CORE_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE;
        return core_result_ok();
    }
    return csv_result(CORE_ERR_NOT_FOUND, "unknown degraded reason");
}

void core_scene_view_packet_readback_init(CoreSceneViewPacketReadback *readback) {
    if (!readback) return;
    memset(readback, 0, sizeof(*readback));
}

void core_scene_view_packet_summary_init(CoreSceneViewPacketSummary *summary) {
    if (!summary) return;
    memset(summary, 0, sizeof(*summary));
    summary->readOnly = true;
    summary->focusedObjectIndex = -1;
    summary->firstFaceGroupIndex = -1;
    summary->lastFaceGroupIndex = -1;
}

CoreResult core_scene_view_packet_summary_from_readback(
    const CoreSceneViewPacketReadback *readback,
    CoreSceneViewPacketSummary *out_summary) {
    if (out_summary) core_scene_view_packet_summary_init(out_summary);
    if (!readback || !out_summary) {
        return csv_result(CORE_ERR_INVALID_ARG, "invalid scene-view summary args");
    }
    if (!readback->valid) {
        return csv_result(CORE_ERR_FORMAT, "invalid scene-view readback summary source");
    }

    out_summary->valid = true;
    out_summary->readOnly = true;
    out_summary->projected = readback->projected;
    out_summary->complete = readback->complete;
    out_summary->focusedObjectIndex = readback->focusedObjectIndex;
    out_summary->triangleCount = readback->triangleCount;
    out_summary->faceGroupCount = readback->faceGroupCount;
    out_summary->firstFaceGroupIndex = readback->firstPickId.faceGroupIndex;
    out_summary->lastFaceGroupIndex = readback->lastPickId.faceGroupIndex;
    out_summary->firstAlpha = readback->firstAlpha;
    out_summary->previewQuality = readback->previewQuality;
    out_summary->degradedReason = readback->degradedReason;
    out_summary->materialPreview =
        readback->previewQuality == CORE_SCENE_VIEW_PREVIEW_MATERIAL;
    out_summary->transparentPreview =
        (readback->firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_TRANSPARENT) != 0u ||
        readback->firstAlpha < 255u;
    out_summary->emissivePreview =
        (readback->firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_EMISSIVE) != 0u;
    out_summary->mirrorPreview =
        (readback->firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_MIRROR) != 0u;
    out_summary->texturedPreview =
        (readback->firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_TEXTURED) != 0u;
    return core_result_ok();
}

static bool csv_get_int(const cJSON *obj, const char *key, int *out_value) {
    const cJSON *value = NULL;
    if (!cJSON_IsObject(obj) || !key || !out_value) return false;
    value = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(value)) return false;
    *out_value = value->valueint;
    return true;
}

static bool csv_get_bool(const cJSON *obj, const char *key, bool *out_value) {
    const cJSON *value = NULL;
    if (!cJSON_IsObject(obj) || !key || !out_value) return false;
    value = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsBool(value)) return false;
    *out_value = cJSON_IsTrue(value);
    return true;
}

static bool csv_read_pick_id(const cJSON *triangle, CoreSceneViewPickId *out_pick_id) {
    const cJSON *pick = NULL;
    if (!cJSON_IsObject(triangle) || !out_pick_id) return false;
    pick = cJSON_GetObjectItemCaseSensitive(triangle, "pick_id");
    return csv_get_int(pick, "scene_object_index", &out_pick_id->sceneObjectIndex) &&
           csv_get_int(pick, "primitive_index", &out_pick_id->primitiveIndex) &&
           csv_get_int(pick, "triangle_index", &out_pick_id->triangleIndex) &&
           csv_get_int(pick, "local_triangle_index", &out_pick_id->localTriangleIndex) &&
           csv_get_int(pick, "face_group_index", &out_pick_id->faceGroupIndex);
}

static bool csv_read_alpha(const cJSON *triangle, unsigned char *out_alpha) {
    const cJSON *rgba = NULL;
    const cJSON *alpha = NULL;
    if (!cJSON_IsObject(triangle) || !out_alpha) return false;
    rgba = cJSON_GetObjectItemCaseSensitive(triangle, "rgba");
    if (!cJSON_IsArray(rgba) || cJSON_GetArraySize(rgba) < 4) return false;
    alpha = cJSON_GetArrayItem(rgba, 3);
    if (!cJSON_IsNumber(alpha) || alpha->valueint < 0 || alpha->valueint > 255) return false;
    *out_alpha = (unsigned char)alpha->valueint;
    return true;
}

static bool csv_validate_schema(const cJSON *root) {
    const cJSON *family = NULL;
    const cJSON *variant = NULL;
    if (!cJSON_IsObject(root)) return false;
    family = cJSON_GetObjectItemCaseSensitive(root, "schema_family");
    variant = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    return cJSON_IsString(family) &&
           cJSON_IsString(variant) &&
           csv_text_equals(family->valuestring, CORE_SCENE_VIEW_SCHEMA_FAMILY) &&
           csv_text_equals(variant->valuestring, CORE_SCENE_VIEW_PACKET_RAY_TRACING_VARIANT);
}

CoreResult core_scene_view_packet_readback_from_json_string(
    const char *json,
    CoreSceneViewPacketReadback *out_readback) {
    cJSON *root = NULL;
    const cJSON *triangles = NULL;
    const cJSON *first = NULL;
    const cJSON *last = NULL;
    const cJSON *quality = NULL;
    const cJSON *reason = NULL;
    int display_flags = 0;
    CoreResult result = core_result_ok();

    if (!json || !out_readback) return csv_result(CORE_ERR_INVALID_ARG, "invalid readback args");
    core_scene_view_packet_readback_init(out_readback);

    root = cJSON_Parse(json);
    if (!csv_validate_schema(root)) {
        result = csv_result(CORE_ERR_FORMAT, "unsupported scene-view packet schema");
        goto cleanup;
    }

    triangles = cJSON_GetObjectItemCaseSensitive(root, "triangles");
    quality = cJSON_GetObjectItemCaseSensitive(root, "preview_quality");
    reason = cJSON_GetObjectItemCaseSensitive(root, "degraded_reason");
    if (!cJSON_IsString(quality) ||
        core_scene_view_preview_quality_parse(quality->valuestring,
                                              &out_readback->previewQuality).code != CORE_OK ||
        !cJSON_IsString(reason) ||
        core_scene_view_degraded_reason_parse(reason->valuestring,
                                              &out_readback->degradedReason).code != CORE_OK ||
        !csv_get_int(root, "focused_object_index", &out_readback->focusedObjectIndex) ||
        !csv_get_bool(root, "projected", &out_readback->projected) ||
        !csv_get_bool(root, "complete", &out_readback->complete) ||
        !csv_get_int(root, "triangle_count", &out_readback->triangleCount) ||
        !csv_get_int(root, "face_group_count", &out_readback->faceGroupCount) ||
        !cJSON_IsArray(triangles)) {
        result = csv_result(CORE_ERR_FORMAT, "invalid scene-view packet metadata");
        goto cleanup;
    }

    if (out_readback->triangleCount <= 0 ||
        out_readback->faceGroupCount <= 0 ||
        cJSON_GetArraySize(triangles) < out_readback->triangleCount) {
        result = csv_result(CORE_ERR_FORMAT, "invalid scene-view triangle counts");
        goto cleanup;
    }

    first = cJSON_GetArrayItem(triangles, 0);
    last = cJSON_GetArrayItem(triangles, out_readback->triangleCount - 1);
    if (!csv_read_pick_id(first, &out_readback->firstPickId) ||
        !csv_read_pick_id(last, &out_readback->lastPickId) ||
        !csv_get_int(first, "display_flags", &display_flags) ||
        display_flags < 0 ||
        !csv_read_alpha(first, &out_readback->firstAlpha)) {
        result = csv_result(CORE_ERR_FORMAT, "invalid scene-view triangle readback");
        goto cleanup;
    }

    out_readback->firstDisplayFlags = (uint32_t)display_flags;
    out_readback->valid = true;

cleanup:
    cJSON_Delete(root);
    if (result.code != CORE_OK) core_scene_view_packet_readback_init(out_readback);
    return result;
}
