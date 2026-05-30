#ifndef MAPFORGE_APP_APP_HEADLESS_JOB_BUNDLE_H
#define MAPFORGE_APP_APP_HEADLESS_JOB_BUNDLE_H

#include "app/app_headless.h"
#include "core_headless_job.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct MapForgeHeadlessJobBundle {
    CoreHeadlessJobEnvelope envelope;
    char bundle_path[PATH_MAX];
    char bundle_dir[PATH_MAX];
    char resolved_scene_payload_path[PATH_MAX];
    char resolved_run_config_path[PATH_MAX];
    char resolved_report_path[PATH_MAX];
    char resolved_logs_dir[PATH_MAX];
    char resolved_artifacts_dir[PATH_MAX];
} MapForgeHeadlessJobBundle;

bool map_forge_headless_job_bundle_load(const char *job_json_path,
                                        MapForgeHeadlessJobBundle *out_bundle,
                                        char *out_diagnostics,
                                        size_t out_diagnostics_size);
bool map_forge_headless_job_load_for_run(const char *job_path,
                                         MapForgeHeadlessJob *out_job,
                                         MapForgeHeadlessJobBundle *out_bundle,
                                         bool *out_is_shared_bundle,
                                         char *out_error,
                                         size_t out_error_size);
bool map_forge_headless_job_bundle_write(const char *job_json_path,
                                         const CoreHeadlessJobEnvelope *envelope,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size);
bool map_forge_headless_job_report_write(const char *report_path,
                                         const CoreHeadlessJobReport *report,
                                         const CoreHeadlessJobArtifact *artifacts,
                                         size_t artifact_count,
                                         char *out_diagnostics,
                                         size_t out_diagnostics_size);

#endif
