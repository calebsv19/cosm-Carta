#!/usr/bin/env python3
"""Validate the deterministic S1 capability-report invariants."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


SCHEMA = "codework_gpu_capability_report_v1"
MODULE_VERSION = (
    Path(__file__).resolve().parents[1] / "VERSION"
).read_text(encoding="utf-8").strip()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def require_keys(value: dict[str, Any], keys: set[str], context: str) -> None:
    missing = sorted(keys - value.keys())
    require(not missing, f"{context}: missing keys {missing}")


def validate_device(device: dict[str, Any], index: int) -> None:
    context = f"devices[{index}]"
    require_keys(
        device,
        {
            "name",
            "device_uuid",
            "vendor_id",
            "device_id",
            "type",
            "api_version",
            "driver_version",
            "driver_name",
            "driver_info",
            "selected",
            "suitable",
            "rejection_bits",
            "rejections",
            "selected_queues",
            "features",
            "subgroup",
            "queue_families",
            "memory_heaps",
            "extensions",
        },
        context,
    )
    require(isinstance(device["name"], str) and device["name"], f"{context}: name")
    require(
        isinstance(device["device_uuid"], str)
        and len(device["device_uuid"]) == 32,
        f"{context}: device_uuid",
    )
    require(
        device["extensions"] == sorted(device["extensions"]),
        f"{context}: extensions are not sorted",
    )
    queue_families = device["queue_families"]
    require(
        [queue["index"] for queue in queue_families]
        == list(range(len(queue_families))),
        f"{context}: queue-family indices are not canonical",
    )
    for role in ("graphics", "compute", "transfer"):
        selected = device["selected_queues"][role]
        require(
            selected is None
            or (isinstance(selected, int) and 0 <= selected < len(queue_families)),
            f"{context}: invalid {role} queue selection",
        )
    require(
        isinstance(device["rejection_bits"], int)
        and isinstance(device["rejections"], list),
        f"{context}: rejection contract",
    )


def validate_report(report: dict[str, Any], require_validation: bool) -> None:
    require_keys(
        report,
        {
            "schema",
            "schema_version",
            "module_version",
            "platform",
            "architecture",
            "compiler",
            "vulkan",
            "validation",
            "result",
            "devices",
        },
        "report",
    )
    require(report["schema"] == SCHEMA, "report: schema")
    require(report["schema_version"] == 1, "report: schema_version")
    require(report["module_version"] == MODULE_VERSION, "report: module_version")
    require(report["result"]["status"] == "ok", "report: runtime status")
    require(report["result"]["vulkan_result"] == 0, "report: vulkan_result")

    devices = report["devices"]
    require(isinstance(devices, list) and devices, "report: no devices")
    for index, device in enumerate(devices):
        validate_device(device, index)

    canonical_device_keys = [
        (
            device["vendor_id"],
            device["device_id"],
            device["name"],
            device["device_uuid"],
        )
        for device in devices
    ]
    require(
        canonical_device_keys == sorted(canonical_device_keys),
        "report: devices are not sorted",
    )

    selected_index = report["result"]["selected_device_index"]
    require(
        isinstance(selected_index, int) and 0 <= selected_index < len(devices),
        "report: selected_device_index",
    )
    require(devices[selected_index]["selected"], "report: selected flag")
    require(devices[selected_index]["suitable"], "report: selected suitability")
    require(
        sum(bool(device["selected"]) for device in devices) == 1,
        "report: exactly one device must be selected",
    )

    validation = report["validation"]
    require_keys(
        validation,
        {
            "requested",
            "available",
            "enabled",
            "load_failed",
            "debug_utils_available",
            "warnings",
            "errors",
        },
        "validation",
    )
    require(validation["errors"] == 0, "validation: errors")
    if require_validation:
        require(validation["requested"], "validation: not requested")
        require(validation["available"], "validation: unavailable")
        require(validation["enabled"], "validation: not enabled")
        require(not validation["load_failed"], "validation: load failed")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("--require-validation", action="store_true")
    args = parser.parse_args()

    with args.report.open("r", encoding="utf-8") as handle:
        report = json.load(handle)
    validate_report(report, args.require_validation)
    print(f"validated {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
