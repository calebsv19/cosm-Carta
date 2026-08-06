#!/usr/bin/env python3
"""Validate S4 timing/crossover reports and deterministic structure."""

from __future__ import annotations

import argparse
import copy
import json
import math
import pathlib

MODULE_VERSION = (
    pathlib.Path(__file__).resolve().parents[1] / "VERSION"
).read_text(encoding="utf-8").strip()


EXPECTED_SIZES = [256, 1024, 4096, 16384, 65536, 262144, 1048576]
TIMING_KEYS = {
    "cpu_reference",
    "upload_host_copy",
    "upload_submit_wait",
    "upload_gpu",
    "execution_submit_wait",
    "execution_gpu",
    "download_submit_wait",
    "download_gpu",
    "readback_host_copy",
    "transfer_gpu",
    "total_wall",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def validate(document: dict, require_validation: bool) -> None:
    require(
        document.get("schema") == "codework_gpu_timing_report_v1",
        "unexpected timing report schema",
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

    platform = document["platform"]
    require(platform["os"], "platform OS missing")
    require(platform["architecture"], "platform architecture missing")
    require(platform["device_name"], "device name missing")
    require(len(platform["device_uuid"]) == 32, "device UUID missing")
    require(platform["compute_queue_family"] >= 0,
            "compute queue family missing")

    timestamp = document["timestamp"]
    require(timestamp["supported"], "timestamp queries unsupported")
    require(0 < timestamp["valid_bits"] <= 64,
            "invalid timestamp valid-bit count")
    require(math.isfinite(timestamp["period_ns"]) and
            timestamp["period_ns"] > 0,
            "invalid timestamp period")
    require(timestamp["measurement_count"] == 189,
            "timestamp measurement accounting drift")

    require(document["methodology"] == {
        "warmups": 2,
        "samples": 7,
        "aggregation": "median",
        "cpu_batch_min_values": 16777216,
        "dispatches_per_sample": 2,
        "barriers_per_sample": 1,
        "final_readbacks_per_sample": 1,
        "gpu_timestamps_bracket_queue_commands": True,
        "submit_wait_includes_fence_wait": True,
        "cpu_operation": "twice(output=input*3u+7u)",
    }, "timing methodology drift")

    workloads = document["workloads"]
    require([item["value_count"] for item in workloads] == EXPECTED_SIZES,
            "workload-size ladder drift")
    for item in workloads:
        count = item["value_count"]
        require(item["byte_count"] == count * 4, "byte count mismatch")
        require(item["group_count_x"] == (count + 63) // 64,
                "dispatch group count mismatch")
        require(item["parity"], "CPU/GPU workload parity failed")
        require(item["checksum"] >= 0, "invalid checksum")
        timings = item["median_ns"]
        require(set(timings) == TIMING_KEYS, "timing phase set drift")
        require(all(isinstance(value, int) and value >= 0
                    for value in timings.values()),
                "invalid timing value")
        require(
            timings["transfer_gpu"] ==
            timings["upload_gpu"] + timings["download_gpu"],
            "transfer timing is not upload plus download",
        )

    for key in ("execution_only", "end_to_end"):
        crossover = document["crossover"][key]
        if crossover["observed"]:
            require(crossover["first_value_count"] in EXPECTED_SIZES,
                    f"{key} crossover not in workload ladder")
        else:
            require(crossover["first_value_count"] is None,
                    f"{key} unobserved crossover must be null")


def deterministic(document: dict) -> dict:
    result = copy.deepcopy(document)
    for workload in result["workloads"]:
        workload.pop("median_ns", None)
    result.pop("crossover", None)
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
            "deterministic timing evidence differs outside measurements",
        )
    print(f"validated {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
