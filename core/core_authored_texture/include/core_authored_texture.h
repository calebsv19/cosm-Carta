#ifndef CORE_AUTHORED_TEXTURE_H
#define CORE_AUTHORED_TEXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_AUTHORED_TEXTURE_SCHEMA_V1 1
#define CORE_AUTHORED_TEXTURE_SCHEMA_V2 2
#define CORE_AUTHORED_TEXTURE_SCHEMA_V5 5
#define CORE_AUTHORED_TEXTURE_FACE_CORNER_COUNT 4u
#define CORE_AUTHORED_TEXTURE_FACE_EDGE_COUNT 4u
#define CORE_AUTHORED_TEXTURE_UNKNOWN_ID 255u

typedef enum CoreAuthoredTexturePrimitiveKind {
    CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_NONE = 0,
    CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_PLANE = 1,
    CORE_AUTHORED_TEXTURE_PRIMITIVE_KIND_RECT_PRISM = 2
} CoreAuthoredTexturePrimitiveKind;

typedef enum CoreAuthoredTextureBindingKind {
    CORE_AUTHORED_TEXTURE_BINDING_KIND_NONE = 0,
    CORE_AUTHORED_TEXTURE_BINDING_KIND_SEPARATE_FACES = 1
} CoreAuthoredTextureBindingKind;

typedef enum CoreAuthoredTextureOutputKind {
    CORE_AUTHORED_TEXTURE_OUTPUT_KIND_NONE = 0,
    CORE_AUTHORED_TEXTURE_OUTPUT_KIND_LEGACY_FLATTENED = 1,
    CORE_AUTHORED_TEXTURE_OUTPUT_KIND_FLATTENED_ONLY = 2,
    CORE_AUTHORED_TEXTURE_OUTPUT_KIND_BASE_PLUS_OVERLAY = 3
} CoreAuthoredTextureOutputKind;

typedef enum CoreAuthoredTextureFaceRole {
    CORE_AUTHORED_TEXTURE_FACE_ROLE_NONE = 0,
    CORE_AUTHORED_TEXTURE_FACE_ROLE_FRONT = 1,
    CORE_AUTHORED_TEXTURE_FACE_ROLE_BACK = 2,
    CORE_AUTHORED_TEXTURE_FACE_ROLE_LEFT = 3,
    CORE_AUTHORED_TEXTURE_FACE_ROLE_RIGHT = 4,
    CORE_AUTHORED_TEXTURE_FACE_ROLE_TOP = 5,
    CORE_AUTHORED_TEXTURE_FACE_ROLE_BOTTOM = 6
} CoreAuthoredTextureFaceRole;

typedef enum CoreAuthoredTextureNetLayoutKind {
    CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_NONE = 0,
    CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_PLANE = 1,
    CORE_AUTHORED_TEXTURE_NET_LAYOUT_KIND_RECT_PRISM_CROSS = 2
} CoreAuthoredTextureNetLayoutKind;

typedef enum CoreAuthoredTextureNetOrientation {
    CORE_AUTHORED_TEXTURE_NET_ORIENTATION_NONE = 0,
    CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R0 = 1,
    CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R90 = 2,
    CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R180 = 3,
    CORE_AUTHORED_TEXTURE_NET_ORIENTATION_R270 = 4
} CoreAuthoredTextureNetOrientation;

typedef enum CoreAuthoredTextureNetSlot {
    CORE_AUTHORED_TEXTURE_NET_SLOT_NONE = 0,
    CORE_AUTHORED_TEXTURE_NET_SLOT_FRONT = 1,
    CORE_AUTHORED_TEXTURE_NET_SLOT_BACK = 2,
    CORE_AUTHORED_TEXTURE_NET_SLOT_LEFT = 3,
    CORE_AUTHORED_TEXTURE_NET_SLOT_RIGHT = 4,
    CORE_AUTHORED_TEXTURE_NET_SLOT_TOP = 5,
    CORE_AUTHORED_TEXTURE_NET_SLOT_BOTTOM = 6
} CoreAuthoredTextureNetSlot;

typedef struct CoreAuthoredTextureManifestContract {
    int schema_version;
    CoreAuthoredTextureBindingKind binding_kind;
    CoreAuthoredTexturePrimitiveKind primitive_kind;
    CoreAuthoredTextureOutputKind output_kind;
    bool has_legacy_surfaces;
    bool has_base_surfaces;
    bool has_overlay_surfaces;
} CoreAuthoredTextureManifestContract;

const char* core_authored_texture_primitive_kind_name(
    CoreAuthoredTexturePrimitiveKind kind);
bool core_authored_texture_primitive_kind_parse(const char* text,
                                                CoreAuthoredTexturePrimitiveKind* out_kind);

const char* core_authored_texture_binding_kind_name(
    CoreAuthoredTextureBindingKind kind);
bool core_authored_texture_binding_kind_parse(const char* text,
                                              CoreAuthoredTextureBindingKind* out_kind);

const char* core_authored_texture_output_kind_name(CoreAuthoredTextureOutputKind kind);
bool core_authored_texture_output_kind_parse(const char* text,
                                             CoreAuthoredTextureOutputKind* out_kind);

const char* core_authored_texture_face_role_name(CoreAuthoredTextureFaceRole role);
bool core_authored_texture_face_role_parse(const char* text,
                                           CoreAuthoredTextureFaceRole* out_role);

const char* core_authored_texture_net_layout_kind_name(CoreAuthoredTextureNetLayoutKind kind);
bool core_authored_texture_net_layout_kind_parse(const char* text,
                                                 CoreAuthoredTextureNetLayoutKind* out_kind);

const char* core_authored_texture_net_orientation_name(
    CoreAuthoredTextureNetOrientation orientation);
bool core_authored_texture_net_orientation_parse(
    const char* text,
    CoreAuthoredTextureNetOrientation* out_orientation);

const char* core_authored_texture_net_slot_name(CoreAuthoredTextureNetSlot slot);
bool core_authored_texture_net_slot_parse(const char* text,
                                          CoreAuthoredTextureNetSlot* out_slot);

bool core_authored_texture_schema_version_supported(int schema_version);
size_t core_authored_texture_expected_face_count(
    CoreAuthoredTexturePrimitiveKind primitive_kind);
bool core_authored_texture_face_role_allowed_for_primitive(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    CoreAuthoredTextureFaceRole face_role);
bool core_authored_texture_face_roles_complete(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    const CoreAuthoredTextureFaceRole* face_roles,
    size_t face_role_count);
bool core_authored_texture_manifest_contract_validate(
    const CoreAuthoredTextureManifestContract* contract);
bool core_authored_texture_semantic_net_validate(
    CoreAuthoredTexturePrimitiveKind primitive_kind,
    CoreAuthoredTextureFaceRole face_role,
    CoreAuthoredTextureNetLayoutKind layout_kind,
    CoreAuthoredTextureNetSlot net_slot,
    CoreAuthoredTextureNetOrientation orientation,
    const uint8_t* corner_ids,
    const uint8_t* edge_ids,
    const CoreAuthoredTextureFaceRole* adjacent_face_roles);

#ifdef __cplusplus
}
#endif

#endif
