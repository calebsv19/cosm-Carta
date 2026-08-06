#!/usr/bin/env python3
"""Validate S3 residency reports and compare deterministic evidence."""

from __future__ import annotations

import argparse
import json
import pathlib

MODULE_VERSION = (
    pathlib.Path(__file__).resolve().parents[1] / "VERSION"
).read_text(encoding="utf-8").strip()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def validate(document: dict, require_validation: bool) -> None:
    require(
        document.get("schema") == "codework_gpu_residency_report_v1",
        "unexpected residency schema",
    )
    require(document.get("schema_version") == 1, "unexpected schema version")
    require(document.get("module_version") == MODULE_VERSION,
            "unexpected module")
    require(document.get("runtime_status") == "ok", "runtime did not pass")
    validation = document["validation"]
    require(validation["warnings"] == 0, "validation warnings present")
    require(validation["errors"] == 0, "validation errors present")
    if require_validation:
        require(validation["enabled"], "validation not enabled")
    require(len(document["device_identity"]["uuid"]) == 32,
            "device UUID missing")
    memory = document["memory"]
    require(memory["staging_host_visible_coherent"],
            "staging memory contract failed")
    require(memory["device_a_local"] and memory["device_b_local"],
            "device-local memory contract failed")
    contract = document["contract"]
    require(contract == {
        "lifecycle_cycles": 4,
        "chains": 13,
        "dispatches_per_chain": 2,
        "barriers_per_chain": 1,
        "readbacks_per_chain": 1,
        "final_only_readback": True,
        "staging_reused": True,
        "device_buffers_reused": True,
        "descriptor_state_reused": True,
        "command_state_reused": True,
    }, "resident execution contract drift")
    require(document["negative_fixtures"] == {
        "null_session": "invalid_argument",
        "double_session": "invalid_argument",
        "buffer_range": "buffer_range_invalid",
        "program_binding": "descriptor_binding_invalid",
        "referenced_buffer": "resource_in_use",
        "close_with_resources": "resource_in_use",
    }, "negative fixture contract drift")
    require(document["timeout_fixture"] == {
        "submit_status": "fence_wait_timeout",
        "in_flight_observed": True,
        "close_status": "resource_in_use",
        "recovery_status": "ok",
    }, "timeout recovery contract drift")
    require(document["resource_accounting"] == {
        "submissions": 39,
        "completed_submissions": 39,
        "uploads": 13,
        "readbacks": 13,
        "dispatches": 26,
        "barriers": 13,
        "final_live_counts_zero": True,
    }, "resource accounting drift")
    u32 = document["u32"]
    require(u32["parity"], "resident u32 parity failed")
    require(u32["value_count"] == 1025, "unexpected value count")
    require(len(u32["input"]) == 1025, "input length mismatch")
    require(len(u32["expected"]) == 1025, "expected length mismatch")
    require(len(u32["output"]) == 1025, "output length mismatch")
    require(u32["expected"] == u32["output"], "output differs from oracle")
    require(all(value >= 0 for value in document["timing_ns"].values()),
            "negative timing")


def deterministic(document: dict) -> dict:
    result = dict(document)
    result.pop("timing_ns", None)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report")
    parser.add_argument("--compare")
    parser.add_argument("--require-validation", action="store_true")
    args = parser.parse_args()
    first = json.loads(pathlib.Path(args.report).read_text(encoding="utf-8"))
    validate(first, args.require_validation)
    if args.compare:
        second = json.loads(
            pathlib.Path(args.compare).read_text(encoding="utf-8")
        )
        validate(second, args.require_validation)
        require(
            deterministic(first) == deterministic(second),
            "deterministic residency evidence differs outside timing",
        )
    print(f"validated {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
