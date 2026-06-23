#include "app/app_headless_job_bundle.h"
#include "app_headless_util.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>

static bool resolve_bundle_path(const char *bundle_dir,
                                const char *path,
                                char *out_path,
                                size_t out_path_size) {
    if (!path || !path[0] || !out_path || out_path_size == 0u) return false;
    if (path[0] == '/') {
        return map_forge_headless_copy_string(out_path, out_path_size, path);
    }
    if (!bundle_dir || !bundle_dir[0] || strcmp(bundle_dir, ".") == 0) {
        return map_forge_headless_copy_string(out_path, out_path_size, path);
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
    if (!owner || !key || !out_payload || !map_forge_headless_json_get_object(owner, key, &obj)) return false;
    memset(out_payload, 0, sizeof(*out_payload));
    if (!map_forge_headless_json_get_string_ref(obj, "schema_family", &text) ||
        !map_forge_headless_copy_string(out_payload->schema_family, sizeof(out_payload->schema_family), text)) {
        return false;
    }
    if (!map_forge_headless_json_get_string_ref(obj, "schema_variant", &text) ||
        !map_forge_headless_copy_string(out_payload->schema_variant, sizeof(out_payload->schema_variant), text)) {
        return false;
    }
    if (!map_forge_headless_json_get_string_ref(obj, "path", &text) ||
        !map_forge_headless_copy_string(out_payload->path, sizeof(out_payload->path), text)) {
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

    map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!job_json_path || !job_json_path[0] || !out_bundle) return false;

    memset(&bundle, 0, sizeof(bundle));
    core_headless_job_envelope_init(&bundle.envelope);
    if (!map_forge_headless_copy_string(bundle.bundle_path, sizeof(bundle.bundle_path), job_json_path) ||
        !map_forge_headless_parent_dir(job_json_path, bundle.bundle_dir, sizeof(bundle.bundle_dir))) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to derive bundle directory");
        return false;
    }

    root = json_object_from_file(job_json_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to parse outer job json");
        return false;
    }

    if (!map_forge_headless_json_get_string_ref(root, "schema_family", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.schema_family, sizeof(bundle.envelope.schema_family), text) ||
        !map_forge_headless_json_get_string_ref(root, "schema_variant", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.schema_variant, sizeof(bundle.envelope.schema_variant), text)) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "missing shared job schema identifiers");
        return false;
    }
    if (strcmp(bundle.envelope.schema_family, "codework_job") != 0 ||
        strcmp(bundle.envelope.schema_variant, "headless_bundle_v1") != 0) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "request is not a codework headless bundle");
        return false;
    }
    if (!map_forge_headless_json_get_string_ref(root, "job_id", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.job_id, sizeof(bundle.envelope.job_id), text) ||
        !map_forge_headless_json_get_string_ref(root, "program", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.program, sizeof(bundle.envelope.program), text)) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "missing outer job identity");
        return false;
    }
    if (strcmp(bundle.envelope.program, "map_forge") != 0) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "outer job program must be map_forge");
        return false;
    }

    if (!map_forge_headless_json_get_object(root, "tool", &tool) ||
        !map_forge_headless_json_get_string_ref(tool, "name", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.tool.name, sizeof(bundle.envelope.tool.name), text) ||
        !map_forge_headless_json_get_string_ref(tool, "version", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.tool.version, sizeof(bundle.envelope.tool.version), text) ||
        !map_forge_headless_json_get_string_ref(tool, "target_os", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.tool.target_os, sizeof(bundle.envelope.tool.target_os), text) ||
        !map_forge_headless_json_get_string_ref(tool, "target_arch", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.tool.target_arch, sizeof(bundle.envelope.tool.target_arch), text)) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "missing outer tool fields");
        return false;
    }

    if (!load_payload_ref(root, "scene_payload", &bundle.envelope.scene_payload) ||
        !load_payload_ref(root, "run_config", &bundle.envelope.run_config)) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "missing scene payload or run config reference");
        return false;
    }

    if (!map_forge_headless_json_get_object(root, "outputs", &outputs) ||
        !map_forge_headless_json_get_string_ref(outputs, "root", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.outputs.root, sizeof(bundle.envelope.outputs.root), text) ||
        !map_forge_headless_json_get_string_ref(outputs, "report_path", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.outputs.report_path, sizeof(bundle.envelope.outputs.report_path), text) ||
        !map_forge_headless_json_get_string_ref(outputs, "logs_dir", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.outputs.logs_dir, sizeof(bundle.envelope.outputs.logs_dir), text) ||
        !map_forge_headless_json_get_string_ref(outputs, "artifacts_dir", &text) ||
        !map_forge_headless_copy_string(bundle.envelope.outputs.artifacts_dir, sizeof(bundle.envelope.outputs.artifacts_dir), text)) {
        json_object_put(root);
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "missing outer outputs block");
        return false;
    }

    if (map_forge_headless_json_get_object(root, "metadata", &metadata)) {
        if (map_forge_headless_json_get_string_ref(metadata, "title", &text)) {
            (void)map_forge_headless_copy_string(bundle.envelope.metadata.title,
                              sizeof(bundle.envelope.metadata.title),
                              text);
        }
        if (map_forge_headless_json_get_string_ref(metadata, "description", &text)) {
            (void)map_forge_headless_copy_string(bundle.envelope.metadata.description,
                              sizeof(bundle.envelope.metadata.description),
                              text);
        }
        if (map_forge_headless_json_get_string_ref(metadata, "created_by", &text)) {
            (void)map_forge_headless_copy_string(bundle.envelope.metadata.created_by,
                              sizeof(bundle.envelope.metadata.created_by),
                              text);
        }
        if (map_forge_headless_json_get_string_ref(metadata, "created_at", &text)) {
            (void)map_forge_headless_copy_string(bundle.envelope.metadata.created_at,
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
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to resolve bundle-relative paths");
        return false;
    }

    if (!core_headless_job_envelope_validate(&bundle.envelope)) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "outer job envelope failed validation");
        return false;
    }

    *out_bundle = bundle;
    map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "ok");
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
        map_forge_headless_diag_set(out_error, out_error_size, "ok");
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
        map_forge_headless_diag_set(out_error, out_error_size, job_diag);
        return false;
    }
    if (out_is_shared_bundle) *out_is_shared_bundle = true;
    map_forge_headless_diag_set(out_error, out_error_size, "ok");
    return true;
}

bool map_forge_headless_job_bundle_write(const char *job_json_path,
                                         const CoreHeadlessJobEnvelope *envelope,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    FILE *file = NULL;

    map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!job_json_path || !job_json_path[0] || !envelope) return false;
    if (!core_headless_job_envelope_validate(envelope)) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "shared job envelope failed validation");
        return false;
    }
    if (!map_forge_headless_ensure_parent_dir_mode(job_json_path, 0700)) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to create job.json parent directory");
        return false;
    }

    file = fopen(job_json_path, "wb");
    if (!file) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to open job.json");
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_family\": ");
    map_forge_headless_json_write_string(file, envelope->schema_family);
    fprintf(file, ",\n");
    fprintf(file, "  \"schema_variant\": ");
    map_forge_headless_json_write_string(file, envelope->schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "  \"job_id\": ");
    map_forge_headless_json_write_string(file, envelope->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": ");
    map_forge_headless_json_write_string(file, envelope->program);
    fprintf(file, ",\n");
    fprintf(file, "  \"tool\": {\n");
    fprintf(file, "    \"name\": ");
    map_forge_headless_json_write_string(file, envelope->tool.name);
    fprintf(file, ",\n");
    fprintf(file, "    \"version\": ");
    map_forge_headless_json_write_string(file, envelope->tool.version);
    fprintf(file, ",\n");
    fprintf(file, "    \"target_os\": ");
    map_forge_headless_json_write_string(file, envelope->tool.target_os);
    fprintf(file, ",\n");
    fprintf(file, "    \"target_arch\": ");
    map_forge_headless_json_write_string(file, envelope->tool.target_arch);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"scene_payload\": {\n");
    fprintf(file, "    \"schema_family\": ");
    map_forge_headless_json_write_string(file, envelope->scene_payload.schema_family);
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_variant\": ");
    map_forge_headless_json_write_string(file, envelope->scene_payload.schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "    \"path\": ");
    map_forge_headless_json_write_string(file, envelope->scene_payload.path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"run_config\": {\n");
    fprintf(file, "    \"schema_family\": ");
    map_forge_headless_json_write_string(file, envelope->run_config.schema_family);
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_variant\": ");
    map_forge_headless_json_write_string(file, envelope->run_config.schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "    \"path\": ");
    map_forge_headless_json_write_string(file, envelope->run_config.path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"outputs\": {\n");
    fprintf(file, "    \"root\": ");
    map_forge_headless_json_write_string(file, envelope->outputs.root);
    fprintf(file, ",\n");
    fprintf(file, "    \"report_path\": ");
    map_forge_headless_json_write_string(file, envelope->outputs.report_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"logs_dir\": ");
    map_forge_headless_json_write_string(file, envelope->outputs.logs_dir);
    fprintf(file, ",\n");
    fprintf(file, "    \"artifacts_dir\": ");
    map_forge_headless_json_write_string(file, envelope->outputs.artifacts_dir);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"title\": ");
    map_forge_headless_json_write_string(file, envelope->metadata.title);
    fprintf(file, ",\n");
    fprintf(file, "    \"description\": ");
    map_forge_headless_json_write_string(file, envelope->metadata.description);
    fprintf(file, ",\n");
    fprintf(file, "    \"created_by\": ");
    map_forge_headless_json_write_string(file, envelope->metadata.created_by);
    fprintf(file, ",\n");
    fprintf(file, "    \"created_at\": ");
    map_forge_headless_json_write_string(file, envelope->metadata.created_at);
    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");
    fclose(file);
    map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool map_forge_headless_job_report_write(const char *report_path,
                                         const CoreHeadlessJobReport *report,
                                         const CoreHeadlessJobArtifact *artifacts,
                                         size_t artifact_count,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size) {
    FILE *file = NULL;

    map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!report_path || !report_path[0] || !report) return false;
    if (!core_headless_job_report_validate(report)) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "shared report failed validation");
        return false;
    }
    if (!map_forge_headless_ensure_parent_dir_mode(report_path, 0700)) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to create report parent directory");
        return false;
    }

    file = fopen(report_path, "wb");
    if (!file) {
        map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "failed to open report");
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_family\": ");
    map_forge_headless_json_write_string(file, report->schema_family);
    fprintf(file, ",\n");
    fprintf(file, "  \"schema_variant\": ");
    map_forge_headless_json_write_string(file, report->schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "  \"job_id\": ");
    map_forge_headless_json_write_string(file, report->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": ");
    map_forge_headless_json_write_string(file, report->program);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    map_forge_headless_json_write_string(file, report->state);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    map_forge_headless_json_write_string(file, report->stage);
    fprintf(file, ",\n");
    fprintf(file, "  \"created_at\": ");
    map_forge_headless_json_write_string(file, report->created_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at\": ");
    map_forge_headless_json_write_string(file, report->started_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at\": ");
    map_forge_headless_json_write_string(file, report->updated_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at\": ");
    map_forge_headless_json_write_string(file, report->finished_at);
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
        map_forge_headless_json_write_string(file, artifacts[i].type);
        fprintf(file, ",\n");
        fprintf(file, "      \"path\": ");
        map_forge_headless_json_write_string(file, artifacts[i].path);
        fprintf(file, "\n    }");
    }
    if (artifact_count > 0u) {
        fprintf(file, "\n");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    fclose(file);
    map_forge_headless_diag_set(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}
