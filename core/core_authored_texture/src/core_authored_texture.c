#include "core_authored_texture.h"

#include <string.h>

static bool core_authored_texture_text_equals(const char* a, const char* b) {
    return a && b && strcmp(a, b) == 0;
}

static bool core_authored_texture_u8_values_unique(const uint8_t* values, size_t count) {
    size_t i = 0u;
    size_t j = 0u;
    if (!values) {
        return false;
    }
    for (i = 0u; i < count; ++i) {
        for (j = i + 1u; j < count; ++j) {
            if (values[i] == values[j]) {
                return false;
            }
        }
    }
    return true;
}

static bool core_authored_texture_face_roles_unique(const CoreAuthoredTextureFaceRole* roles,
                                                    size_t count) {
    size_t i = 0u;
    size_t j = 0u;
    if (!roles) {
        return false;
    }
    for (i = 0u; i < count; ++i) {
        for (j = i + 1u; j < count; ++j) {
            if (roles[i] == roles[j]) {
                return false;
            }
        }
    }
    return true;
}

const char* core_authored_texture_primitive_kind_name(
    CoreAuthoredTexturePrimitiveKind kind) {
    switch (kind) {
        case CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE:
            return "PLANE";
        case CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM:
            return "RECT_PRISM";
        case CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_NONE:
        default:
            return "";
    }
}

bool core_authored_texture_primitive_kind_parse(const char* text,
                                                CoreAuthoredTexturePrimitiveKind* out_kind) {
    if (!out_kind) return false;
    *out_kind = CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_NONE;
    if (core_authored_texture_text_equals(text, "PLANE")) {
        *out_kind = CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE;
        return true;
    }
    if (core_authored_texture_text_equals(text, "RECT_PRISM")) {
        *out_kind = CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM;
        return true;
    }
    return false;
}

const char* core_authored_texture_binding_kind_name(
    CoreAuthoredTextureBindingKind kind) {
    switch (kind) {
        case CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES:
            return "SEPARATE_FACES";
        case CORE_AUTHORED_TEXTURE_BINDING_KIND_NONE:
        default:
            return "";
    }
}

bool core_authored_texture_binding_kind_parse(const char* text,
                                              CoreAuthoredTextureBindingKind* out_kind) {
    if (!out_kind) return false;
    *out_kind = CORE_AUTHORED_TEXTURE_BINDING_KIND_NONE;
    if (core_authored_texture_text_equals(text, "SEPARATE_FACES")) {
        *out_kind = CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES;
        return true;
    }
    return false;
}

const char* core_authored_texture_output_kind_name(CoreAuthoredTextureOutputKind kind) {
    switch (kind) {
        case CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED:
            return "LEGACY_FLATTENED";
        case CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY:
            return "FLATTENED_ONLY";
        case CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY:
            return "BASE_PLUS_OVERLAY";
        case CORE_AUTHORED_TEXTURE_OUTPUT_KIND_NONE:
        default:
            return "";
    }
}

bool core_authored_texture_output_kind_parse(const char* text,
                                             CoreAuthoredTextureOutputKind* out_kind) {
    if (!out_kind) return false;
    *out_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_NONE;
    if (core_authored_texture_text_equals(text, "LEGACY_FLATTENED")) {
        *out_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED;
        return true;
    }
    if (core_authored_texture_text_equals(text, "FLATTENED_ONLY")) {
        *out_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY;
        return true;
    }
    if (core_authored_texture_text_equals(text, "BASE_PLUS_OVERLAY")) {
        *out_kind = CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY;
        return true;
    }
    return false;
}

const char* core_authored_texture_face_role_name(CoreAuthoredTextureFaceRole role) {
    switch (role) {
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT:
            return "FRONT";
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK:
            return "BACK";
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT:
            return "LEFT";
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT:
            return "RIGHT";
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP:
            return "TOP";
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM:
            return "BOTTOM";
        case CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE:
            return "NONE";
        default:
            return "";
    }
}

bool core_authored_texture_face_role_parse(const char* text,
                                           CoreAuthoredTextureFaceRole* out_role) {
    if (!out_role) return false;
    *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
    if (core_authored_texture_text_equals(text, "FRONT")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT;
        return true;
    }
    if (core_authored_texture_text_equals(text, "BACK")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK;
        return true;
    }
    if (core_authored_texture_text_equals(text, "LEFT")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT;
        return true;
    }
    if (core_authored_texture_text_equals(text, "RIGHT")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT;
        return true;
    }
    if (core_authored_texture_text_equals(text, "TOP")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP;
        return true;
    }
    if (core_authored_texture_text_equals(text, "BOTTOM")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM;
        return true;
    }
    if (core_authored_texture_text_equals(text, "NONE") ||
        core_authored_texture_text_equals(text, "UNSPECIFIED") ||
        core_authored_texture_text_equals(text, "SURFACE")) {
        *out_role = CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE;
        return true;
    }
    return false;
}

const char* core_authored_texture_net_layout_kind_name(
    CoreAuthoredTextureNetLayoutKind kind) {
    switch (kind) {
        case CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_PLANE:
            return "PLANE";
        case CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS:
            return "PRISM_CROSS";
        case CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_NONE:
            return "NONE";
        default:
            return "";
    }
}

bool core_authored_texture_net_layout_kind_parse(const char* text,
                                                 CoreAuthoredTextureNetLayoutKind* out_kind) {
    if (!out_kind) return false;
    *out_kind = CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_NONE;
    if (core_authored_texture_text_equals(text, "PLANE")) {
        *out_kind = CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_PLANE;
        return true;
    }
    if (core_authored_texture_text_equals(text, "PRISM_CROSS")) {
        *out_kind = CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS;
        return true;
    }
    if (core_authored_texture_text_equals(text, "NONE")) {
        return true;
    }
    return false;
}

const char* core_authored_texture_net_orientation_name(
    CoreAuthoredTextureNetOrientation orientation) {
    switch (orientation) {
        case CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R0:
            return "R0";
        case CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R90:
            return "R90";
        case CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R180:
            return "R180";
        case CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R270:
            return "R270";
        case CORE_AUTHORED_TEXTURE_NET_ORIENTATION_NONE:
            return "NONE";
        default:
            return "";
    }
}

bool core_authored_texture_net_orientation_parse(
    const char* text,
    CoreAuthoredTextureNetOrientation* out_orientation) {
    if (!out_orientation) return false;
    *out_orientation = CORE_AUTHORED_TEXTURE_NET_ORIENTATION_NONE;
    if (core_authored_texture_text_equals(text, "R0")) {
        *out_orientation = CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R0;
        return true;
    }
    if (core_authored_texture_text_equals(text, "R90")) {
        *out_orientation = CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R90;
        return true;
    }
    if (core_authored_texture_text_equals(text, "R180")) {
        *out_orientation = CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R180;
        return true;
    }
    if (core_authored_texture_text_equals(text, "R270")) {
        *out_orientation = CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R270;
        return true;
    }
    if (core_authored_texture_text_equals(text, "NONE")) {
        return true;
    }
    return false;
}

const char* core_authored_texture_net_slot_name(CoreAuthoredTextureNetSlot slot) {
    switch (slot) {
        case CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT:
            return "FRONT";
        case CORE_AUTHORED_TEXTURE_NET_SLOT_BACK:
            return "BACK";
        case CORE_AUTHORED_TEXTURE_NET_SLOT_LEFT:
            return "LEFT";
        case CORE_AUTHORED_TEXTURE_NET_SLOT_RIGHT:
            return "RIGHT";
        case CORE_AUTHORED_TEXTURE_NET_SLOT_TOP:
            return "TOP";
        case CORE_AUTHORED_TEXTURE_NET_SLOT_BOTTOM:
            return "BOTTOM";
        case CORE_AUTHORED_TEXTURE_NET_SLOT_NONE:
            return "NONE";
        default:
            return "";
    }
}

bool core_authored_texture_net_slot_parse(const char* text,
                                          CoreAuthoredTextureNetSlot* out_slot) {
    if (!out_slot) return false;
    *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_NONE;
    if (core_authored_texture_text_equals(text, "FRONT")) {
        *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT;
        return true;
    }
    if (core_authored_texture_text_equals(text, "BACK")) {
        *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_BACK;
        return true;
    }
    if (core_authored_texture_text_equals(text, "LEFT")) {
        *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_LEFT;
        return true;
    }
    if (core_authored_texture_text_equals(text, "RIGHT")) {
        *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_RIGHT;
        return true;
    }
    if (core_authored_texture_text_equals(text, "TOP")) {
        *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_TOP;
        return true;
    }
    if (core_authored_texture_text_equals(text, "BOTTOM")) {
        *out_slot = CORE_AUTHORED_TEXTURE_NET_SLOT_BOTTOM;
        return true;
    }
    if (core_authored_texture_text_equals(text, "NONE")) {
        return true;
    }
    return false;
}

bool core_authored_texture_schema_version_supported(int schema_version) {
    return schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V1 ||
           schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V2 ||
           schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V5;
}

size_t core_authored_texture_expected_face_count(
    CoreAuthoredTexturePrimitiveKind primitive_kind) {
    switch (primitive_kind) {
        case CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE:
            return 1u;
        case CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM:
            return 6u;
        case CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_NONE:
        default:
            return 0u;
    }
}

bool core_authored_texture_face_role_allowed_for_primitive(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    CoreAuthoredTextureFaceRole face_role) {
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE) {
        return face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT;
    }
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) {
        return face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT ||
               face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK ||
               face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT ||
               face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT ||
               face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP ||
               face_role == CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM;
    }
    return false;
}

bool core_authored_texture_face_roles_complete(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    const CoreAuthoredTextureFaceRole* face_roles,
    size_t face_role_count) {
    bool seen_front = false;
    bool seen_back = false;
    bool seen_left = false;
    bool seen_right = false;
    bool seen_top = false;
    bool seen_bottom = false;
    size_t i = 0u;
    if (!face_roles ||
        face_role_count != core_authored_texture_expected_face_count(primitive_kind)) {
        return false;
    }
    for (i = 0u; i < face_role_count; ++i) {
        if (!core_authored_texture_face_role_allowed_for_primitive(primitive_kind, face_roles[i])) {
            return false;
        }
        switch (face_roles[i]) {
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT:
                if (seen_front) return false;
                seen_front = true;
                break;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK:
                if (seen_back) return false;
                seen_back = true;
                break;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT:
                if (seen_left) return false;
                seen_left = true;
                break;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT:
                if (seen_right) return false;
                seen_right = true;
                break;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP:
                if (seen_top) return false;
                seen_top = true;
                break;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM:
                if (seen_bottom) return false;
                seen_bottom = true;
                break;
            case CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE:
            default:
                return false;
        }
    }
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE) {
        return seen_front;
    }
    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) {
        return seen_front && seen_back && seen_left && seen_right && seen_top && seen_bottom;
    }
    return false;
}

bool core_authored_texture_manifest_contract_validate(
    const CoreAuthoredTextureManifestContract* contract) {
    if (!contract) return false;
    if (!core_authored_texture_schema_version_supported(contract->schema_version)) return false;
    if (contract->binding_kind != CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES) return false;
    if (contract->primitive_kind != CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE &&
        contract->primitive_kind != CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) {
        return false;
    }
    if (contract->schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V1 ||
        contract->schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V2) {
        return contract->output_kind == CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED &&
               contract->has_legacy_surfaces &&
               !contract->has_base_surfaces &&
               !contract->has_overlay_surfaces;
    }
    if (contract->schema_version == CORE_AUTHORED_TEXTURE_SCHEMA_V5) {
        if (contract->output_kind == CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY) {
            return contract->has_base_surfaces &&
                   !contract->has_overlay_surfaces &&
                   !contract->has_legacy_surfaces;
        }
        if (contract->output_kind == CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY) {
            return contract->has_base_surfaces && contract->has_overlay_surfaces &&
                   !contract->has_legacy_surfaces;
        }
    }
    return false;
}

bool core_authored_texture_semantic_net_validate(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    CoreAuthoredTextureFaceRole face_role,
    CoreAuthoredTextureNetLayoutKind layout_kind,
    CoreAuthoredTextureNetSlot net_slot,
    CoreAuthoredTextureNetOrientation orientation,
    const uint8_t* corner_ids,
    const uint8_t* edge_ids,
    const CoreAuthoredTextureFaceRole* adjacent_face_roles) {
    size_t i = 0u;
    if (!core_authored_texture_face_role_allowed_for_primitive(primitive_kind, face_role) ||
        !corner_ids || !edge_ids || !adjacent_face_roles ||
        orientation == CORE_AUTHORED_TEXTURE_NET_ORIENTATION_NONE) {
        return false;
    }

    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE) {
        if (layout_kind != CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_PLANE ||
            face_role != CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT ||
            net_slot != CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT) {
            return false;
        }
        for (i = 0u; i < CORE_AUTHORED_TEXTURE_FACE_CORNER_COUNT; ++i) {
            if (corner_ids[i] != CORE_AUTHORED_TEXTURE_UNKNOWN_ID ||
                edge_ids[i] != CORE_AUTHORED_TEXTURE_UNKNOWN_ID ||
                adjacent_face_roles[i] != CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE) {
                return false;
            }
        }
        return true;
    }

    if (primitive_kind == CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM) {
        if (layout_kind != CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS ||
            (CoreAuthoredTextureFaceRole)net_slot != face_role ||
            !core_authored_texture_u8_values_unique(corner_ids,
                                                    CORE_AUTHORED_TEXTURE_FACE_CORNER_COUNT) ||
            !core_authored_texture_u8_values_unique(edge_ids,
                                                    CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT) ||
            !core_authored_texture_face_roles_unique(adjacent_face_roles,
                                                     CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT)) {
            return false;
        }
        for (i = 0u; i < CORE_AUTHORED_TEXTURE_FACE_CORNER_COUNT; ++i) {
            if (corner_ids[i] > 7u ||
                edge_ids[i] > 11u ||
                !core_authored_texture_face_role_allowed_for_primitive(
                    primitive_kind, adjacent_face_roles[i]) ||
                adjacent_face_roles[i] == CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE ||
                adjacent_face_roles[i] == face_role) {
                return false;
            }
        }
        return true;
    }

    return false;
}
