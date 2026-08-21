#ifndef CORE_HEADLESS_JOB_PLATFORM_H
#define CORE_HEADLESS_JOB_PLATFORM_H

#include "core_headless_job.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CORE_HEADLESS_JOB_MAX_DIGEST_LENGTH 127
#define CORE_HEADLESS_JOB_MAX_MEDIA_TYPE_LENGTH 95
#define CORE_HEADLESS_JOB_MAX_CAPABILITY_LENGTH 95
#define CORE_HEADLESS_JOB_MAX_CONSTRAINT_LENGTH 95
#define CORE_HEADLESS_JOB_MAX_ROLE_LENGTH 63
#define CORE_HEADLESS_JOB_MAX_FAILURE_CODE_LENGTH 63
#define CORE_HEADLESS_JOB_MAX_RECORDS 1024u

#define CORE_HEADLESS_JOB_ENVELOPE_FAMILY "codework_job"
#define CORE_HEADLESS_JOB_ENVELOPE_VARIANT "job_envelope_v1"
#define CORE_HEADLESS_JOB_EVENT_FAMILY "codework_job_event"
#define CORE_HEADLESS_JOB_EVENT_VARIANT "job_event_v1"
#define CORE_HEADLESS_JOB_RESULT_FAMILY "codework_job_result"
#define CORE_HEADLESS_JOB_RESULT_VARIANT "job_result_v1"
#define CORE_HEADLESS_JOB_ARTIFACT_FAMILY "codework_artifact"
#define CORE_HEADLESS_JOB_ARTIFACT_VARIANT "artifact_manifest_v1"
#define CORE_HEADLESS_JOB_WORKFLOW_FAMILY "codework_workflow"
#define CORE_HEADLESS_JOB_WORKFLOW_VARIANT "workflow_manifest_v1"
#define CORE_HEADLESS_JOB_WORKER_FAMILY "codework_worker"
#define CORE_HEADLESS_JOB_WORKER_VARIANT "worker_capabilities_v1"

typedef enum CoreHeadlessJobPlatformState {
    CORE_HEADLESS_JOB_STATE_INVALID = 0,
    CORE_HEADLESS_JOB_STATE_SUBMITTED,
    CORE_HEADLESS_JOB_STATE_VALIDATING,
    CORE_HEADLESS_JOB_STATE_QUEUED,
    CORE_HEADLESS_JOB_STATE_CLAIMED,
    CORE_HEADLESS_JOB_STATE_PREPARING,
    CORE_HEADLESS_JOB_STATE_RUNNING,
    CORE_HEADLESS_JOB_STATE_COLLECTING,
    CORE_HEADLESS_JOB_STATE_COMPLETED,
    CORE_HEADLESS_JOB_STATE_FAILED,
    CORE_HEADLESS_JOB_STATE_CANCELLED
} CoreHeadlessJobPlatformState;

typedef enum CoreHeadlessJobEventKind {
    CORE_HEADLESS_JOB_EVENT_INVALID = 0,
    CORE_HEADLESS_JOB_EVENT_STATE_TRANSITION,
    CORE_HEADLESS_JOB_EVENT_PROGRESS,
    CORE_HEADLESS_JOB_EVENT_DIAGNOSTIC,
    CORE_HEADLESS_JOB_EVENT_CLAIM,
    CORE_HEADLESS_JOB_EVENT_LEASE,
    CORE_HEADLESS_JOB_EVENT_ARTIFACT
} CoreHeadlessJobEventKind;

typedef enum CoreHeadlessJobOutcome {
    CORE_HEADLESS_JOB_OUTCOME_INVALID = 0,
    CORE_HEADLESS_JOB_OUTCOME_COMPLETED,
    CORE_HEADLESS_JOB_OUTCOME_FAILED,
    CORE_HEADLESS_JOB_OUTCOME_CANCELLED
} CoreHeadlessJobOutcome;

typedef struct CoreHeadlessJobIdRef {
    char id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
} CoreHeadlessJobIdRef;

typedef struct CoreHeadlessJobContractRef {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
} CoreHeadlessJobContractRef;

typedef struct CoreHeadlessJobCapabilityRequirement {
    char name[CORE_HEADLESS_JOB_MAX_CAPABILITY_LENGTH + 1];
    char version_constraint[CORE_HEADLESS_JOB_MAX_CONSTRAINT_LENGTH + 1];
} CoreHeadlessJobCapabilityRequirement;

typedef struct CoreHeadlessJobInputArtifact {
    char artifact_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char role[CORE_HEADLESS_JOB_MAX_ROLE_LENGTH + 1];
} CoreHeadlessJobInputArtifact;

typedef struct CoreHeadlessJobResourceHints {
    uint32_t min_cpu_cores;
    uint64_t min_memory_bytes;
    uint32_t min_gpu_count;
    uint64_t max_runtime_seconds;
} CoreHeadlessJobResourceHints;

typedef struct CoreHeadlessJobRetryPolicy {
    uint32_t max_attempts;
} CoreHeadlessJobRetryPolicy;

typedef struct CoreHeadlessJobEnvelopeV1 {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char idempotency_key[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char program[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    CoreHeadlessJobToolIdentity adapter;
    CoreHeadlessJobPayloadRef payload;
    const CoreHeadlessJobInputArtifact *input_artifacts;
    size_t input_artifact_count;
    const CoreHeadlessJobCapabilityRequirement *required_capabilities;
    size_t required_capability_count;
    CoreHeadlessJobContractRef output_contract;
    CoreHeadlessJobResourceHints resources;
    CoreHeadlessJobRetryPolicy retry;
    const CoreHeadlessJobIdRef *parent_jobs;
    size_t parent_job_count;
    char created_by[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char created_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
} CoreHeadlessJobEnvelopeV1;

typedef struct CoreHeadlessJobEventV1 {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char event_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char attempt_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char worker_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char lease_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    uint64_t sequence;
    char occurred_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
    CoreHeadlessJobEventKind kind;
    CoreHeadlessJobPlatformState previous_state;
    CoreHeadlessJobPlatformState state;
    double progress;
    char code[CORE_HEADLESS_JOB_MAX_FAILURE_CODE_LENGTH + 1];
    char message[CORE_HEADLESS_JOB_MAX_TEXT_LENGTH + 1];
} CoreHeadlessJobEventV1;

typedef struct CoreHeadlessJobArtifactConstituent {
    char path[CORE_HEADLESS_JOB_MAX_PATH_LENGTH + 1];
    char digest[CORE_HEADLESS_JOB_MAX_DIGEST_LENGTH + 1];
    uint64_t size_bytes;
} CoreHeadlessJobArtifactConstituent;

typedef struct CoreHeadlessJobArtifactManifestV1 {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char artifact_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char logical_name[CORE_HEADLESS_JOB_MAX_TEXT_LENGTH + 1];
    char type[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char media_type[CORE_HEADLESS_JOB_MAX_MEDIA_TYPE_LENGTH + 1];
    CoreHeadlessJobContractRef data_contract;
    char digest_algorithm[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char digest[CORE_HEADLESS_JOB_MAX_DIGEST_LENGTH + 1];
    uint64_t size_bytes;
    char producer_job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char producer_attempt_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    const CoreHeadlessJobIdRef *parent_artifacts;
    size_t parent_artifact_count;
    const CoreHeadlessJobArtifactConstituent *constituents;
    size_t constituent_count;
} CoreHeadlessJobArtifactManifestV1;

typedef struct CoreHeadlessJobResultV1 {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char attempt_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    CoreHeadlessJobOutcome outcome;
    char finished_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
    CoreHeadlessJobToolIdentity executor;
    const CoreHeadlessJobIdRef *output_artifacts;
    size_t output_artifact_count;
    char failure_code[CORE_HEADLESS_JOB_MAX_FAILURE_CODE_LENGTH + 1];
    char failure_message[CORE_HEADLESS_JOB_MAX_TEXT_LENGTH + 1];
} CoreHeadlessJobResultV1;

typedef struct CoreHeadlessJobArtifactBinding {
    char input_role[CORE_HEADLESS_JOB_MAX_ROLE_LENGTH + 1];
    char source_stage_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char source_artifact_role[CORE_HEADLESS_JOB_MAX_ROLE_LENGTH + 1];
} CoreHeadlessJobArtifactBinding;

typedef struct CoreHeadlessJobWorkflowStageV1 {
    char stage_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char job_template_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    const CoreHeadlessJobIdRef *dependencies;
    size_t dependency_count;
    const CoreHeadlessJobArtifactBinding *bindings;
    size_t binding_count;
} CoreHeadlessJobWorkflowStageV1;

typedef struct CoreHeadlessJobWorkflowManifestV1 {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char workflow_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char idempotency_key[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    const CoreHeadlessJobWorkflowStageV1 *stages;
    size_t stage_count;
    char created_by[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char created_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
} CoreHeadlessJobWorkflowManifestV1;

typedef struct CoreHeadlessJobWorkerAdapter {
    char name[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char version[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char executable_digest[CORE_HEADLESS_JOB_MAX_DIGEST_LENGTH + 1];
} CoreHeadlessJobWorkerAdapter;

typedef struct CoreHeadlessJobWorkerCapability {
    char name[CORE_HEADLESS_JOB_MAX_CAPABILITY_LENGTH + 1];
    char version[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
} CoreHeadlessJobWorkerCapability;

typedef struct CoreHeadlessJobWorkerResources {
    uint32_t cpu_cores;
    uint64_t memory_bytes;
    uint32_t gpu_count;
} CoreHeadlessJobWorkerResources;

typedef struct CoreHeadlessJobWorkerCapabilitiesV1 {
    char schema_family[CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH + 1];
    char schema_variant[CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH + 1];
    char worker_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1];
    char target_os[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    char target_arch[CORE_HEADLESS_JOB_MAX_NAME_LENGTH + 1];
    const CoreHeadlessJobWorkerAdapter *adapters;
    size_t adapter_count;
    const CoreHeadlessJobWorkerCapability *capabilities;
    size_t capability_count;
    CoreHeadlessJobWorkerResources resources;
    char observed_at[CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH + 1];
} CoreHeadlessJobWorkerCapabilitiesV1;

void core_headless_job_envelope_v1_init(CoreHeadlessJobEnvelopeV1 *job);
void core_headless_job_event_v1_init(CoreHeadlessJobEventV1 *event);
void core_headless_job_artifact_manifest_v1_init(CoreHeadlessJobArtifactManifestV1 *artifact);
void core_headless_job_result_v1_init(CoreHeadlessJobResultV1 *result);
void core_headless_job_workflow_manifest_v1_init(CoreHeadlessJobWorkflowManifestV1 *workflow);
void core_headless_job_worker_capabilities_v1_init(CoreHeadlessJobWorkerCapabilitiesV1 *worker);

const char *core_headless_job_state_name(CoreHeadlessJobPlatformState state);
const char *core_headless_job_event_kind_name(CoreHeadlessJobEventKind kind);
const char *core_headless_job_outcome_name(CoreHeadlessJobOutcome outcome);
bool core_headless_job_state_is_terminal(CoreHeadlessJobPlatformState state);
bool core_headless_job_state_transition_is_valid(
    CoreHeadlessJobPlatformState previous_state,
    CoreHeadlessJobPlatformState state
);

bool core_headless_job_envelope_v1_validate(const CoreHeadlessJobEnvelopeV1 *job);
bool core_headless_job_event_v1_validate(const CoreHeadlessJobEventV1 *event);
bool core_headless_job_artifact_manifest_v1_validate(
    const CoreHeadlessJobArtifactManifestV1 *artifact
);
bool core_headless_job_result_v1_validate(const CoreHeadlessJobResultV1 *result);
bool core_headless_job_workflow_manifest_v1_validate(
    const CoreHeadlessJobWorkflowManifestV1 *workflow
);
bool core_headless_job_worker_capabilities_v1_validate(
    const CoreHeadlessJobWorkerCapabilitiesV1 *worker
);

#endif
