#ifndef CORE_MESH_COMPILE_NORMALS_H
#define CORE_MESH_COMPILE_NORMALS_H

#include "core_mesh_asset.h"

CoreResult core_mesh_compile_runtime_generate_vertex_normals(
    CoreMeshAssetRuntimeDocument *document,
    CoreMeshAssetImportedNormalMode mode,
    double crease_angle_degrees);

#endif
