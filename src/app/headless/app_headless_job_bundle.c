#include "app/app_headless_job_bundle.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void diag_set(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) return;
    snprintf(out, out_size, "%s", message);
}

static bool copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

static bool parent_dir_of(const char *path, char *out_dir, size_t out_dir_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return copy_string(out_dir, out_dir_size, ".");
    len = (size_t)(slash - path);
    if (len == 0u) return copy_string(out_dir, out_dir_size, "/");
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return true;
}

static bool ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;

    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && access(tmp, F_OK) != 0) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && access(tmp, F_OK) != 0) {
        return false;
    }
    return true;
}

static bool ensure_parent_directory_exists(const char *path) {
    char dir[PATH_MAX];
    if (!parent_dir_of(path, dir, sizeof(dir))) return false;
    return ensure_directory_exists(dir);
}

static void json_write_string(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

static bool json_get_object(json_object *owner, const char *key, json_object **out_obj) {
    json_object *obj = NULL;
    if (out_obj) *out_obj = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_object)) {
        return false;
    }
    if (out_obj) *out_obj = obj;
    return true;
}

static bool json_get_string(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) *out_value = NULL;
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) *out_value = json_object_get_string(obj);
    return true;
}

static bool resolve_bundle_path(const char *bundle_dir,
                                const char *path,
                                char *out_path,
                                size_t out_path_size) {
    if (!path || !path[0] || !out_path || out_path_size == 0u) return false;
    if (path[0] == '/') {
        return copy_string(out_path, out_path_size, path);
    }
    if (!bundle_dir || !bundle_dir[0] || strcmp(bundle_dir, ".") == 0) {
        return copy_string(out_path, out_path_size, path);
    }
    if (snprintf(out_path, out_path_size, "%s/%s", bundle_dir, path) >= (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

static bool load_payload_ref(json_object *owner,
                             const char *key,
                             CoreHeadlessJobPayloadRef *out_payload) {
    json_object *obj = NULL;
    const char *text = NULL;
    if (!owner || !key || !out_payload || !json_get_object(owner, key, &obj)) return false;
    memset(out_payload, 0, sizeof(*out_payload));
    if (!json_get_string(obj, "schema_family", &text) ||
        !copy_string(out_payload->schema_family, sizeof(out_payload->schema_family), text)) {
        return false;
    }
    if (!json_get_string(obj, "schema_variant", &text) ||
        !copy_string(out_payload->schema_variant, sizeof(out_payload->schema_variant), text)) {
        return false;
    }
    if (!json_get_string(obj, "path", &text) ||
        !copy_string(out_payload->path, sizeof(out_payload->path), text)) {
        return false;
    }
    return true;
}

bool map_forge_headless_job_bundle_load(const char *job_json_path,
                                        MapForgeHeadlessJobBundle *out_bundle,
                                        char *out_diagnostics,
                                        size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *tool = NULL;
    json_object *outputs = NULL;
    json_object *metadata = NULL;
    const char *text = NULL;
    MapForgeHeadlessJobBundle bundle;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!job_json_path || !job_json_path[0] || !out_bundle) return false;

    memset(&bundle, 0, sizeof(bundle));
    core_headless_job_envelope_init(&bundle.envelope);
    if (!copy_string(bundle.bundle_path, sizeof(bundle.bundle_path), job_json_path) ||
        !parent_dir_of(job_json_path, bundle.bundle_dir, sizeof(bundle.bundle_dir))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to derive bundle directory");
        return false;
    }

    root = json_object_from_file(job_json_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "failed to parse outer job json");
        return false;
    }

    if (!json_get_string(root, "schema_family", &text) ||
        !copy_string(bundle.envelope.schema_family, sizeof(bundle.envelope.schema_family), text) ||
        !json_get_string(root, "schema_variant", &text) ||
        !copy_string(bundle.envelope.schema_variant, sizeof(bundle.envelope.schema_variant), text)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "missing shared job schema identifiers");
        return false;
    }
    if (strcmp(bundle.envelope.schema_family, "codework_job") != 0 ||
        strcmp(bundle.envelope.schema_variant, "headless_bundle_v1") != 0) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "request is not a codework headless bundle");
        return false;
    }
    if (!json_get_string(root, "job_id", &text) ||
        !copy_string(bundle.envelope.job_id, sizeof(bundle.envelope.job_id), text) ||
        !json_get_string(root, "program", &text) ||
        !copy_string(bundle.envelope.program, sizeof(bundle.envelope.program), text)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "missing outer job identity");
        return false;
    }
    if (strcmp(bundle.envelope.program, "map_forge") != 0) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "outer job program must be map_forge");
        return false;
    }

    if (!json_get_object(root, "tool", &tool) ||
        !json_get_string(tool, "name", &text) ||
        !copy_string(bundle.envelope.tool.name, sizeof(bundle.envelope.tool.name), text) ||
        !json_get_string(tool, "version", &text) ||
        !copy_string(bundle.envelope.tool.version, sizeof(bundle.envelope.tool.version), text) ||
        !json_get_string(tool, "target_os", &text) ||
        !copy_string(bundle.envelope.tool.target_os, sizeof(bundle.envelope.tool.target_os), text) ||
        !json_get_string(tool, "target_arch", &text) ||
        !copy_string(bundle.envelope.tool.target_arch, sizeof(bundle.envelope.tool.target_arch), text)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "missing outer tool fields");
        return false;
    }

    if (!load_payload_ref(root, "scene_payload", &bundle.envelope.scene_payload) ||
        !load_payload_ref(root, "run_config", &bundle.envelope.run_config)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "missing scene payload or run config reference");
        return false;
    }

    if (!json_get_object(root, "outputs", &outputs) ||
        !json_get_string(outputs, "root", &text) ||
        !copy_string(bundle.envelope.outputs.root, sizeof(bundle.envelope.outputs.root), text) ||
        !json_get_string(outputs, "report_path", &text) ||
        !copy_string(bundle.envelope.outputs.report_path, sizeof(bundle.envelope.outputs.report_path), text) ||
        !json_get_string(outputs, "logs_dir", &text) ||
        !copy_string(bundle.envelope.outputs.logs_dir, sizeof(bundle.envelope.outputs.logs_dir), text) ||
        !json_get_string(outputs, "artifacts_dir", &text) ||
        !copy_string(bundle.envelope.outputs.artifacts_dir, sizeof(bundle.envelope.outputs.artifacts_dir), text)) {
        json_object_put(root);
        diag_set(out_diagnostics, out_diagnostics_size, "missing outer outputs block");
        return false;
    }

    if (json_get_object(root, "metadata", &metadata)) {
        if (json_get_string(metadata, "title", &text)) {
            (void)copy_string(bundle.envelope.metadata.title,
                              sizeof(bundle.envelope.metadata.title),
                              text);
        }
        if (json_get_string(metadata, "description", &text)) {
            (void)copy_string(bundle.envelope.metadata.description,
                              sizeof(bundle.envelope.metadata.description),
                              text);
        }
        if (json_get_string(metadata, "created_by", &text)) {
            (void)copy_string(bundle.envelope.metadata.created_by,
                              sizeof(bundle.envelope.metadata.created_by),
                              text);
        }
        if (json_get_string(metadata, "created_at", &text)) {
            (void)copy_string(bundle.envelope.metadata.created_at,
                              sizeof(bundle.envelope.metadata.created_at),
                              text);
        }
    }

    json_object_put(root);

    if (!resolve_bundle_path(bundle.bundle_dir,
                             bundle.envelope.scene_payload.path,
                             bundle.resolved_scene_payload_path,
                             sizeof(bundle.resolved_scene_payload_path)) ||
        !resolve_bundle_path(bundle.bundle_dir,
                             bundle.envelope.run_config.path,
                             bundle.resolved_run_config_path,
                             sizeof(bundle.resolved_run_config_path)) ||
        !resolve_bundle_path(bundle.bundle_dir,
                             bundle.envelope.outputs.report_path,
                             bundle.resolved_report_path,
                             sizeof(bundle.resolved_report_path)) ||
        !resolve_bundle_path(bundle.bundle_dir,
                             bundle.envelope.outputs.logs_dir,
                             bundle.resolved_logs_dir,
                             sizeof(bundle.resolved_logs_dir)) ||
        !resolve_bundle_path(bundle.bundle_dir,
                             bundle.envelope.outputs.artifacts_dir,
                             bundle.resolved_artifacts_dir,
                             sizeof(bundle.resolved_artifacts_dir))) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve bundle-relative paths");
        return false;
    }

    if (!core_headless_job_envelope_validate(&bundle.envelope)) {
        diag_set(out_diagnostics, out_diagnostics_size, "outer job envelope failed validation");
        return false;
    }

    *out_bundle = bundle;
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool map_forge_headless_job_load_for_run(const char *job_path,
                                         MapForgeHeadlessJob *out_job,
                                         MapForgeHeadlessJobBundle *out_bundle,
                                         bool *out_is_shared_bundle,
                                         char *out_error,
                                         size_t out_error_size) {
    char job_diag[256];
    char bundle_diag[256];

    if (out_is_shared_bundle) *out_is_shared_bundle = false;
    if (out_bundle) memset(out_bundle, 0, sizeof(*out_bundle));

    if (map_forge_headless_job_load(job_path, out_job, job_diag, sizeof(job_diag))) {
        diag_set(out_error, out_error_size, "ok");
        return true;
    }

    if (!map_forge_headless_job_bundle_load(job_path,
                                            out_bundle,
                                            bundle_diag,
                                            sizeof(bundle_diag))) {
        if (out_error && out_error_size > 0u) {
            snprintf(out_error,
                     out_error_size,
                     "job load failed (%s); outer bundle load failed (%s)",
                     job_diag,
                     bundle_diag);
        }
        return false;
    }

    if (!map_forge_headless_job_load(out_bundle->resolved_scene_payload_path,
                                     out_job,
                                     job_diag,
                                     sizeof(job_diag))) {
        diag_set(out_error, out_error_size, job_diag);
        return false;
    }
    if (out_is_shared_bundle) *out_is_shared_bundle = true;
    diag_set(out_error, out_error_size, "ok");
    return true;
}

bool map_forge_headless_job_bundle_write(const char *job_json_path,
                                         const CoreHeadlessJobEnvelope *envelope,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    FILE *file = NULL;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!job_json_path || !job_json_path[0] || !envelope) return false;
    if (!core_headless_job_envelope_validate(envelope)) {
        diag_set(out_diagnostics, out_diagnostics_size, "shared job envelope failed validation");
        return false;
    }
    if (!ensure_parent_directory_exists(job_json_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to create job.json parent directory");
        return false;
    }

    file = fopen(job_json_path, "wb");
    if (!file) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to open job.json");
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_family\": ");
    json_write_string(file, envelope->schema_family);
    fprintf(file, ",\n");
    fprintf(file, "  \"schema_variant\": ");
    json_write_string(file, envelope->schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "  \"job_id\": ");
    json_write_string(file, envelope->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": ");
    json_write_string(file, envelope->program);
    fprintf(file, ",\n");
    fprintf(file, "  \"tool\": {\n");
    fprintf(file, "    \"name\": ");
    json_write_string(file, envelope->tool.name);
    fprintf(file, ",\n");
    fprintf(file, "    \"version\": ");
    json_write_string(file, envelope->tool.version);
    fprintf(file, ",\n");
    fprintf(file, "    \"target_os\": ");
    json_write_string(file, envelope->tool.target_os);
    fprintf(file, ",\n");
    fprintf(file, "    \"target_arch\": ");
    json_write_string(file, envelope->tool.target_arch);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"scene_payload\": {\n");
    fprintf(file, "    \"schema_family\": ");
    json_write_string(file, envelope->scene_payload.schema_family);
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_variant\": ");
    json_write_string(file, envelope->scene_payload.schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "    \"path\": ");
    json_write_string(file, envelope->scene_payload.path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"run_config\": {\n");
    fprintf(file, "    \"schema_family\": ");
    json_write_string(file, envelope->run_config.schema_family);
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_variant\": ");
    json_write_string(file, envelope->run_config.schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "    \"path\": ");
    json_write_string(file, envelope->run_config.path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"outputs\": {\n");
    fprintf(file, "    \"root\": ");
    json_write_string(file, envelope->outputs.root);
    fprintf(file, ",\n");
    fprintf(file, "    \"report_path\": ");
    json_write_string(file, envelope->outputs.report_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"logs_dir\": ");
    json_write_string(file, envelope->outputs.logs_dir);
    fprintf(file, ",\n");
    fprintf(file, "    \"artifacts_dir\": ");
    json_write_string(file, envelope->outputs.artifacts_dir);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"title\": ");
    json_write_string(file, envelope->metadata.title);
    fprintf(file, ",\n");
    fprintf(file, "    \"description\": ");
    json_write_string(file, envelope->metadata.description);
    fprintf(file, ",\n");
    fprintf(file, "    \"created_by\": ");
    json_write_string(file, envelope->metadata.created_by);
    fprintf(file, ",\n");
    fprintf(file, "    \"created_at\": ");
    json_write_string(file, envelope->metadata.created_at);
    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");
    fclose(file);
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool map_forge_headless_job_report_write(const char *report_path,
                                         const CoreHeadlessJobReport *report,
                                         const CoreHeadlessJobArtifact *artifacts,
                                         size_t artifact_count,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    FILE *file = NULL;

    diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!report_path || !report_path[0] || !report) return false;
    if (!core_headless_job_report_validate(report)) {
        diag_set(out_diagnostics, out_diagnostics_size, "shared report failed validation");
        return false;
    }
    if (!ensure_parent_directory_exists(report_path)) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to create report parent directory");
        return false;
    }

    file = fopen(report_path, "wb");
    if (!file) {
        diag_set(out_diagnostics, out_diagnostics_size, "failed to open report");
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_family\": ");
    json_write_string(file, report->schema_family);
    fprintf(file, ",\n");
    fprintf(file, "  \"schema_variant\": ");
    json_write_string(file, report->schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "  \"job_id\": ");
    json_write_string(file, report->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": ");
    json_write_string(file, report->program);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    json_write_string(file, report->state);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    json_write_string(file, report->stage);
    fprintf(file, ",\n");
    fprintf(file, "  \"created_at\": ");
    json_write_string(file, report->created_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at\": ");
    json_write_string(file, report->started_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at\": ");
    json_write_string(file, report->updated_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at\": ");
    json_write_string(file, report->finished_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"artifacts\": [");
    for (size_t i = 0u; i < artifact_count; ++i) {
        if (i == 0u) {
            fprintf(file, "\n");
        } else {
            fprintf(file, ",\n");
        }
        fprintf(file, "    {\n");
        fprintf(file, "      \"type\": ");
        json_write_string(file, artifacts[i].type);
        fprintf(file, ",\n");
        fprintf(file, "      \"path\": ");
        json_write_string(file, artifacts[i].path);
        fprintf(file, "\n    }");
    }
    if (artifact_count > 0u) {
        fprintf(file, "\n");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    fclose(file);
    diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
