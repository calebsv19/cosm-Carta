#include "core_headless_job.h"

#include <string.h>

static size_t bounded_strlen(const char *s, size_t max_len) {
    size_t i = 0u;

    if (!s) {
        return 0u;
    }
    while (i < max_len && s[i] != '\0') {
        i += 1u;
    }
    return i;
}

static bool bounded_nonempty_string(const char *s, size_t max_len) {
    size_t len;

    if (!s) {
        return false;
    }
    len = bounded_strlen(s, max_len + 1u);
    return len > 0u && len <= max_len;
}

static bool payload_ref_validate(const CoreHeadlessJobPayloadRef *payload) {
    if (!payload) {
        return false;
    }
    return bounded_nonempty_string(payload->schema_family, CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH) &&
           bounded_nonempty_string(payload->schema_variant, CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH) &&
           bounded_nonempty_string(payload->path, CORE_HEADLESS_JOB_MAX_PATH_LENGTH);
}

static bool outputs_validate(const CoreHeadlessJobOutputs *outputs) {
    if (!outputs) {
        return false;
    }
    return bounded_nonempty_string(outputs->root, CORE_HEADLESS_JOB_MAX_PATH_LENGTH) &&
           bounded_nonempty_string(outputs->report_path, CORE_HEADLESS_JOB_MAX_PATH_LENGTH) &&
           bounded_nonempty_string(outputs->logs_dir, CORE_HEADLESS_JOB_MAX_PATH_LENGTH) &&
           bounded_nonempty_string(outputs->artifacts_dir, CORE_HEADLESS_JOB_MAX_PATH_LENGTH);
}

void core_headless_job_envelope_init(CoreHeadlessJobEnvelope *job) {
    if (!job) {
        return;
    }

    memset(job, 0, sizeof(*job));
    memcpy(job->schema_family, "codework_job", sizeof("codework_job"));
    memcpy(job->schema_variant, "headless_bundle_v1", sizeof("headless_bundle_v1"));
}

void core_headless_job_report_init(CoreHeadlessJobReport *report) {
    if (!report) {
        return;
    }

    memset(report, 0, sizeof(*report));
    memcpy(report->schema_family, "codework_job_report", sizeof("codework_job_report"));
    memcpy(report->schema_variant, "headless_report_v1", sizeof("headless_report_v1"));
}

void core_headless_job_artifact_init(CoreHeadlessJobArtifact *artifact) {
    if (!artifact) {
        return;
    }
    memset(artifact, 0, sizeof(*artifact));
}

bool core_headless_job_artifact_validate(const CoreHeadlessJobArtifact *artifact) {
    if (!artifact) {
        return false;
    }
    return bounded_nonempty_string(artifact->type, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           bounded_nonempty_string(artifact->path, CORE_HEADLESS_JOB_MAX_PATH_LENGTH);
}

bool core_headless_job_envelope_validate(const CoreHeadlessJobEnvelope *job) {
    if (!job) {
        return false;
    }

    return bounded_nonempty_string(job->schema_family, CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH) &&
           bounded_nonempty_string(job->schema_variant, CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH) &&
           bounded_nonempty_string(job->job_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) &&
           bounded_nonempty_string(job->program, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           bounded_nonempty_string(job->tool.name, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           bounded_nonempty_string(job->tool.version, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           bounded_nonempty_string(job->tool.target_os, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           bounded_nonempty_string(job->tool.target_arch, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           payload_ref_validate(&job->scene_payload) &&
           payload_ref_validate(&job->run_config) &&
           outputs_validate(&job->outputs) &&
           bounded_nonempty_string(job->metadata.created_by, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           bounded_nonempty_string(job->metadata.created_at, CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH);
}

bool core_headless_job_report_validate(const CoreHeadlessJobReport *report) {
    size_t i;

    if (!report) {
        return false;
    }

    if (!bounded_nonempty_string(report->schema_family, CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH) ||
        !bounded_nonempty_string(report->schema_variant, CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH) ||
        !bounded_nonempty_string(report->job_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !bounded_nonempty_string(report->program, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !bounded_nonempty_string(report->state, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !bounded_nonempty_string(report->stage, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !bounded_nonempty_string(report->created_at, CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH) ||
        !bounded_nonempty_string(report->updated_at, CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH)) {
        return false;
    }

    if (report->artifact_count > 0u && !report->artifacts) {
        return false;
    }

    for (i = 0u; i < report->artifact_count; ++i) {
        if (!core_headless_job_artifact_validate(&report->artifacts[i])) {
            return false;
        }
    }

    return true;
}
