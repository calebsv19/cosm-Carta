#!/usr/bin/env python3

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "fixtures" / "platform_v1"
UTC_RE = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?Z$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
EXECUTABLE_DIGEST_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
STATES = {
    "submitted",
    "validating",
    "queued",
    "claimed",
    "preparing",
    "running",
    "collecting",
    "completed",
    "failed",
    "cancelled",
}


def load(name):
    def reject_duplicates(pairs):
        result = {}
        for key, value in pairs:
            assert key not in result, f"{name}: duplicate key {key}"
            result[key] = value
        return result

    with (FIXTURES / name).open("r", encoding="utf-8") as handle:
        return json.load(handle, object_pairs_hook=reject_duplicates)


def require_keys(document, keys, name):
    missing = set(keys) - set(document)
    assert not missing, f"{name}: missing {sorted(missing)}"


def validate_timestamp(value, name):
    assert isinstance(value, str) and UTC_RE.fullmatch(value), (
        f"{name}: invalid UTC timestamp"
    )


def validate_relative_path(value, name):
    assert isinstance(value, str) and value, f"{name}: empty path"
    assert not value.startswith(("/", "\\")), f"{name}: absolute path"
    assert "\\" not in value, f"{name}: backslash path"
    assert all(part not in {"", ".", ".."} for part in value.split("/")), (
        f"{name}: unsafe segment"
    )


def validate_schema(document, family, variant, name):
    assert document["schema_family"] == family, f"{name}: schema family"
    assert document["schema_variant"] == variant, f"{name}: schema variant"


def validate_job():
    name = "job_envelope.json"
    document = load(name)
    require_keys(
        document,
        {
            "schema_family",
            "schema_variant",
            "job_id",
            "idempotency_key",
            "program",
            "adapter",
            "payload",
            "input_artifacts",
            "required_capabilities",
            "output_contract",
            "resources",
            "retry",
            "parent_jobs",
            "created_by",
            "created_at",
        },
        name,
    )
    validate_schema(document, "codework_job", "job_envelope_v1", name)
    validate_relative_path(document["payload"]["path"], name)
    assert document["retry"]["max_attempts"] >= 1
    validate_timestamp(document["created_at"], name)
    assert all(item["artifact_id"] and item["role"] for item in document["input_artifacts"])
    assert all(item["name"] for item in document["required_capabilities"])


def validate_event():
    name = "job_event.json"
    document = load(name)
    require_keys(
        document,
        {
            "schema_family",
            "schema_variant",
            "event_id",
            "job_id",
            "attempt_id",
            "worker_id",
            "lease_id",
            "sequence",
            "occurred_at",
            "kind",
            "previous_state",
            "state",
            "progress",
            "code",
            "message",
        },
        name,
    )
    validate_schema(document, "codework_job_event", "job_event_v1", name)
    assert document["sequence"] > 0
    validate_timestamp(document["occurred_at"], name)
    assert document["state"] in STATES
    assert document["kind"] in {
        "state_transition",
        "progress",
        "diagnostic",
        "claim",
        "lease",
        "artifact",
    }
    assert document["previous_state"] is None or document["previous_state"] in STATES
    assert 0.0 <= document["progress"] <= 1.0


def validate_artifact():
    name = "artifact_manifest.json"
    document = load(name)
    require_keys(
        document,
        {
            "schema_family",
            "schema_variant",
            "artifact_id",
            "logical_name",
            "type",
            "media_type",
            "data_contract",
            "digest_algorithm",
            "digest",
            "size_bytes",
            "producer_job_id",
            "producer_attempt_id",
            "parent_artifacts",
            "constituents",
        },
        name,
    )
    validate_schema(document, "codework_artifact", "artifact_manifest_v1", name)
    assert document["digest_algorithm"] == "sha256"
    assert SHA256_RE.fullmatch(document["digest"])
    assert document["size_bytes"] >= 0
    for constituent in document["constituents"]:
        validate_relative_path(constituent["path"], name)
        assert SHA256_RE.fullmatch(constituent["digest"])
        assert constituent["size_bytes"] >= 0


def validate_result():
    name = "job_result.json"
    document = load(name)
    require_keys(
        document,
        {
            "schema_family",
            "schema_variant",
            "job_id",
            "attempt_id",
            "outcome",
            "finished_at",
            "executor",
            "output_artifacts",
            "failure_code",
            "failure_message",
        },
        name,
    )
    validate_schema(document, "codework_job_result", "job_result_v1", name)
    assert document["outcome"] in {"completed", "failed", "cancelled"}
    validate_timestamp(document["finished_at"], name)
    if document["outcome"] == "completed":
        assert not document["failure_code"] and not document["failure_message"]
    else:
        assert document["failure_code"] and document["failure_message"]


def validate_workflow():
    name = "workflow_manifest.json"
    document = load(name)
    require_keys(
        document,
        {
            "schema_family",
            "schema_variant",
            "workflow_id",
            "idempotency_key",
            "stages",
            "created_by",
            "created_at",
        },
        name,
    )
    validate_schema(document, "codework_workflow", "workflow_manifest_v1", name)
    validate_timestamp(document["created_at"], name)
    seen = set()
    for stage in document["stages"]:
        assert stage["stage_id"] not in seen
        assert all(dependency in seen for dependency in stage["dependencies"])
        assert all(binding["source_stage_id"] in seen for binding in stage["bindings"])
        seen.add(stage["stage_id"])
    assert seen


def validate_worker():
    name = "worker_capabilities.json"
    document = load(name)
    require_keys(
        document,
        {
            "schema_family",
            "schema_variant",
            "worker_id",
            "target_os",
            "target_arch",
            "adapters",
            "capabilities",
            "resources",
            "observed_at",
        },
        name,
    )
    validate_schema(document, "codework_worker", "worker_capabilities_v1", name)
    assert document["adapters"]
    assert all(
        EXECUTABLE_DIGEST_RE.fullmatch(adapter["executable_digest"])
        for adapter in document["adapters"]
    )
    assert document["resources"]["cpu_cores"] > 0
    assert document["resources"]["memory_bytes"] > 0
    assert document["resources"]["gpu_count"] >= 0
    validate_timestamp(document["observed_at"], name)


def main():
    validate_job()
    validate_event()
    validate_artifact()
    validate_result()
    validate_workflow()
    validate_worker()


if __name__ == "__main__":
    main()
