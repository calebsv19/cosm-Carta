#include "core_mesh_preview.h"

#include "core_io.h"

#include "cjson/cJSON.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CORE_MESH_PREVIEW_SHARP_EDGE_DOT_THRESHOLD 0.8191520443

typedef struct CoreMeshPreviewTempEdge {
    size_t a;
    size_t b;
    CoreObjectVec3 normal;
} CoreMeshPreviewTempEdge;

typedef struct CoreMeshPreviewEdgeKey {
    size_t a;
    size_t b;
} CoreMeshPreviewEdgeKey;

static CoreResult mesh_preview_ok(void) {
    return core_result_ok();
}

static CoreResult mesh_preview_invalid(const char *message) {
    CoreResult r = {CORE_ERR_INVALID_ARG, message};
    return r;
}

static CoreResult mesh_preview_io(const char *message) {
    CoreResult r = {CORE_ERR_IO, message};
    return r;
}

static CoreResult mesh_preview_nomem(const char *message) {
    CoreResult r = {CORE_ERR_OUT_OF_MEMORY, message};
    return r;
}

static bool mesh_preview_text_equals(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

const char *core_mesh_preview_mode_name(CoreMeshPreviewMode mode) {
    switch (mode) {
        case CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1: return "feature_edges_v1";
        case CORE_MESH_PREVIEW_MODE_UNKNOWN:
        default: return "unknown";
    }
}

CoreResult core_mesh_preview_mode_parse(const char *text, CoreMeshPreviewMode *out_mode) {
    if (!out_mode) return mesh_preview_invalid("missing preview mode output");
    *out_mode = CORE_MESH_PREVIEW_MODE_UNKNOWN;
    if (mesh_preview_text_equals(text, "feature_edges_v1")) {
        *out_mode = CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1;
        return mesh_preview_ok();
    }
    return mesh_preview_invalid("unknown mesh preview mode");
}

void core_mesh_preview_runtime_payload_init(CoreMeshPreviewRuntimePayload *payload) {
    if (!payload) return;
    memset(payload, 0, sizeof(*payload));
    payload->mode = CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1;
}

void core_mesh_preview_runtime_payload_free(CoreMeshPreviewRuntimePayload *payload) {
    if (!payload) return;
    free(payload->edges);
    core_mesh_preview_runtime_payload_init(payload);
}

CoreResult core_mesh_preview_runtime_payload_set_edge_count(
    CoreMeshPreviewRuntimePayload *payload,
    size_t edge_count) {
    CoreMeshPreviewEdge *edges = NULL;
    if (!payload) return mesh_preview_invalid("missing mesh preview payload");
    free(payload->edges);
    payload->edges = NULL;
    payload->edge_count = 0u;
    if (edge_count == 0u) return mesh_preview_invalid("mesh preview edge count is zero");
    if (edge_count > SIZE_MAX / sizeof(CoreMeshPreviewEdge)) {
        return mesh_preview_invalid("mesh preview edge count overflows allocation");
    }
    edges = (CoreMeshPreviewEdge *)calloc(edge_count, sizeof(CoreMeshPreviewEdge));
    if (!edges) return mesh_preview_nomem("failed to allocate mesh preview edges");
    payload->edges = edges;
    payload->edge_count = edge_count;
    return mesh_preview_ok();
}

static bool mesh_preview_vec3_finite(CoreObjectVec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

CoreResult core_mesh_preview_runtime_payload_validate(
    const CoreMeshPreviewRuntimePayload *payload) {
    if (!payload) return mesh_preview_invalid("missing mesh preview payload");
    if (!payload->asset_id[0]) return mesh_preview_invalid("mesh preview missing asset_id");
    if (payload->source_vertex_count == 0u || payload->source_triangle_count == 0u) {
        return mesh_preview_invalid("mesh preview source counts are invalid");
    }
    if (payload->mode != CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1) {
        return mesh_preview_invalid("mesh preview mode is invalid");
    }
    if (payload->edge_count == 0u || !payload->edges) {
        return mesh_preview_invalid("mesh preview has no drawable edges");
    }
    if (!mesh_preview_vec3_finite(payload->local_bounds.min) ||
        !mesh_preview_vec3_finite(payload->local_bounds.max) ||
        payload->local_bounds.min.x > payload->local_bounds.max.x ||
        payload->local_bounds.min.y > payload->local_bounds.max.y ||
        payload->local_bounds.min.z > payload->local_bounds.max.z) {
        return mesh_preview_invalid("mesh preview local bounds are invalid");
    }
    for (size_t i = 0u; i < payload->edge_count; ++i) {
        if (!mesh_preview_vec3_finite(payload->edges[i].a) ||
            !mesh_preview_vec3_finite(payload->edges[i].b)) {
            return mesh_preview_invalid("mesh preview edge contains non-finite coordinates");
        }
    }
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_path_from_runtime(const char *runtime_path,
                                               char *out_path,
                                               size_t out_path_size) {
    const char *runtime_suffix = ".runtime.json";
    const char *preview_suffix = ".preview.json";
    size_t runtime_len = 0u;
    size_t suffix_len = strlen(runtime_suffix);
    if (!runtime_path || !runtime_path[0] || !out_path || out_path_size == 0u) {
        return mesh_preview_invalid("missing mesh preview path input");
    }
    runtime_len = strlen(runtime_path);
    if (runtime_len > suffix_len &&
        strcmp(runtime_path + runtime_len - suffix_len, runtime_suffix) == 0) {
        size_t base_len = runtime_len - suffix_len;
        if (base_len + strlen(preview_suffix) + 1u > out_path_size) {
            return mesh_preview_invalid("mesh preview path buffer is too small");
        }
        memcpy(out_path, runtime_path, base_len);
        memcpy(out_path + base_len, preview_suffix, strlen(preview_suffix) + 1u);
        return mesh_preview_ok();
    }
    if (snprintf(out_path, out_path_size, "%s.preview.json", runtime_path) >= (int)out_path_size) {
        return mesh_preview_invalid("mesh preview path buffer is too small");
    }
    return mesh_preview_ok();
}

static CoreObjectVec3 mesh_preview_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static CoreObjectVec3 mesh_preview_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (CoreObjectVec3){
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)};
}

static double mesh_preview_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

static CoreObjectVec3 mesh_preview_normalize(CoreObjectVec3 v) {
    const double len_sq = mesh_preview_dot(v, v);
    if (len_sq <= 0.0 || !isfinite(len_sq)) return (CoreObjectVec3){0.0, 0.0, 0.0};
    {
        const double inv_len = 1.0 / sqrt(len_sq);
        return (CoreObjectVec3){v.x * inv_len, v.y * inv_len, v.z * inv_len};
    }
}

static CoreObjectVec3 mesh_preview_triangle_normal(
    const CoreMeshAssetRuntimeDocument *document,
    const CoreMeshAssetRuntimeTriangle *tri) {
    const CoreObjectVec3 a = document->vertices[tri->a].position;
    const CoreObjectVec3 b = document->vertices[tri->b].position;
    const CoreObjectVec3 c = document->vertices[tri->c].position;
    return mesh_preview_normalize(mesh_preview_cross(mesh_preview_sub(b, a),
                                                     mesh_preview_sub(c, a)));
}

static int mesh_preview_compare_temp_edges(const void *lhs, const void *rhs) {
    const CoreMeshPreviewTempEdge *a = (const CoreMeshPreviewTempEdge *)lhs;
    const CoreMeshPreviewTempEdge *b = (const CoreMeshPreviewTempEdge *)rhs;
    if (a->a < b->a) return -1;
    if (a->a > b->a) return 1;
    if (a->b < b->b) return -1;
    if (a->b > b->b) return 1;
    return 0;
}

static void mesh_preview_add_temp_edge(CoreMeshPreviewTempEdge *edges,
                                       size_t *edge_count,
                                       size_t a,
                                       size_t b,
                                       CoreObjectVec3 normal) {
    CoreMeshPreviewTempEdge *edge = &edges[(*edge_count)++];
    if (a < b) {
        edge->a = a;
        edge->b = b;
    } else {
        edge->a = b;
        edge->b = a;
    }
    edge->normal = normal;
}

static CoreResult mesh_preview_build_feature_edges(
    const CoreMeshAssetRuntimeDocument *document,
    CoreMeshPreviewEdgeKey **out_edges,
    size_t *out_edge_count) {
    CoreMeshPreviewTempEdge *temp_edges = NULL;
    CoreMeshPreviewEdgeKey *candidates = NULL;
    size_t temp_edge_count = 0u;
    size_t candidate_count = 0u;
    if (out_edges) *out_edges = NULL;
    if (out_edge_count) *out_edge_count = 0u;
    if (!document || !out_edges || !out_edge_count) {
        return mesh_preview_invalid("missing mesh preview feature-edge input");
    }
    if (document->triangle_count == 0u ||
        document->triangle_count > SIZE_MAX / (3u * sizeof(CoreMeshPreviewTempEdge))) {
        return mesh_preview_invalid("invalid mesh preview triangle count");
    }
    temp_edges = (CoreMeshPreviewTempEdge *)malloc(document->triangle_count * 3u *
                                                  sizeof(CoreMeshPreviewTempEdge));
    candidates = (CoreMeshPreviewEdgeKey *)malloc(document->triangle_count * 3u *
                                                  sizeof(CoreMeshPreviewEdgeKey));
    if (!temp_edges || !candidates) {
        free(temp_edges);
        free(candidates);
        return mesh_preview_nomem("failed to allocate mesh preview edge set");
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[i];
        const CoreObjectVec3 normal = mesh_preview_triangle_normal(document, tri);
        mesh_preview_add_temp_edge(temp_edges, &temp_edge_count, tri->a, tri->b, normal);
        mesh_preview_add_temp_edge(temp_edges, &temp_edge_count, tri->b, tri->c, normal);
        mesh_preview_add_temp_edge(temp_edges, &temp_edge_count, tri->c, tri->a, normal);
    }
    qsort(temp_edges,
          temp_edge_count,
          sizeof(temp_edges[0]),
          mesh_preview_compare_temp_edges);
    for (size_t i = 0u; i < temp_edge_count;) {
        size_t j = i + 1u;
        bool feature = false;
        while (j < temp_edge_count &&
               temp_edges[j].a == temp_edges[i].a &&
               temp_edges[j].b == temp_edges[i].b) {
            ++j;
        }
        if (j - i != 2u) {
            feature = true;
        } else {
            const double dot = mesh_preview_dot(temp_edges[i].normal, temp_edges[i + 1u].normal);
            feature = !isfinite(dot) || dot <= CORE_MESH_PREVIEW_SHARP_EDGE_DOT_THRESHOLD;
        }
        if (feature) candidates[candidate_count++] = (CoreMeshPreviewEdgeKey){temp_edges[i].a, temp_edges[i].b};
        i = j;
    }
    if (candidate_count == 0u) {
        for (size_t i = 0u; i < temp_edge_count;) {
            size_t j = i + 1u;
            while (j < temp_edge_count &&
                   temp_edges[j].a == temp_edges[i].a &&
                   temp_edges[j].b == temp_edges[i].b) {
                ++j;
            }
            candidates[candidate_count++] = (CoreMeshPreviewEdgeKey){temp_edges[i].a, temp_edges[i].b};
            i = j;
        }
    }
    free(temp_edges);
    *out_edges = candidates;
    *out_edge_count = candidate_count;
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_build_runtime_payload(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    CoreMeshPreviewRuntimePayload *out_payload) {
    CoreMeshPreviewEdgeKey *feature_edges = NULL;
    size_t feature_edge_count = 0u;
    size_t edge_count = 0u;
    size_t stride = 1u;
    CoreResult r;
    if (!document || !out_payload) return mesh_preview_invalid("missing mesh preview build input");
    r = core_mesh_asset_runtime_document_validate(document);
    if (r.code != CORE_OK) return r;
    if (max_edges == 0u) max_edges = CORE_MESH_PREVIEW_DEFAULT_MAX_EDGES;
    r = mesh_preview_build_feature_edges(document, &feature_edges, &feature_edge_count);
    if (r.code != CORE_OK) return r;
    if (feature_edge_count == 0u) {
        free(feature_edges);
        return mesh_preview_invalid("mesh preview produced no feature edges");
    }
    if (feature_edge_count > max_edges) {
        stride = (size_t)ceil((double)feature_edge_count / (double)max_edges);
        if (stride == 0u) stride = 1u;
    }
    for (size_t i = 0u; i < feature_edge_count; i += stride) ++edge_count;

    core_mesh_preview_runtime_payload_free(out_payload);
    snprintf(out_payload->asset_id, sizeof(out_payload->asset_id), "%s", document->contract.asset_id);
    snprintf(out_payload->source_asset_id,
             sizeof(out_payload->source_asset_id),
             "%s",
             document->contract.source_asset_id);
    if (runtime_path) {
        snprintf(out_payload->runtime_path, sizeof(out_payload->runtime_path), "%s", runtime_path);
    }
    out_payload->local_bounds = document->contract.local_bounds;
    out_payload->source_vertex_count = document->vertex_count;
    out_payload->source_triangle_count = document->triangle_count;
    out_payload->source_feature_edge_count = feature_edge_count;
    out_payload->sampled_triangle_count = edge_count;
    out_payload->mode = CORE_MESH_PREVIEW_MODE_FEATURE_EDGES_V1;
    r = core_mesh_preview_runtime_payload_set_edge_count(out_payload, edge_count);
    if (r.code != CORE_OK) {
        free(feature_edges);
        return r;
    }
    edge_count = 0u;
    for (size_t i = 0u; i < feature_edge_count; i += stride) {
        const CoreMeshPreviewEdgeKey edge = feature_edges[i];
        out_payload->edges[edge_count++] =
            (CoreMeshPreviewEdge){document->vertices[edge.a].position,
                                  document->vertices[edge.b].position};
    }
    free(feature_edges);
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

static bool mesh_preview_write_json_string(FILE *f, const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    if (fputc('"', f) == EOF) return false;
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
            case '\\': if (fputs("\\\\", f) == EOF) return false; break;
            case '"': if (fputs("\\\"", f) == EOF) return false; break;
            case '\b': if (fputs("\\b", f) == EOF) return false; break;
            case '\f': if (fputs("\\f", f) == EOF) return false; break;
            case '\n': if (fputs("\\n", f) == EOF) return false; break;
            case '\r': if (fputs("\\r", f) == EOF) return false; break;
            case '\t': if (fputs("\\t", f) == EOF) return false; break;
            default:
                if (ch < 0x20u) {
                    if (fprintf(f, "\\u%04x", (unsigned int)ch) < 0) return false;
                } else if (fputc((int)ch, f) == EOF) {
                    return false;
                }
                break;
        }
    }
    return fputc('"', f) != EOF;
}

static bool mesh_preview_write_vec3(FILE *f, CoreObjectVec3 v) {
    return fprintf(f, "{\"x\":%.17g,\"y\":%.17g,\"z\":%.17g}", v.x, v.y, v.z) >= 0;
}

CoreResult core_mesh_preview_save_file(const CoreMeshPreviewRuntimePayload *payload,
                                       const char *preview_path) {
    FILE *f = NULL;
    CoreResult r = core_mesh_preview_runtime_payload_validate(payload);
    if (r.code != CORE_OK) return r;
    if (!preview_path || !preview_path[0]) return mesh_preview_invalid("missing mesh preview output path");
    f = fopen(preview_path, "wb");
    if (!f) return mesh_preview_io("failed to open mesh preview output file");
    bool ok = fprintf(f, "{\n\t\"schema_family\":\"core_mesh_preview\",\n") >= 0;
    ok = ok && fprintf(f, "\t\"schema_variant\":\"core_mesh_preview_runtime_v1\",\n") >= 0;
    ok = ok && fprintf(f, "\t\"schema_version\":1,\n\t\"asset_id\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->asset_id) &&
         fprintf(f, ",\n\t\"source_asset_id\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->source_asset_id) &&
         fprintf(f, ",\n\t\"runtime_path\":") >= 0 &&
         mesh_preview_write_json_string(f, payload->runtime_path) &&
         fprintf(f,
                 ",\n\t\"source_vertex_count\":%zu,\n\t\"source_triangle_count\":%zu,\n",
                 payload->source_vertex_count,
                 payload->source_triangle_count) >= 0;
    ok = ok && fprintf(f, "\t\"local_bounds\":{\"min\":") >= 0 &&
         mesh_preview_write_vec3(f, payload->local_bounds.min) &&
         fprintf(f, ",\"max\":") >= 0 &&
         mesh_preview_write_vec3(f, payload->local_bounds.max) &&
         fprintf(f,
                 "},\n\t\"preview_mode\":\"%s\",\n\t\"source_feature_edge_count\":%zu,\n\t\"sampled_triangle_count\":%zu,\n\t\"edge_count\":%zu,\n\t\"edges\":[\n",
                 core_mesh_preview_mode_name(payload->mode),
                 payload->source_feature_edge_count,
                 payload->sampled_triangle_count,
                 payload->edge_count) >= 0;
    for (size_t i = 0u; ok && i < payload->edge_count; ++i) {
        ok = fprintf(f, "\t\t{\"a\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->edges[i].a) &&
             fprintf(f, ",\"b\":") >= 0 &&
             mesh_preview_write_vec3(f, payload->edges[i].b) &&
             fprintf(f, "}%s\n", (i + 1u < payload->edge_count) ? "," : "") >= 0;
    }
    ok = ok && fprintf(f, "\t]\n}\n") >= 0;
    if (!ok || fclose(f) != 0) return mesh_preview_io("failed to write mesh preview output file");
    return mesh_preview_ok();
}

static bool mesh_preview_vec3_from_json(const cJSON *node, CoreObjectVec3 *out) {
    const cJSON *x = NULL;
    const cJSON *y = NULL;
    const cJSON *z = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    *out = (CoreObjectVec3){x->valuedouble, y->valuedouble, z->valuedouble};
    return true;
}

static CoreResult mesh_preview_read_text_file(const char *path, char **out_text) {
    CoreBuffer buffer = {0};
    CoreResult r;
    char *text = NULL;
    if (!path || !path[0] || !out_text) return mesh_preview_invalid("missing mesh preview input path");
    *out_text = NULL;
    r = core_io_read_all(path, &buffer);
    if (r.code != CORE_OK || !buffer.data || buffer.size == 0u) {
        return mesh_preview_io("failed to read mesh preview input file");
    }
    text = (char *)malloc(buffer.size + 1u);
    if (!text) {
        core_io_buffer_free(&buffer);
        return mesh_preview_nomem("failed to allocate mesh preview input buffer");
    }
    memcpy(text, buffer.data, buffer.size);
    text[buffer.size] = '\0';
    core_io_buffer_free(&buffer);
    *out_text = text;
    return mesh_preview_ok();
}

CoreResult core_mesh_preview_load_file(const char *preview_path,
                                       CoreMeshPreviewRuntimePayload *out_payload) {
    char *text = NULL;
    cJSON *root = NULL;
    const cJSON *schema = NULL;
    const cJSON *edges = NULL;
    const cJSON *bounds = NULL;
    const cJSON *mode = NULL;
    int edge_array_count = 0;
    size_t edge_count = 0u;
    CoreResult r;
    if (!out_payload) return mesh_preview_invalid("missing mesh preview output payload");
    r = mesh_preview_read_text_file(preview_path, &text);
    if (r.code != CORE_OK) return r;
    root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("failed to parse mesh preview file");
    }
    schema = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    if (!cJSON_IsString(schema) ||
        (!mesh_preview_text_equals(schema->valuestring, "core_mesh_preview_runtime_v1") &&
         !mesh_preview_text_equals(schema->valuestring, "line_drawing_mesh_runtime_preview_v1"))) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has unsupported schema");
    }

    core_mesh_preview_runtime_payload_free(out_payload);
    const cJSON *asset_id = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    const cJSON *source_asset_id = cJSON_GetObjectItemCaseSensitive(root, "source_asset_id");
    const cJSON *runtime_path = cJSON_GetObjectItemCaseSensitive(root, "runtime_path");
    const cJSON *source_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "source_vertex_count");
    const cJSON *source_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "source_triangle_count");
    if (!cJSON_IsNumber(source_vertex_count)) {
        source_vertex_count = cJSON_GetObjectItemCaseSensitive(root, "vertex_count");
    }
    if (!cJSON_IsNumber(source_triangle_count)) {
        source_triangle_count = cJSON_GetObjectItemCaseSensitive(root, "triangle_count");
    }
    const cJSON *source_feature_edge_count =
        cJSON_GetObjectItemCaseSensitive(root, "source_feature_edge_count");
    const cJSON *sampled_triangle_count =
        cJSON_GetObjectItemCaseSensitive(root, "sampled_triangle_count");
    if (!cJSON_IsString(asset_id) || !asset_id->valuestring || !asset_id->valuestring[0] ||
        !cJSON_IsNumber(source_vertex_count) || !cJSON_IsNumber(source_triangle_count)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has invalid metadata");
    }
    snprintf(out_payload->asset_id, sizeof(out_payload->asset_id), "%s", asset_id->valuestring);
    if (cJSON_IsString(source_asset_id) && source_asset_id->valuestring) {
        snprintf(out_payload->source_asset_id,
                 sizeof(out_payload->source_asset_id),
                 "%s",
                 source_asset_id->valuestring);
    }
    if (cJSON_IsString(runtime_path) && runtime_path->valuestring) {
        snprintf(out_payload->runtime_path,
                 sizeof(out_payload->runtime_path),
                 "%s",
                 runtime_path->valuestring);
    }
    out_payload->source_vertex_count = (size_t)source_vertex_count->valuedouble;
    out_payload->source_triangle_count = (size_t)source_triangle_count->valuedouble;
    out_payload->source_feature_edge_count =
        cJSON_IsNumber(source_feature_edge_count) ? (size_t)source_feature_edge_count->valuedouble : 0u;
    out_payload->sampled_triangle_count =
        cJSON_IsNumber(sampled_triangle_count) ? (size_t)sampled_triangle_count->valuedouble : 0u;
    bounds = cJSON_GetObjectItemCaseSensitive(root, "local_bounds");
    if (!cJSON_IsObject(bounds) ||
        !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(bounds, "min"),
                                     &out_payload->local_bounds.min) ||
        !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(bounds, "max"),
                                     &out_payload->local_bounds.max)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has invalid bounds");
    }
    mode = cJSON_GetObjectItemCaseSensitive(root, "preview_mode");
    if (cJSON_IsString(mode)) {
        r = core_mesh_preview_mode_parse(mode->valuestring, &out_payload->mode);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            return r;
        }
    }
    edges = cJSON_GetObjectItemCaseSensitive(root, "edges");
    if (!cJSON_IsArray(edges)) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has no edge array");
    }
    edge_array_count = cJSON_GetArraySize(edges);
    if (edge_array_count <= 0) {
        cJSON_Delete(root);
        return mesh_preview_invalid("mesh preview file has empty edge array");
    }
    r = core_mesh_preview_runtime_payload_set_edge_count(out_payload, (size_t)edge_array_count);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        return r;
    }
    for (int i = 0; i < edge_array_count; ++i) {
        const cJSON *edge = cJSON_GetArrayItem(edges, i);
        CoreMeshPreviewEdge parsed = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
        if (!mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(edge, "a"), &parsed.a) ||
            !mesh_preview_vec3_from_json(cJSON_GetObjectItemCaseSensitive(edge, "b"), &parsed.b)) {
            continue;
        }
        out_payload->edges[edge_count++] = parsed;
    }
    out_payload->edge_count = edge_count;
    if (out_payload->source_feature_edge_count == 0u) out_payload->source_feature_edge_count = edge_count;
    if (out_payload->sampled_triangle_count == 0u) out_payload->sampled_triangle_count = edge_count;
    cJSON_Delete(root);
    return core_mesh_preview_runtime_payload_validate(out_payload);
}

CoreResult core_mesh_preview_save_for_runtime_document(
    const CoreMeshAssetRuntimeDocument *document,
    const char *runtime_path,
    size_t max_edges,
    char *out_preview_path,
    size_t out_preview_path_size) {
    CoreMeshPreviewRuntimePayload payload;
    char preview_path[512];
    CoreResult r;
    core_mesh_preview_runtime_payload_init(&payload);
    r = core_mesh_preview_path_from_runtime(runtime_path, preview_path, sizeof(preview_path));
    if (r.code != CORE_OK) return r;
    r = core_mesh_preview_build_runtime_payload(document, runtime_path, max_edges, &payload);
    if (r.code == CORE_OK) r = core_mesh_preview_save_file(&payload, preview_path);
    core_mesh_preview_runtime_payload_free(&payload);
    if (r.code == CORE_OK && out_preview_path && out_preview_path_size > 0u) {
        snprintf(out_preview_path, out_preview_path_size, "%s", preview_path);
    }
    return r;
}
