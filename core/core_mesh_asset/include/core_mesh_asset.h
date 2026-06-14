#ifndef CORE_MESH_ASSET_H
#define CORE_MESH_ASSET_H

#include <stdbool.h>
#include <stddef.h>

#include "core_base.h"
#include "core_object.h"
#include "core_units.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_MESH_ASSET_SCHEMA_VERSION_1 1

typedef enum CoreMeshAssetSchemaVariant {
    CORE_MESH_ASSET_SCHEMA_VARIANT_UNKNOWN = 0,
    CORE_MESH_ASSET_SCHEMA_VARIANT_AUTHORING_V1 = 1,
    CORE_MESH_ASSET_SCHEMA_VARIANT_RUNTIME_V1 = 2
} CoreMeshAssetSchemaVariant;

typedef enum CoreMeshAssetType {
    CORE_MESH_ASSET_TYPE_UNKNOWN = 0,
    CORE_MESH_ASSET_TYPE_SOLID_MESH = 1
} CoreMeshAssetType;

typedef enum CoreMeshAssetPrimitiveSeedKind {
    CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_UNKNOWN = 0,
    CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE = 1,
    CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM = 2
} CoreMeshAssetPrimitiveSeedKind;

typedef enum CoreMeshAssetSourceMode {
    CORE_MESH_ASSET_SOURCE_MODE_UNKNOWN = 0,
    CORE_MESH_ASSET_SOURCE_MODE_PROFILE_EXTRUSION = 1,
    CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED = 2,
    CORE_MESH_ASSET_SOURCE_MODE_REVOLVE = 3,
    CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH = 4
} CoreMeshAssetSourceMode;

typedef enum CoreMeshAssetImportedMeshSourceFormat {
    CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_UNKNOWN = 0,
    CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL = 1
} CoreMeshAssetImportedMeshSourceFormat;

typedef struct CoreMeshAssetFrame3 {
    CoreObjectVec3 origin;
    CoreObjectVec3 axis_u;
    CoreObjectVec3 axis_v;
    CoreObjectVec3 normal;
} CoreMeshAssetFrame3;

typedef struct CoreMeshAssetBounds3 {
    CoreObjectVec3 min;
    CoreObjectVec3 max;
} CoreMeshAssetBounds3;

typedef struct CoreMeshAssetAuthoringContract {
    char asset_id[64];
    CoreUnitKind unit_kind;
    double world_scale;
    CoreMeshAssetType asset_type;
    CoreMeshAssetSourceMode source_mode;
    CoreMeshAssetFrame3 pivot;
    bool topology_closed_volume_expected;
    bool topology_manifold_expected;
} CoreMeshAssetAuthoringContract;

typedef struct CoreMeshAssetRuntimeContract {
    char asset_id[64];
    char source_asset_id[64];
    CoreMeshAssetType asset_type;
    CoreMeshAssetFrame3 pivot;
    CoreMeshAssetBounds3 local_bounds;
    size_t vertex_count;
    size_t triangle_count;
    bool topology_closed_volume;
    bool topology_manifold_expected;
} CoreMeshAssetRuntimeContract;

typedef struct CoreMeshAssetRuntimeVertex {
    CoreObjectVec3 position;
} CoreMeshAssetRuntimeVertex;

typedef struct CoreMeshAssetRuntimeTriangle {
    size_t a;
    size_t b;
    size_t c;
    char surface_group_id[64];
} CoreMeshAssetRuntimeTriangle;

typedef struct CoreMeshAssetSurfaceGroup {
    char group_id[64];
    size_t triangle_start;
    size_t triangle_count;
} CoreMeshAssetSurfaceGroup;

typedef struct CoreMeshAssetRuntimeDocument {
    CoreMeshAssetRuntimeContract contract;
    size_t vertex_count;
    CoreMeshAssetRuntimeVertex *vertices;
    size_t triangle_count;
    CoreMeshAssetRuntimeTriangle *triangles;
    size_t surface_group_count;
    CoreMeshAssetSurfaceGroup *surface_groups;
} CoreMeshAssetRuntimeDocument;

typedef struct CoreMeshAssetPlanePrimitiveSeed {
    double width;
    double height;
    CoreMeshAssetFrame3 frame;
    bool lock_to_construction_plane;
    bool lock_to_bounds;
} CoreMeshAssetPlanePrimitiveSeed;

typedef struct CoreMeshAssetRectPrismPrimitiveSeed {
    double width;
    double height;
    double depth;
    CoreMeshAssetFrame3 frame;
    bool lock_to_construction_plane;
    bool lock_to_bounds;
} CoreMeshAssetRectPrismPrimitiveSeed;

typedef struct CoreMeshAssetPrimitiveSeed {
    char primitive_id[64];
    CoreMeshAssetPrimitiveSeedKind kind;
    CoreObject object;
    CoreMeshAssetPlanePrimitiveSeed plane;
    CoreMeshAssetRectPrismPrimitiveSeed rect_prism;
} CoreMeshAssetPrimitiveSeed;

typedef struct CoreMeshAssetImportedMeshSource {
    char import_id[64];
    CoreMeshAssetImportedMeshSourceFormat source_format;
    char source_uri[256];
    CoreUnitKind source_unit_kind;
    double source_to_asset_scale;
    char orientation_policy[64];
    char default_surface_group_id[64];
    bool weld_vertices;
    double weld_tolerance;
    bool preserve_source_normals;
    bool topology_closed_volume_observed;
    bool topology_manifold_observed;
} CoreMeshAssetImportedMeshSource;

typedef struct CoreMeshAssetAuthoringDocument {
    CoreMeshAssetAuthoringContract contract;
    size_t primitive_seed_count;
    CoreMeshAssetPrimitiveSeed *primitive_seeds;
    bool has_imported_mesh_source;
    CoreMeshAssetImportedMeshSource imported_mesh_source;
} CoreMeshAssetAuthoringDocument;

const char *core_mesh_asset_schema_variant_name(CoreMeshAssetSchemaVariant variant);
CoreResult core_mesh_asset_schema_variant_parse(const char *text,
                                                CoreMeshAssetSchemaVariant *out_variant);

const char *core_mesh_asset_type_name(CoreMeshAssetType type);
CoreResult core_mesh_asset_type_parse(const char *text, CoreMeshAssetType *out_type);

const char *core_mesh_asset_primitive_seed_kind_name(CoreMeshAssetPrimitiveSeedKind kind);
CoreResult core_mesh_asset_primitive_seed_kind_parse(const char *text,
                                                     CoreMeshAssetPrimitiveSeedKind *out_kind);

const char *core_mesh_asset_source_mode_name(CoreMeshAssetSourceMode mode);
CoreResult core_mesh_asset_source_mode_parse(const char *text, CoreMeshAssetSourceMode *out_mode);

const char *core_mesh_asset_imported_mesh_source_format_name(
    CoreMeshAssetImportedMeshSourceFormat format);
CoreResult core_mesh_asset_imported_mesh_source_format_parse(
    const char *text,
    CoreMeshAssetImportedMeshSourceFormat *out_format);
void core_mesh_asset_imported_mesh_source_init(CoreMeshAssetImportedMeshSource *source);
CoreResult core_mesh_asset_imported_mesh_source_validate(
    const CoreMeshAssetImportedMeshSource *source);

void core_mesh_asset_authoring_contract_init(CoreMeshAssetAuthoringContract *contract);
CoreResult core_mesh_asset_authoring_contract_set_asset_id(CoreMeshAssetAuthoringContract *contract,
                                                           const char *asset_id);
CoreResult core_mesh_asset_authoring_contract_validate(
    const CoreMeshAssetAuthoringContract *contract);

void core_mesh_asset_runtime_contract_init(CoreMeshAssetRuntimeContract *contract);
CoreResult core_mesh_asset_runtime_contract_set_asset_id(CoreMeshAssetRuntimeContract *contract,
                                                         const char *asset_id);
CoreResult core_mesh_asset_runtime_contract_set_source_asset_id(
    CoreMeshAssetRuntimeContract *contract,
    const char *asset_id);
CoreResult core_mesh_asset_runtime_contract_validate(const CoreMeshAssetRuntimeContract *contract);

void core_mesh_asset_runtime_document_init(CoreMeshAssetRuntimeDocument *document);
void core_mesh_asset_runtime_document_free(CoreMeshAssetRuntimeDocument *document);
CoreResult core_mesh_asset_runtime_document_set_vertex_count(
    CoreMeshAssetRuntimeDocument *document,
    size_t vertex_count);
CoreResult core_mesh_asset_runtime_document_set_triangle_count(
    CoreMeshAssetRuntimeDocument *document,
    size_t triangle_count);
CoreResult core_mesh_asset_runtime_document_set_surface_group_count(
    CoreMeshAssetRuntimeDocument *document,
    size_t surface_group_count);
CoreResult core_mesh_asset_runtime_document_validate(
    const CoreMeshAssetRuntimeDocument *document);
CoreResult core_mesh_asset_runtime_document_load_file(const char *path,
                                                      CoreMeshAssetRuntimeDocument *out_document);
CoreResult core_mesh_asset_runtime_document_save_file(
    const CoreMeshAssetRuntimeDocument *document,
    const char *path);

void core_mesh_asset_authoring_document_init(CoreMeshAssetAuthoringDocument *document);
void core_mesh_asset_authoring_document_free(CoreMeshAssetAuthoringDocument *document);
CoreResult core_mesh_asset_authoring_document_set_primitive_seed_count(
    CoreMeshAssetAuthoringDocument *document,
    size_t primitive_seed_count);
CoreResult core_mesh_asset_authoring_document_validate(
    const CoreMeshAssetAuthoringDocument *document);
CoreResult core_mesh_asset_authoring_document_load_file(const char *path,
                                                        CoreMeshAssetAuthoringDocument *out_document);
CoreResult core_mesh_asset_authoring_document_save_file(
    const CoreMeshAssetAuthoringDocument *document,
    const char *path);

#ifdef __cplusplus
}
#endif

#endif
