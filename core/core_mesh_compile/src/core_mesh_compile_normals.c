#include "core_mesh_compile_normals.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static CoreResult mesh_normals_invalid_arg(const char *message) {
    CoreResult result = {CORE_ERR_INVALID_ARG, message};
    return result;
}

static CoreObjectVec3 mesh_normals_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 result = {a.x - b.x, a.y - b.y, a.z - b.z};
    return result;
}

static double mesh_normals_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 mesh_normals_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return result;
}

static bool mesh_normals_normalize(CoreObjectVec3 value, CoreObjectVec3 *out_value) {
    double length_sq = mesh_normals_dot(value, value);
    double inverse_length;
    if (!out_value || !isfinite(length_sq) || length_sq <= 1e-24) return false;
    inverse_length = 1.0 / sqrt(length_sq);
    out_value->x = value.x * inverse_length;
    out_value->y = value.y * inverse_length;
    out_value->z = value.z * inverse_length;
    return true;
}

static double mesh_normals_corner_angle(CoreObjectVec3 center,
                                        CoreObjectVec3 a,
                                        CoreObjectVec3 b) {
    CoreObjectVec3 edge_a;
    CoreObjectVec3 edge_b;
    double cosine;
    if (!mesh_normals_normalize(mesh_normals_sub(a, center), &edge_a) ||
        !mesh_normals_normalize(mesh_normals_sub(b, center), &edge_b)) {
        return 0.0;
    }
    cosine = mesh_normals_dot(edge_a, edge_b);
    if (cosine < -1.0) cosine = -1.0;
    if (cosine > 1.0) cosine = 1.0;
    return acos(cosine);
}

static CoreResult mesh_normals_generate_smooth(CoreMeshAssetRuntimeDocument *document) {
    CoreObjectVec3 *fallback_normals = NULL;
    size_t i;
    if (!document || !document->vertices || document->vertex_count == 0u ||
        !document->triangles || document->triangle_count == 0u) {
        return mesh_normals_invalid_arg("smooth normal generation input is invalid");
    }
    fallback_normals =
        (CoreObjectVec3 *)core_alloc(document->vertex_count * sizeof(*fallback_normals));
    if (!fallback_normals) {
        return (CoreResult){CORE_ERR_OUT_OF_MEMORY, "out of memory"};
    }
    memset(fallback_normals, 0, document->vertex_count * sizeof(*fallback_normals));
    for (i = 0u; i < document->vertex_count; ++i) {
        document->vertices[i].normal = (CoreObjectVec3){0.0, 0.0, 0.0};
    }
    for (i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[i];
        size_t indices[3] = {triangle->a, triangle->b, triangle->c};
        CoreObjectVec3 points[3] = {
            document->vertices[triangle->a].position,
            document->vertices[triangle->b].position,
            document->vertices[triangle->c].position
        };
        CoreObjectVec3 face_normal;
        size_t corner;
        if (!mesh_normals_normalize(
                mesh_normals_cross(mesh_normals_sub(points[1], points[0]),
                                   mesh_normals_sub(points[2], points[0])),
                &face_normal)) {
            core_free(fallback_normals);
            return mesh_normals_invalid_arg("smooth normal face is degenerate");
        }
        for (corner = 0u; corner < 3u; ++corner) {
            double angle = mesh_normals_corner_angle(points[corner],
                                                     points[(corner + 1u) % 3u],
                                                     points[(corner + 2u) % 3u]);
            CoreObjectVec3 *normal = &document->vertices[indices[corner]].normal;
            CoreObjectVec3 *fallback = &fallback_normals[indices[corner]];
            if (mesh_normals_dot(*fallback, *fallback) <= 1e-24) {
                *fallback = face_normal;
            }
            normal->x += face_normal.x * angle;
            normal->y += face_normal.y * angle;
            normal->z += face_normal.z * angle;
        }
    }
    for (i = 0u; i < document->vertex_count; ++i) {
        CoreObjectVec3 normalized;
        if (!mesh_normals_normalize(document->vertices[i].normal, &normalized)) {
            if (!mesh_normals_normalize(fallback_normals[i], &normalized)) {
                core_free(fallback_normals);
                return mesh_normals_invalid_arg("smooth vertex normal has zero contribution");
            }
        }
        document->vertices[i].normal = normalized;
    }
    core_free(fallback_normals);
    document->vertex_normal_count = document->vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_SMOOTH;
    return core_result_ok();
}

typedef struct MeshNormalsFaceInfo {
    CoreObjectVec3 normal;
    double corner_angles[3];
} MeshNormalsFaceInfo;

static size_t mesh_normals_find_root(size_t *parents, size_t corner) {
    size_t root = corner;
    while (parents[root] != root) root = parents[root];
    while (parents[corner] != corner) {
        size_t next = parents[corner];
        parents[corner] = root;
        corner = next;
    }
    return root;
}

static void mesh_normals_union(size_t *parents, size_t a, size_t b) {
    size_t root_a = mesh_normals_find_root(parents, a);
    size_t root_b = mesh_normals_find_root(parents, b);
    if (root_a == root_b) return;
    if (root_a < root_b) parents[root_b] = root_a;
    else parents[root_a] = root_b;
}

static bool mesh_normals_triangles_share_edge_at_vertex(
    const CoreMeshAssetRuntimeTriangle *a,
    const CoreMeshAssetRuntimeTriangle *b,
    size_t vertex) {
    size_t a_indices[3] = {a->a, a->b, a->c};
    size_t b_indices[3] = {b->a, b->b, b->c};
    size_t i;
    size_t j;
    if (strcmp(a->surface_group_id, b->surface_group_id) != 0) return false;
    for (i = 0u; i < 3u; ++i) {
        if (a_indices[i] == vertex) continue;
        for (j = 0u; j < 3u; ++j) {
            if (b_indices[j] == a_indices[i] && b_indices[j] != vertex) return true;
        }
    }
    return false;
}

static CoreResult mesh_normals_generate_crease_aware(
    CoreMeshAssetRuntimeDocument *document,
    double crease_angle_degrees) {
    const double radians_per_degree = 0.01745329251994329577;
    const size_t corner_count = document ? document->triangle_count * 3u : 0u;
    MeshNormalsFaceInfo *faces = NULL;
    size_t *parents = NULL;
    size_t *first_corner = NULL;
    size_t *next_corner = NULL;
    size_t *corner_vertices = NULL;
    size_t *root_to_vertex = NULL;
    CoreMeshAssetRuntimeVertex *new_vertices = NULL;
    size_t new_vertex_count = 0u;
    double crease_cosine;
    size_t triangle_index;
    size_t corner;

    if (!document || !document->vertices || document->vertex_count == 0u ||
        !document->triangles || document->triangle_count == 0u ||
        !isfinite(crease_angle_degrees) || crease_angle_degrees <= 0.0 ||
        crease_angle_degrees > 180.0 || document->triangle_count > SIZE_MAX / 3u ||
        corner_count > SIZE_MAX / sizeof(CoreMeshAssetRuntimeVertex) ||
        corner_count > SIZE_MAX / sizeof(size_t)) {
        return mesh_normals_invalid_arg("crease-aware normal generation input is invalid");
    }
    faces = (MeshNormalsFaceInfo *)core_alloc(document->triangle_count * sizeof(*faces));
    parents = (size_t *)core_alloc(corner_count * sizeof(*parents));
    first_corner = (size_t *)core_alloc(document->vertex_count * sizeof(*first_corner));
    next_corner = (size_t *)core_alloc(corner_count * sizeof(*next_corner));
    corner_vertices = (size_t *)core_alloc(corner_count * sizeof(*corner_vertices));
    root_to_vertex = (size_t *)core_alloc(corner_count * sizeof(*root_to_vertex));
    new_vertices =
        (CoreMeshAssetRuntimeVertex *)core_alloc(corner_count * sizeof(*new_vertices));
    if (!faces || !parents || !first_corner || !next_corner || !corner_vertices ||
        !root_to_vertex || !new_vertices) {
        core_free(faces);
        core_free(parents);
        core_free(first_corner);
        core_free(next_corner);
        core_free(corner_vertices);
        core_free(root_to_vertex);
        core_free(new_vertices);
        return (CoreResult){CORE_ERR_OUT_OF_MEMORY, "out of memory"};
    }
    for (corner = 0u; corner < document->vertex_count; ++corner) {
        first_corner[corner] = SIZE_MAX;
    }
    for (corner = 0u; corner < corner_count; ++corner) {
        parents[corner] = corner;
        next_corner[corner] = SIZE_MAX;
        root_to_vertex[corner] = SIZE_MAX;
    }

    for (triangle_index = 0u; triangle_index < document->triangle_count; ++triangle_index) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[triangle_index];
        size_t indices[3] = {triangle->a, triangle->b, triangle->c};
        CoreObjectVec3 points[3] = {
            document->vertices[triangle->a].position,
            document->vertices[triangle->b].position,
            document->vertices[triangle->c].position
        };
        if (!mesh_normals_normalize(
                mesh_normals_cross(mesh_normals_sub(points[1], points[0]),
                                   mesh_normals_sub(points[2], points[0])),
                &faces[triangle_index].normal)) {
            goto invalid_face;
        }
        for (corner = 0u; corner < 3u; ++corner) {
            size_t corner_id = triangle_index * 3u + corner;
            corner_vertices[corner_id] = indices[corner];
            faces[triangle_index].corner_angles[corner] =
                mesh_normals_corner_angle(points[corner],
                                          points[(corner + 1u) % 3u],
                                          points[(corner + 2u) % 3u]);
        }
    }

    crease_cosine = cos(crease_angle_degrees * radians_per_degree);
    for (triangle_index = 0u; triangle_index < document->triangle_count; ++triangle_index) {
        const CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[triangle_index];
        for (corner = 0u; corner < 3u; ++corner) {
            size_t corner_id = triangle_index * 3u + corner;
            size_t vertex = corner_vertices[corner_id];
            size_t prior = first_corner[vertex];
            while (prior != SIZE_MAX) {
                size_t prior_triangle = prior / 3u;
                if (mesh_normals_dot(faces[triangle_index].normal,
                                     faces[prior_triangle].normal) >= crease_cosine &&
                    mesh_normals_triangles_share_edge_at_vertex(
                        triangle,
                        &document->triangles[prior_triangle],
                        vertex)) {
                    mesh_normals_union(parents, corner_id, prior);
                }
                prior = next_corner[prior];
            }
            next_corner[corner_id] = first_corner[vertex];
            first_corner[vertex] = corner_id;
        }
    }

    memset(new_vertices, 0, corner_count * sizeof(*new_vertices));
    for (triangle_index = 0u; triangle_index < document->triangle_count; ++triangle_index) {
        CoreMeshAssetRuntimeTriangle *triangle = &document->triangles[triangle_index];
        size_t *indices[3] = {&triangle->a, &triangle->b, &triangle->c};
        for (corner = 0u; corner < 3u; ++corner) {
            size_t corner_id = triangle_index * 3u + corner;
            size_t root = mesh_normals_find_root(parents, corner_id);
            size_t output_vertex = root_to_vertex[root];
            CoreObjectVec3 *normal;
            if (output_vertex == SIZE_MAX) {
                output_vertex = new_vertex_count++;
                root_to_vertex[root] = output_vertex;
                new_vertices[output_vertex].position =
                    document->vertices[corner_vertices[corner_id]].position;
            }
            *indices[corner] = output_vertex;
            normal = &new_vertices[output_vertex].normal;
            normal->x += faces[triangle_index].normal.x *
                         faces[triangle_index].corner_angles[corner];
            normal->y += faces[triangle_index].normal.y *
                         faces[triangle_index].corner_angles[corner];
            normal->z += faces[triangle_index].normal.z *
                         faces[triangle_index].corner_angles[corner];
        }
    }
    for (corner = 0u; corner < new_vertex_count; ++corner) {
        CoreObjectVec3 normalized;
        if (!mesh_normals_normalize(new_vertices[corner].normal, &normalized)) {
            goto invalid_face;
        }
        new_vertices[corner].normal = normalized;
    }
    core_free(document->vertices);
    document->vertices = new_vertices;
    document->vertex_count = new_vertex_count;
    document->contract.vertex_count = new_vertex_count;
    document->vertex_normal_count = new_vertex_count;
    document->normal_provenance =
        CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_GENERATED_CREASE_AWARE;
    core_free(faces);
    core_free(parents);
    core_free(first_corner);
    core_free(next_corner);
    core_free(corner_vertices);
    core_free(root_to_vertex);
    return core_result_ok();

invalid_face:
    core_free(faces);
    core_free(parents);
    core_free(first_corner);
    core_free(next_corner);
    core_free(corner_vertices);
    core_free(root_to_vertex);
    core_free(new_vertices);
    return mesh_normals_invalid_arg("crease-aware normal generation failed");
}

CoreResult core_mesh_compile_runtime_generate_vertex_normals(
    CoreMeshAssetRuntimeDocument *document,
    CoreMeshAssetImportedNormalMode mode,
    double crease_angle_degrees) {
    if (!document) return mesh_normals_invalid_arg("runtime mesh document is null");
    if (mode == CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_NONE) {
        document->vertex_normal_count = 0u;
        document->normal_provenance = CORE_MESH_ASSET_RUNTIME_NORMAL_PROVENANCE_NONE;
        return core_result_ok();
    }
    if (mode == CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_SMOOTH) {
        return mesh_normals_generate_smooth(document);
    }
    if (mode == CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_CREASE_AWARE) {
        return mesh_normals_generate_crease_aware(document, crease_angle_degrees);
    }
    return mesh_normals_invalid_arg("unknown imported normal mode");
}
