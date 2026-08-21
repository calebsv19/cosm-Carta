#include "core_headless_job_platform.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static void fill_tool(CoreHeadlessJobToolIdentity *tool, const char *name) {
    strcpy(tool->name, name);
    strcpy(tool->version, "1.0.0");
    strcpy(tool->target_os, "macos");
    strcpy(tool->target_arch, "arm64");
}

static void test_state_vocabulary(void) {
    assert(strcmp(
               core_headless_job_state_name(CORE_HEADLESS_JOB_STATE_SUBMITTED),
               "submitted"
           ) == 0);
    assert(strcmp(
               core_headless_job_event_kind_name(CORE_HEADLESS_JOB_EVENT_CLAIM),
               "claim"
           ) == 0);
    assert(strcmp(
               core_headless_job_outcome_name(CORE_HEADLESS_JOB_OUTCOME_COMPLETED),
               "completed"
           ) == 0);
    assert(core_headless_job_state_transition_is_valid(
        CORE_HEADLESS_JOB_STATE_INVALID,
        CORE_HEADLESS_JOB_STATE_SUBMITTED
    ));
    assert(core_headless_job_state_transition_is_valid(
        CORE_HEADLESS_JOB_STATE_RUNNING,
        CORE_HEADLESS_JOB_STATE_COLLECTING
    ));
    assert(core_headless_job_state_transition_is_valid(
        CORE_HEADLESS_JOB_STATE_RUNNING,
        CORE_HEADLESS_JOB_STATE_QUEUED
    ));
    assert(core_headless_job_state_transition_is_valid(
        CORE_HEADLESS_JOB_STATE_RUNNING,
        CORE_HEADLESS_JOB_STATE_FAILED
    ));
    assert(!core_headless_job_state_transition_is_valid(
        CORE_HEADLESS_JOB_STATE_COMPLETED,
        CORE_HEADLESS_JOB_STATE_QUEUED
    ));
    assert(!core_headless_job_state_transition_is_valid(
        CORE_HEADLESS_JOB_STATE_RUNNING,
        CORE_HEADLESS_JOB_STATE_RUNNING
    ));
    assert(core_headless_job_state_is_terminal(CORE_HEADLESS_JOB_STATE_COMPLETED));
    assert(core_headless_job_state_is_terminal(CORE_HEADLESS_JOB_STATE_FAILED));
    assert(core_headless_job_state_is_terminal(CORE_HEADLESS_JOB_STATE_CANCELLED));
    assert(!core_headless_job_state_is_terminal(CORE_HEADLESS_JOB_STATE_RUNNING));
}

static void test_job_envelope(void) {
    CoreHeadlessJobEnvelopeV1 job;
    CoreHeadlessJobInputArtifact inputs[1];
    CoreHeadlessJobCapabilityRequirement capabilities[1];
    CoreHeadlessJobIdRef parents[1];

    core_headless_job_envelope_v1_init(&job);
    assert(strcmp(job.schema_family, CORE_HEADLESS_JOB_ENVELOPE_FAMILY) == 0);
    assert(strcmp(job.schema_variant, CORE_HEADLESS_JOB_ENVELOPE_VARIANT) == 0);
    assert(job.retry.max_attempts == 1u);
    assert(!core_headless_job_envelope_v1_validate(&job));

    strcpy(job.job_id, "job.physics.render.001");
    strcpy(job.idempotency_key, "physics-render-fixture-v1");
    strcpy(job.program, "physics_sim");
    fill_tool(&job.adapter, "physics_sim_adapter");
    strcpy(job.payload.schema_family, "physics_sim_request");
    strcpy(job.payload.schema_variant, "detached_job_v1");
    strcpy(job.payload.path, "input/physics_request.json");
    strcpy(inputs[0].artifact_id, "artifact.scene-project.001");
    strcpy(inputs[0].role, "scene_project");
    job.input_artifacts = inputs;
    job.input_artifact_count = 1u;
    strcpy(capabilities[0].name, "physics-sim-headless");
    strcpy(capabilities[0].version_constraint, ">=0.3.2");
    job.required_capabilities = capabilities;
    job.required_capability_count = 1u;
    strcpy(job.output_contract.schema_family, "physics_sim_cache");
    strcpy(job.output_contract.schema_variant, "active_cache_manifest_v1");
    job.resources.min_cpu_cores = 2u;
    job.resources.min_memory_bytes = 1024u;
    job.retry.max_attempts = 2u;
    strcpy(parents[0].id, "job.scene.validate.001");
    job.parent_jobs = parents;
    job.parent_job_count = 1u;
    strcpy(job.created_by, "contract-test");
    strcpy(job.created_at, "2026-07-28T20:00:00Z");
    assert(core_headless_job_envelope_v1_validate(&job));

    job.retry.max_attempts = 0u;
    assert(!core_headless_job_envelope_v1_validate(&job));
    job.retry.max_attempts = 2u;
    job.input_artifacts = NULL;
    assert(!core_headless_job_envelope_v1_validate(&job));
    job.input_artifacts = inputs;
    strcpy(job.payload.path, "../physics_request.json");
    assert(!core_headless_job_envelope_v1_validate(&job));
    strcpy(job.payload.path, "input/physics_request.json");
    strcpy(job.created_at, "2026-07-28 20:00:00Z");
    assert(!core_headless_job_envelope_v1_validate(&job));
}

static void test_events(void) {
    CoreHeadlessJobEventV1 event;

    core_headless_job_event_v1_init(&event);
    strcpy(event.event_id, "event.001");
    strcpy(event.job_id, "job.physics.render.001");
    event.sequence = 1u;
    strcpy(event.occurred_at, "2026-07-28T20:00:00Z");
    event.kind = CORE_HEADLESS_JOB_EVENT_STATE_TRANSITION;
    event.previous_state = CORE_HEADLESS_JOB_STATE_INVALID;
    event.state = CORE_HEADLESS_JOB_STATE_SUBMITTED;
    assert(core_headless_job_event_v1_validate(&event));

    event.previous_state = CORE_HEADLESS_JOB_STATE_COMPLETED;
    event.state = CORE_HEADLESS_JOB_STATE_RUNNING;
    assert(!core_headless_job_event_v1_validate(&event));

    event.kind = CORE_HEADLESS_JOB_EVENT_PROGRESS;
    event.previous_state = CORE_HEADLESS_JOB_STATE_INVALID;
    event.state = CORE_HEADLESS_JOB_STATE_RUNNING;
    event.progress = 0.5;
    assert(core_headless_job_event_v1_validate(&event));
    event.progress = NAN;
    assert(!core_headless_job_event_v1_validate(&event));

    event.kind = CORE_HEADLESS_JOB_EVENT_CLAIM;
    event.progress = 0.0;
    assert(!core_headless_job_event_v1_validate(&event));
    strcpy(event.attempt_id, "attempt.001");
    strcpy(event.worker_id, "worker.local.001");
    strcpy(event.lease_id, "lease.001");
    assert(core_headless_job_event_v1_validate(&event));
}

static void test_artifact_manifest(void) {
    CoreHeadlessJobArtifactManifestV1 artifact;
    CoreHeadlessJobIdRef parents[1];
    CoreHeadlessJobArtifactConstituent constituents[1];

    core_headless_job_artifact_manifest_v1_init(&artifact);
    strcpy(artifact.artifact_id, "artifact.physics-cache.001");
    strcpy(artifact.logical_name, "PhysicsSim project cache");
    strcpy(artifact.type, "directory_manifest");
    strcpy(artifact.media_type, "application/json");
    strcpy(artifact.data_contract.schema_family, "physics_sim_cache");
    strcpy(artifact.data_contract.schema_variant, "active_cache_manifest_v1");
    strcpy(artifact.digest_algorithm, "sha256");
    strcpy(
        artifact.digest,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    );
    artifact.size_bytes = 42u;
    strcpy(artifact.producer_job_id, "job.physics.render.001");
    strcpy(artifact.producer_attempt_id, "attempt.001");
    strcpy(parents[0].id, "artifact.scene-project.001");
    artifact.parent_artifacts = parents;
    artifact.parent_artifact_count = 1u;
    strcpy(constituents[0].path, "physics_sim/active_cache_manifest.json");
    strcpy(
        constituents[0].digest,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    );
    constituents[0].size_bytes = 42u;
    artifact.constituents = constituents;
    artifact.constituent_count = 1u;
    assert(core_headless_job_artifact_manifest_v1_validate(&artifact));

    artifact.data_contract.schema_variant[0] = '\0';
    assert(!core_headless_job_artifact_manifest_v1_validate(&artifact));
    strcpy(
        artifact.data_contract.schema_variant,
        "active_cache_manifest_v1"
    );
    artifact.constituents = NULL;
    assert(!core_headless_job_artifact_manifest_v1_validate(&artifact));
    artifact.constituents = constituents;
    artifact.digest[0] = 'A';
    assert(!core_headless_job_artifact_manifest_v1_validate(&artifact));
    artifact.digest[0] = 'a';
    memset(artifact.digest, 'a', sizeof(artifact.digest));
    assert(!core_headless_job_artifact_manifest_v1_validate(&artifact));
}

static void test_result(void) {
    CoreHeadlessJobResultV1 result;
    CoreHeadlessJobIdRef outputs[1];

    core_headless_job_result_v1_init(&result);
    strcpy(result.job_id, "job.physics.render.001");
    strcpy(result.attempt_id, "attempt.001");
    result.outcome = CORE_HEADLESS_JOB_OUTCOME_COMPLETED;
    strcpy(result.finished_at, "2026-07-28T20:01:00Z");
    fill_tool(&result.executor, "physics_sim");
    strcpy(outputs[0].id, "artifact.physics-cache.001");
    result.output_artifacts = outputs;
    result.output_artifact_count = 1u;
    assert(core_headless_job_result_v1_validate(&result));

    strcpy(result.failure_code, "should_not_exist");
    strcpy(result.failure_message, "completed results cannot carry failures");
    assert(!core_headless_job_result_v1_validate(&result));

    result.outcome = CORE_HEADLESS_JOB_OUTCOME_FAILED;
    assert(core_headless_job_result_v1_validate(&result));
    result.failure_message[0] = '\0';
    assert(!core_headless_job_result_v1_validate(&result));
}

static void test_workflow(void) {
    CoreHeadlessJobWorkflowManifestV1 workflow;
    CoreHeadlessJobWorkflowStageV1 stages[2];
    CoreHeadlessJobIdRef dependencies[1];
    CoreHeadlessJobArtifactBinding bindings[1];

    memset(stages, 0, sizeof(stages));
    memset(dependencies, 0, sizeof(dependencies));
    memset(bindings, 0, sizeof(bindings));
    core_headless_job_workflow_manifest_v1_init(&workflow);
    strcpy(workflow.workflow_id, "workflow.physics-render.001");
    strcpy(workflow.idempotency_key, "physics-render-workflow-fixture-v1");
    strcpy(workflow.created_by, "contract-test");
    strcpy(workflow.created_at, "2026-07-28T20:00:00Z");
    strcpy(stages[0].stage_id, "physics");
    strcpy(stages[0].job_template_id, "physics-job-template");
    strcpy(stages[1].stage_id, "render");
    strcpy(stages[1].job_template_id, "render-job-template");
    strcpy(dependencies[0].id, "physics");
    stages[1].dependencies = dependencies;
    stages[1].dependency_count = 1u;
    strcpy(bindings[0].input_role, "physics_cache");
    strcpy(bindings[0].source_stage_id, "physics");
    strcpy(bindings[0].source_artifact_role, "active_cache");
    stages[1].bindings = bindings;
    stages[1].binding_count = 1u;
    workflow.stages = stages;
    workflow.stage_count = 2u;
    assert(core_headless_job_workflow_manifest_v1_validate(&workflow));

    strcpy(dependencies[0].id, "missing");
    assert(!core_headless_job_workflow_manifest_v1_validate(&workflow));
    strcpy(dependencies[0].id, "physics");
    strcpy(stages[1].stage_id, "physics");
    assert(!core_headless_job_workflow_manifest_v1_validate(&workflow));
}

static void test_worker_capabilities(void) {
    CoreHeadlessJobWorkerCapabilitiesV1 worker;
    CoreHeadlessJobWorkerAdapter adapters[2];
    CoreHeadlessJobWorkerCapability capabilities[1];

    memset(adapters, 0, sizeof(adapters));
    memset(capabilities, 0, sizeof(capabilities));
    core_headless_job_worker_capabilities_v1_init(&worker);
    strcpy(worker.worker_id, "worker.local.001");
    strcpy(worker.target_os, "macos");
    strcpy(worker.target_arch, "arm64");
    strcpy(adapters[0].name, "physics_sim");
    strcpy(adapters[0].version, "0.3.2");
    strcpy(
        adapters[0].executable_digest,
        "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    );
    strcpy(adapters[1].name, "ray_tracing");
    strcpy(adapters[1].version, "0.10.1");
    strcpy(
        adapters[1].executable_digest,
        "sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
    );
    worker.adapters = adapters;
    worker.adapter_count = 2u;
    strcpy(capabilities[0].name, "scene-project-portable");
    strcpy(capabilities[0].version, "1");
    worker.capabilities = capabilities;
    worker.capability_count = 1u;
    worker.resources.cpu_cores = 8u;
    worker.resources.memory_bytes = 17179869184u;
    strcpy(worker.observed_at, "2026-07-28T20:00:00Z");
    assert(core_headless_job_worker_capabilities_v1_validate(&worker));

    worker.resources.cpu_cores = 0u;
    assert(!core_headless_job_worker_capabilities_v1_validate(&worker));
    worker.resources.cpu_cores = 8u;
    worker.adapters = NULL;
    assert(!core_headless_job_worker_capabilities_v1_validate(&worker));
    worker.adapters = adapters;
    adapters[0].executable_digest[7] = 'A';
    assert(!core_headless_job_worker_capabilities_v1_validate(&worker));
}

int main(void) {
    test_state_vocabulary();
    test_job_envelope();
    test_events();
    test_artifact_manifest();
    test_result();
    test_workflow();
    test_worker_capabilities();
    return 0;
}
