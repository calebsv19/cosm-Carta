#include "core_headless_job.h"

#include <assert.h>
#include <string.h>

int main(void) {
    CoreHeadlessJobEnvelope job;
    CoreHeadlessJobReport report;
    CoreHeadlessJobArtifact artifacts[2];

    core_headless_job_envelope_init(&job);
    assert(strcmp(job.schema_family, "codework_job") == 0);
    assert(strcmp(job.schema_variant, "headless_bundle_v1") == 0);
    assert(!core_headless_job_envelope_validate(&job));

    strcpy(job.job_id, "ray-tracing--trio-headless-worker--20260522T232500Z--audit07");
    strcpy(job.program, "ray_tracing");
    strcpy(job.tool.name, "ray_tracing");
    strcpy(job.tool.version, "0.1.0");
    strcpy(job.tool.target_os, "linux");
    strcpy(job.tool.target_arch, "x86_64");
    strcpy(job.scene_payload.schema_family, "codework_scene");
    strcpy(job.scene_payload.schema_variant, "scene_runtime_v1");
    strcpy(job.scene_payload.path, "input/scene_runtime.json");
    strcpy(job.run_config.schema_family, "ray_tracing_request");
    strcpy(job.run_config.schema_variant, "worker_request_v1");
    strcpy(job.run_config.path, "input/ray_tracing_request.json");
    strcpy(job.outputs.root, "output");
    strcpy(job.outputs.report_path, "output/report.json");
    strcpy(job.outputs.logs_dir, "logs");
    strcpy(job.outputs.artifacts_dir, "output/artifacts");
    strcpy(job.metadata.created_by, "codex");
    strcpy(job.metadata.created_at, "2026-05-22T23:25:00Z");
    assert(core_headless_job_envelope_validate(&job));

    job.outputs.logs_dir[0] = '\0';
    assert(!core_headless_job_envelope_validate(&job));
    strcpy(job.outputs.logs_dir, "logs");

    core_headless_job_artifact_init(&artifacts[0]);
    assert(!core_headless_job_artifact_validate(&artifacts[0]));
    strcpy(artifacts[0].type, "video");
    strcpy(artifacts[0].path, "output/artifacts/preview.mp4");
    assert(core_headless_job_artifact_validate(&artifacts[0]));

    strcpy(artifacts[1].type, "frame_sequence");
    strcpy(artifacts[1].path, "output/artifacts/frames");
    assert(core_headless_job_artifact_validate(&artifacts[1]));

    core_headless_job_report_init(&report);
    assert(strcmp(report.schema_family, "codework_job_report") == 0);
    assert(strcmp(report.schema_variant, "headless_report_v1") == 0);
    assert(!core_headless_job_report_validate(&report));

    strcpy(report.job_id, "ray-tracing--trio-headless-worker--20260522T232500Z--audit07");
    strcpy(report.program, "ray_tracing");
    strcpy(report.state, "succeeded");
    strcpy(report.stage, "visualizer_publish");
    strcpy(report.created_at, "2026-05-22T23:25:00Z");
    strcpy(report.updated_at, "2026-05-22T23:27:10Z");
    strcpy(report.started_at, "2026-05-22T23:25:01Z");
    strcpy(report.finished_at, "2026-05-22T23:27:10Z");
    report.artifacts = artifacts;
    report.artifact_count = 2u;
    assert(core_headless_job_report_validate(&report));

    report.artifacts = NULL;
    assert(!core_headless_job_report_validate(&report));
    report.artifacts = artifacts;

    report.state[0] = '\0';
    assert(!core_headless_job_report_validate(&report));

    return 0;
}
