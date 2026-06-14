#ifndef CORE_HEADLESS_JOB_H
#define CORE_HEADLESS_JOB_H

#include <stdbool.h>
#include <stddef.h>

#define CORE_HEADLESS_JOB_MAX_ID_LENGTH 95
#define CORE_HEADLESS_JOB_MAX_NAME_LENGTH 63
#define CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH 63
#define CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH 63
#define CORE_HEADLESS_JOB_MAX_PATH_LENGTH 255
#define CORE_HEADLESS_JOB_MAX_TEXT_LENGTH 255
#define CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH 47

typedef struct CoreHeadlessJobToolIdentity {
    char name[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char version[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char target_os[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char target_arch[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
} CoreHeadlessJobToolIdentity;

typedef struct CoreHeadlessJobPayloadRef {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char path[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
} CoreHeadlessJobPayloadRef;

typedef struct CoreHeadlessJobOutputs {
    char root[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
    char report_path[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
    char logs_dir[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
    char artifacts_dir[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
} CoreHeadlessJobOutputs;

typedef struct CoreHeadlessJobMetadata {
    char title[CORE_HEADLESS_JOB_MAX_TEXT_LENGTH + 1];
    char description[CORE_HEADLESS_JOB_MAX_TEXT_LENGTH + 1];
    char created_by[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char created_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
} CoreHeadlessJobMetadata;

typedef struct CoreHeadlessJobEnvelope {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char program[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    CoreHeadlessJobToolIdentity tool;
    CoreHeadlessJobPayloadRef scene_payload;
    CoreHeadlessJobPayloadRef run_config;
    CoreHeadlessJobOutputs outputs;
    CoreHeadlessJobMetadata metadata;
} CoreHeadlessJobEnvelope;

typedef struct CoreHeadlessJobArtifact {
    char type[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char path[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
} CoreHeadlessJobArtifact;

typedef struct CoreHeadlessJobReport {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char program[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char state[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char stage[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char created_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
    char started_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
    char updated_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
    char finished_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
    const CoreHeadlessJobArtifact *artifacts;
    size_t artifact_count;
} CoreHeadlessJobReport;

void core_headless_job_envelope_init(CoreHeadlessJobEnvelope *job);
void core_headless_job_report_init(CoreHeadlessJobReport *report);
void core_headless_job_artifact_init(CoreHeadlessJobArtifact *artifact);

bool core_headless_job_artifact_validate(const CoreHeadlessJobArtifact *artifact);
bool core_headless_job_envelope_validate(const CoreHeadlessJobEnvelope *job);
bool core_headless_job_report_validate(const CoreHeadlessJobReport *report);

#endif
