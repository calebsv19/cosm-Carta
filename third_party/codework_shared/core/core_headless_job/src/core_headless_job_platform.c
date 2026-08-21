#include "core_headless_job_platform.h"

#include <math.h>
#include <ctype.h>
#include <string.h>

static size_t bounded_strlen(const char *text, size_t max_length) {
    size_t length = 0u;

    if (!text) {
        return 0u;
    }
    while (length < max_length && text[length] != '\0') {
        length += 1u;
    }
    return length;
}

static bool required_string(const char *text, size_t max_length) {
    size_t length;

    if (!text) {
        return false;
    }
    length = bounded_strlen(text, max_length + 1u);
    return length > 0u && length <= max_length;
}

static bool optional_string(const char *text, size_t max_length) {
    size_t length;

    if (!text) {
        return false;
    }
    length = bounded_strlen(text, max_length + 1u);
    return length <= max_length;
}

static bool count_and_array_are_valid(size_t count, const void *records) {
    return count <= CORE_HEADLESS_JOB_MAX_RECORDS && (count == 0u || records != NULL);
}

static bool utc_timestamp_validate(const char *timestamp) {
    size_t index;
    size_t length;

    if (!required_string(timestamp, CORE_HEADLESS_JOB_MAX_TIMESTAMP_LENGTH)) {
        return false;
    }
    length = strlen(timestamp);
    if (length < 20u || timestamp[length - 1u] != 'Z' ||
        timestamp[4] != '-' || timestamp[7] != '-' || timestamp[10] != 'T' ||
        timestamp[13] != ':' || timestamp[16] != ':') {
        return false;
    }
    for (index = 0u; index < 19u; ++index) {
        if (index == 4u || index == 7u || index == 10u ||
            index == 13u || index == 16u) {
            continue;
        }
        if (!isdigit((unsigned char)timestamp[index])) {
            return false;
        }
    }
    if (length == 20u) {
        return true;
    }
    if (timestamp[19] != '.' || length == 21u) {
        return false;
    }
    for (index = 20u; index + 1u < length; ++index) {
        if (!isdigit((unsigned char)timestamp[index])) {
            return false;
        }
    }
    return true;
}

static bool lowercase_hex_validate(const char *digest, size_t exact_length) {
    size_t index;

    if (!digest || bounded_strlen(digest, exact_length + 1u) != exact_length) {
        return false;
    }
    for (index = 0u; index < exact_length; ++index) {
        if (!((digest[index] >= '0' && digest[index] <= '9') ||
              (digest[index] >= 'a' && digest[index] <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool executable_digest_validate(const char *digest) {
    return digest && strncmp(digest, "sha256:", 7u) == 0 &&
           lowercase_hex_validate(digest + 7u, 64u);
}

static bool relative_path_validate(const char *path) {
    size_t index = 0u;
    size_t segment_start = 0u;

    if (!required_string(path, CORE_HEADLESS_JOB_MAX_PATH_LENGTH) ||
        path[0] == '/' || path[0] == '\\') {
        return false;
    }
    while (true) {
        char current = path[index];

        if (current == '\\') {
            return false;
        }
        if (current == '/' || current == '\0') {
            size_t segment_length = index - segment_start;

            if (segment_length == 0u ||
                (segment_length == 1u && path[segment_start] == '.') ||
                (segment_length == 2u && path[segment_start] == '.' &&
                 path[segment_start + 1u] == '.')) {
                return false;
            }
            if (current == '\0') {
                return true;
            }
            segment_start = index + 1u;
        }
        index += 1u;
    }
}

static bool schema_is(
    const char *family,
    const char *variant,
    const char *expected_family,
    const char *expected_variant
) {
    return required_string(family, CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH) &&
           required_string(variant, CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH) &&
           strcmp(family, expected_family) == 0 &&
           strcmp(variant, expected_variant) == 0;
}

static bool contract_ref_validate(
    const CoreHeadlessJobContractRef *contract,
    bool allow_empty
) {
    bool family_empty;
    bool variant_empty;

    if (!contract) {
        return false;
    }
    family_empty = contract->schema_family[0] == '\0';
    variant_empty = contract->schema_variant[0] == '\0';
    if (family_empty || variant_empty) {
        return allow_empty && family_empty && variant_empty;
    }
    return required_string(
               contract->schema_family,
               CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH
           ) &&
           required_string(
               contract->schema_variant,
               CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH
           );
}

static bool payload_ref_validate(const CoreHeadlessJobPayloadRef *payload) {
    return payload &&
           required_string(
               payload->schema_family,
               CORE_HEADLESS_JOB_MAX_SCHEMA_FAMILY_LENGTH
           ) &&
           required_string(
               payload->schema_variant,
               CORE_HEADLESS_JOB_MAX_SCHEMA_VARIANT_LENGTH
           ) &&
           relative_path_validate(payload->path);
}

static bool tool_identity_validate(const CoreHeadlessJobToolIdentity *tool) {
    return tool &&
           required_string(tool->name, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           required_string(tool->version, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           required_string(tool->target_os, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) &&
           required_string(tool->target_arch, CORE_HEADLESS_JOB_MAX_NAME_LENGTH);
}

static bool id_ref_array_validate(const CoreHeadlessJobIdRef *ids, size_t count) {
    size_t index;

    if (!count_and_array_are_valid(count, ids)) {
        return false;
    }
    for (index = 0u; index < count; ++index) {
        if (!required_string(ids[index].id, CORE_HEADLESS_JOB_MAX_ID_LENGTH)) {
            return false;
        }
    }
    return true;
}

static bool stage_precedes(
    const CoreHeadlessJobWorkflowManifestV1 *workflow,
    size_t current_index,
    const char *stage_id
) {
    size_t index;

    for (index = 0u; index < current_index; ++index) {
        if (strcmp(workflow->stages[index].stage_id, stage_id) == 0) {
            return true;
        }
    }
    return false;
}

void core_headless_job_envelope_v1_init(CoreHeadlessJobEnvelopeV1 *job) {
    if (!job) {
        return;
    }
    memset(job, 0, sizeof(*job));
    memcpy(
        job->schema_family,
        CORE_HEADLESS_JOB_ENVELOPE_FAMILY,
        sizeof(CORE_HEADLESS_JOB_ENVELOPE_FAMILY)
    );
    memcpy(
        job->schema_variant,
        CORE_HEADLESS_JOB_ENVELOPE_VARIANT,
        sizeof(CORE_HEADLESS_JOB_ENVELOPE_VARIANT)
    );
    job->retry.max_attempts = 1u;
}

void core_headless_job_event_v1_init(CoreHeadlessJobEventV1 *event) {
    if (!event) {
        return;
    }
    memset(event, 0, sizeof(*event));
    memcpy(
        event->schema_family,
        CORE_HEADLESS_JOB_EVENT_FAMILY,
        sizeof(CORE_HEADLESS_JOB_EVENT_FAMILY)
    );
    memcpy(
        event->schema_variant,
        CORE_HEADLESS_JOB_EVENT_VARIANT,
        sizeof(CORE_HEADLESS_JOB_EVENT_VARIANT)
    );
}

void core_headless_job_artifact_manifest_v1_init(
    CoreHeadlessJobArtifactManifestV1 *artifact
) {
    if (!artifact) {
        return;
    }
    memset(artifact, 0, sizeof(*artifact));
    memcpy(
        artifact->schema_family,
        CORE_HEADLESS_JOB_ARTIFACT_FAMILY,
        sizeof(CORE_HEADLESS_JOB_ARTIFACT_FAMILY)
    );
    memcpy(
        artifact->schema_variant,
        CORE_HEADLESS_JOB_ARTIFACT_VARIANT,
        sizeof(CORE_HEADLESS_JOB_ARTIFACT_VARIANT)
    );
}

void core_headless_job_result_v1_init(CoreHeadlessJobResultV1 *result) {
    if (!result) {
        return;
    }
    memset(result, 0, sizeof(*result));
    memcpy(
        result->schema_family,
        CORE_HEADLESS_JOB_RESULT_FAMILY,
        sizeof(CORE_HEADLESS_JOB_RESULT_FAMILY)
    );
    memcpy(
        result->schema_variant,
        CORE_HEADLESS_JOB_RESULT_VARIANT,
        sizeof(CORE_HEADLESS_JOB_RESULT_VARIANT)
    );
}

void core_headless_job_workflow_manifest_v1_init(
    CoreHeadlessJobWorkflowManifestV1 *workflow
) {
    if (!workflow) {
        return;
    }
    memset(workflow, 0, sizeof(*workflow));
    memcpy(
        workflow->schema_family,
        CORE_HEADLESS_JOB_WORKFLOW_FAMILY,
        sizeof(CORE_HEADLESS_JOB_WORKFLOW_FAMILY)
    );
    memcpy(
        workflow->schema_variant,
        CORE_HEADLESS_JOB_WORKFLOW_VARIANT,
        sizeof(CORE_HEADLESS_JOB_WORKFLOW_VARIANT)
    );
}

void core_headless_job_worker_capabilities_v1_init(
    CoreHeadlessJobWorkerCapabilitiesV1 *worker
) {
    if (!worker) {
        return;
    }
    memset(worker, 0, sizeof(*worker));
    memcpy(
        worker->schema_family,
        CORE_HEADLESS_JOB_WORKER_FAMILY,
        sizeof(CORE_HEADLESS_JOB_WORKER_FAMILY)
    );
    memcpy(
        worker->schema_variant,
        CORE_HEADLESS_JOB_WORKER_VARIANT,
        sizeof(CORE_HEADLESS_JOB_WORKER_VARIANT)
    );
}

const char *core_headless_job_state_name(CoreHeadlessJobPlatformState state) {
    switch (state) {
        case CORE_HEADLESS_JOB_STATE_SUBMITTED:
            return "submitted";
        case CORE_HEADLESS_JOB_STATE_VALIDATING:
            return "validating";
        case CORE_HEADLESS_JOB_STATE_QUEUED:
            return "queued";
        case CORE_HEADLESS_JOB_STATE_CLAIMED:
            return "claimed";
        case CORE_HEADLESS_JOB_STATE_PREPARING:
            return "preparing";
        case CORE_HEADLESS_JOB_STATE_RUNNING:
            return "running";
        case CORE_HEADLESS_JOB_STATE_COLLECTING:
            return "collecting";
        case CORE_HEADLESS_JOB_STATE_COMPLETED:
            return "completed";
        case CORE_HEADLESS_JOB_STATE_FAILED:
            return "failed";
        case CORE_HEADLESS_JOB_STATE_CANCELLED:
            return "cancelled";
        case CORE_HEADLESS_JOB_STATE_INVALID:
        default:
            return "invalid";
    }
}

const char *core_headless_job_event_kind_name(CoreHeadlessJobEventKind kind) {
    switch (kind) {
        case CORE_HEADLESS_JOB_EVENT_STATE_TRANSITION:
            return "state_transition";
        case CORE_HEADLESS_JOB_EVENT_PROGRESS:
            return "progress";
        case CORE_HEADLESS_JOB_EVENT_DIAGNOSTIC:
            return "diagnostic";
        case CORE_HEADLESS_JOB_EVENT_CLAIM:
            return "claim";
        case CORE_HEADLESS_JOB_EVENT_LEASE:
            return "lease";
        case CORE_HEADLESS_JOB_EVENT_ARTIFACT:
            return "artifact";
        case CORE_HEADLESS_JOB_EVENT_INVALID:
        default:
            return "invalid";
    }
}

const char *core_headless_job_outcome_name(CoreHeadlessJobOutcome outcome) {
    switch (outcome) {
        case CORE_HEADLESS_JOB_OUTCOME_COMPLETED:
            return "completed";
        case CORE_HEADLESS_JOB_OUTCOME_FAILED:
            return "failed";
        case CORE_HEADLESS_JOB_OUTCOME_CANCELLED:
            return "cancelled";
        case CORE_HEADLESS_JOB_OUTCOME_INVALID:
        default:
            return "invalid";
    }
}

bool core_headless_job_state_is_terminal(CoreHeadlessJobPlatformState state) {
    return state == CORE_HEADLESS_JOB_STATE_COMPLETED ||
           state == CORE_HEADLESS_JOB_STATE_FAILED ||
           state == CORE_HEADLESS_JOB_STATE_CANCELLED;
}

bool core_headless_job_state_transition_is_valid(
    CoreHeadlessJobPlatformState previous_state,
    CoreHeadlessJobPlatformState state
) {
    if (previous_state == CORE_HEADLESS_JOB_STATE_INVALID) {
        return state == CORE_HEADLESS_JOB_STATE_SUBMITTED;
    }
    if (core_headless_job_state_is_terminal(previous_state) ||
        state == CORE_HEADLESS_JOB_STATE_INVALID ||
        previous_state == state) {
        return false;
    }
    if (state == CORE_HEADLESS_JOB_STATE_FAILED ||
        state == CORE_HEADLESS_JOB_STATE_CANCELLED) {
        return true;
    }
    switch (previous_state) {
        case CORE_HEADLESS_JOB_STATE_SUBMITTED:
            return state == CORE_HEADLESS_JOB_STATE_VALIDATING;
        case CORE_HEADLESS_JOB_STATE_VALIDATING:
            return state == CORE_HEADLESS_JOB_STATE_QUEUED;
        case CORE_HEADLESS_JOB_STATE_QUEUED:
            return state == CORE_HEADLESS_JOB_STATE_CLAIMED;
        case CORE_HEADLESS_JOB_STATE_CLAIMED:
            return state == CORE_HEADLESS_JOB_STATE_PREPARING ||
                   state == CORE_HEADLESS_JOB_STATE_QUEUED;
        case CORE_HEADLESS_JOB_STATE_PREPARING:
            return state == CORE_HEADLESS_JOB_STATE_RUNNING ||
                   state == CORE_HEADLESS_JOB_STATE_QUEUED;
        case CORE_HEADLESS_JOB_STATE_RUNNING:
            return state == CORE_HEADLESS_JOB_STATE_COLLECTING ||
                   state == CORE_HEADLESS_JOB_STATE_QUEUED;
        case CORE_HEADLESS_JOB_STATE_COLLECTING:
            return state == CORE_HEADLESS_JOB_STATE_COMPLETED ||
                   state == CORE_HEADLESS_JOB_STATE_QUEUED;
        case CORE_HEADLESS_JOB_STATE_COMPLETED:
        case CORE_HEADLESS_JOB_STATE_FAILED:
        case CORE_HEADLESS_JOB_STATE_CANCELLED:
        case CORE_HEADLESS_JOB_STATE_INVALID:
        default:
            return false;
    }
}

bool core_headless_job_envelope_v1_validate(const CoreHeadlessJobEnvelopeV1 *job) {
    size_t index;

    if (!job ||
        !schema_is(
            job->schema_family,
            job->schema_variant,
            CORE_HEADLESS_JOB_ENVELOPE_FAMILY,
            CORE_HEADLESS_JOB_ENVELOPE_VARIANT
        ) ||
        !required_string(job->job_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(job->idempotency_key, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(job->program, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !tool_identity_validate(&job->adapter) ||
        !payload_ref_validate(&job->payload) ||
        !contract_ref_validate(&job->output_contract, false) ||
        !required_string(job->created_by, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !utc_timestamp_validate(job->created_at) ||
        job->retry.max_attempts == 0u ||
        !count_and_array_are_valid(job->input_artifact_count, job->input_artifacts) ||
        !count_and_array_are_valid(
            job->required_capability_count,
            job->required_capabilities
        ) ||
        !id_ref_array_validate(job->parent_jobs, job->parent_job_count)) {
        return false;
    }

    for (index = 0u; index < job->input_artifact_count; ++index) {
        if (!required_string(
                job->input_artifacts[index].artifact_id,
                CORE_HEADLESS_JOB_MAX_ID_LENGTH
            ) ||
            !required_string(
                job->input_artifacts[index].role,
                CORE_HEADLESS_JOB_MAX_ROLE_LENGTH
            )) {
            return false;
        }
    }

    for (index = 0u; index < job->required_capability_count; ++index) {
        if (!required_string(
                job->required_capabilities[index].name,
                CORE_HEADLESS_JOB_MAX_CAPABILITY_LENGTH
            ) ||
            !optional_string(
                job->required_capabilities[index].version_constraint,
                CORE_HEADLESS_JOB_MAX_CONSTRAINT_LENGTH
            )) {
            return false;
        }
    }
    return true;
}

bool core_headless_job_event_v1_validate(const CoreHeadlessJobEventV1 *event) {
    if (!event ||
        !schema_is(
            event->schema_family,
            event->schema_variant,
            CORE_HEADLESS_JOB_EVENT_FAMILY,
            CORE_HEADLESS_JOB_EVENT_VARIANT
        ) ||
        !required_string(event->event_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(event->job_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !optional_string(event->attempt_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !optional_string(event->worker_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !optional_string(event->lease_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        event->sequence == 0u ||
        !utc_timestamp_validate(event->occurred_at) ||
        event->kind == CORE_HEADLESS_JOB_EVENT_INVALID ||
        event->kind > CORE_HEADLESS_JOB_EVENT_ARTIFACT ||
        !optional_string(event->code, CORE_HEADLESS_JOB_MAX_FAILURE_CODE_LENGTH) ||
        !optional_string(event->message, CORE_HEADLESS_JOB_MAX_TEXT_LENGTH)) {
        return false;
    }

    if (event->kind == CORE_HEADLESS_JOB_EVENT_STATE_TRANSITION) {
        return core_headless_job_state_transition_is_valid(
            event->previous_state,
            event->state
        );
    }
    if (event->state == CORE_HEADLESS_JOB_STATE_INVALID) {
        return false;
    }
    if (event->kind == CORE_HEADLESS_JOB_EVENT_PROGRESS) {
        return isfinite(event->progress) && event->progress >= 0.0 &&
               event->progress <= 1.0;
    }
    if (event->kind == CORE_HEADLESS_JOB_EVENT_CLAIM ||
        event->kind == CORE_HEADLESS_JOB_EVENT_LEASE) {
        return required_string(event->attempt_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) &&
               required_string(event->worker_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) &&
               required_string(event->lease_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH);
    }
    return true;
}

bool core_headless_job_artifact_manifest_v1_validate(
    const CoreHeadlessJobArtifactManifestV1 *artifact
) {
    size_t index;

    if (!artifact ||
        !schema_is(
            artifact->schema_family,
            artifact->schema_variant,
            CORE_HEADLESS_JOB_ARTIFACT_FAMILY,
            CORE_HEADLESS_JOB_ARTIFACT_VARIANT
        ) ||
        !required_string(artifact->artifact_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(artifact->logical_name, CORE_HEADLESS_JOB_MAX_TEXT_LENGTH) ||
        !required_string(artifact->type, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !optional_string(
            artifact->media_type,
            CORE_HEADLESS_JOB_MAX_MEDIA_TYPE_LENGTH
        ) ||
        !contract_ref_validate(&artifact->data_contract, true) ||
        strcmp(artifact->digest_algorithm, "sha256") != 0 ||
        !lowercase_hex_validate(artifact->digest, 64u) ||
        !required_string(
            artifact->producer_job_id,
            CORE_HEADLESS_JOB_MAX_ID_LENGTH
        ) ||
        !required_string(
            artifact->producer_attempt_id,
            CORE_HEADLESS_JOB_MAX_ID_LENGTH
        ) ||
        !id_ref_array_validate(
            artifact->parent_artifacts,
            artifact->parent_artifact_count
        ) ||
        !count_and_array_are_valid(
            artifact->constituent_count,
            artifact->constituents
        )) {
        return false;
    }

    for (index = 0u; index < artifact->constituent_count; ++index) {
        if (!relative_path_validate(artifact->constituents[index].path) ||
            !lowercase_hex_validate(
                artifact->constituents[index].digest,
                64u
            )) {
            return false;
        }
    }
    return true;
}

bool core_headless_job_result_v1_validate(const CoreHeadlessJobResultV1 *result) {
    bool has_failure;

    if (!result ||
        !schema_is(
            result->schema_family,
            result->schema_variant,
            CORE_HEADLESS_JOB_RESULT_FAMILY,
            CORE_HEADLESS_JOB_RESULT_VARIANT
        ) ||
        !required_string(result->job_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(result->attempt_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        result->outcome == CORE_HEADLESS_JOB_OUTCOME_INVALID ||
        result->outcome > CORE_HEADLESS_JOB_OUTCOME_CANCELLED ||
        !utc_timestamp_validate(result->finished_at) ||
        !tool_identity_validate(&result->executor) ||
        !id_ref_array_validate(
            result->output_artifacts,
            result->output_artifact_count
        ) ||
        !optional_string(
            result->failure_code,
            CORE_HEADLESS_JOB_MAX_FAILURE_CODE_LENGTH
        ) ||
        !optional_string(
            result->failure_message,
            CORE_HEADLESS_JOB_MAX_TEXT_LENGTH
        )) {
        return false;
    }

    has_failure = result->failure_code[0] != '\0' &&
                  result->failure_message[0] != '\0';
    if (result->outcome == CORE_HEADLESS_JOB_OUTCOME_COMPLETED) {
        return result->failure_code[0] == '\0' &&
               result->failure_message[0] == '\0';
    }
    return has_failure;
}

bool core_headless_job_workflow_manifest_v1_validate(
    const CoreHeadlessJobWorkflowManifestV1 *workflow
) {
    size_t stage_index;
    size_t other_index;
    size_t item_index;

    if (!workflow ||
        !schema_is(
            workflow->schema_family,
            workflow->schema_variant,
            CORE_HEADLESS_JOB_WORKFLOW_FAMILY,
            CORE_HEADLESS_JOB_WORKFLOW_VARIANT
        ) ||
        !required_string(workflow->workflow_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(
            workflow->idempotency_key,
            CORE_HEADLESS_JOB_MAX_ID_LENGTH
        ) ||
        workflow->stage_count == 0u ||
        !count_and_array_are_valid(workflow->stage_count, workflow->stages) ||
        !required_string(workflow->created_by, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !utc_timestamp_validate(workflow->created_at)) {
        return false;
    }

    for (stage_index = 0u; stage_index < workflow->stage_count; ++stage_index) {
        const CoreHeadlessJobWorkflowStageV1 *stage = &workflow->stages[stage_index];

        if (!required_string(stage->stage_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
            !required_string(
                stage->job_template_id,
                CORE_HEADLESS_JOB_MAX_ID_LENGTH
            ) ||
            !id_ref_array_validate(stage->dependencies, stage->dependency_count) ||
            !count_and_array_are_valid(stage->binding_count, stage->bindings)) {
            return false;
        }

        for (other_index = 0u; other_index < stage_index; ++other_index) {
            if (strcmp(
                    workflow->stages[other_index].stage_id,
                    stage->stage_id
                ) == 0) {
                return false;
            }
        }

        for (item_index = 0u; item_index < stage->dependency_count; ++item_index) {
            if (!stage_precedes(
                    workflow,
                    stage_index,
                    stage->dependencies[item_index].id
                )) {
                return false;
            }
        }

        for (item_index = 0u; item_index < stage->binding_count; ++item_index) {
            const CoreHeadlessJobArtifactBinding *binding = &stage->bindings[item_index];

            if (!required_string(
                    binding->input_role,
                    CORE_HEADLESS_JOB_MAX_ROLE_LENGTH
                ) ||
                !required_string(
                    binding->source_stage_id,
                    CORE_HEADLESS_JOB_MAX_ID_LENGTH
                ) ||
                !required_string(
                    binding->source_artifact_role,
                    CORE_HEADLESS_JOB_MAX_ROLE_LENGTH
                ) ||
                !stage_precedes(
                    workflow,
                    stage_index,
                    binding->source_stage_id
                )) {
                return false;
            }
        }
    }
    return true;
}

bool core_headless_job_worker_capabilities_v1_validate(
    const CoreHeadlessJobWorkerCapabilitiesV1 *worker
) {
    size_t index;

    if (!worker ||
        !schema_is(
            worker->schema_family,
            worker->schema_variant,
            CORE_HEADLESS_JOB_WORKER_FAMILY,
            CORE_HEADLESS_JOB_WORKER_VARIANT
        ) ||
        !required_string(worker->worker_id, CORE_HEADLESS_JOB_MAX_ID_LENGTH) ||
        !required_string(worker->target_os, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        !required_string(worker->target_arch, CORE_HEADLESS_JOB_MAX_NAME_LENGTH) ||
        worker->adapter_count == 0u ||
        !count_and_array_are_valid(worker->adapter_count, worker->adapters) ||
        !count_and_array_are_valid(
            worker->capability_count,
            worker->capabilities
        ) ||
        worker->resources.cpu_cores == 0u ||
        worker->resources.memory_bytes == 0u ||
        !utc_timestamp_validate(worker->observed_at)) {
        return false;
    }

    for (index = 0u; index < worker->adapter_count; ++index) {
        if (!required_string(
                worker->adapters[index].name,
                CORE_HEADLESS_JOB_MAX_NAME_LENGTH
            ) ||
            !required_string(
                worker->adapters[index].version,
                CORE_HEADLESS_JOB_MAX_NAME_LENGTH
            ) ||
            !executable_digest_validate(
                worker->adapters[index].executable_digest
            )) {
            return false;
        }
    }

    for (index = 0u; index < worker->capability_count; ++index) {
        if (!required_string(
                worker->capabilities[index].name,
                CORE_HEADLESS_JOB_MAX_CAPABILITY_LENGTH
            ) ||
            !optional_string(
                worker->capabilities[index].version,
                CORE_HEADLESS_JOB_MAX_NAME_LENGTH
            )) {
            return false;
        }
    }
    return true;
}
