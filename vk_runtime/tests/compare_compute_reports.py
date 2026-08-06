#!/usr/bin/env python3
"""Validate and compare S2 compute reports while excluding measured timing."""

from __future__ import annotations

import argparse
import json
import math
import pathlib

MODULE_VERSION = (
    pathlib.Path(__file__).resolve().parents[1] / "VERSION"
).read_text(encoding="utf-8").strip()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def validate(document: dict) -> None:
    require(
        document.get("schema") == "codework_gpu_compute_report_v1",
        "unexpected compute report schema",
    )
    require(document.get("schema_version") == 1, "unexpected schema version")
    require(document.get("module_version") == MODULE_VERSION,
            "unexpected module")
    require(document.get("runtime_status") == "ok", "runtime did not pass")
    require(document.get("validation_errors") == 0, "validation errors present")
    require(document.get("validation_warnings") == 0,
            "validation warnings present")
    require(len(document["device_identity"]["uuid"]) == 32,
            "device UUID missing")
    require(document["device_identity"]["vendor_id"] >= 0,
            "invalid vendor id")
    require(document["device_identity"]["device_id"] >= 0,
            "invalid device id")
    require(document["negative_fixtures"] == {
        "binding": "descriptor_binding_invalid",
        "shader": "shader_code_invalid",
    }, "negative fixtures did not produce typed failures")
    require(document["u32"]["parity"], "u32 parity failed")
    require(document["f32"]["parity"], "f32 parity failed")
    require(document["f32"]["policy"] == {
        "absolute_tolerance": 1e-6,
        "exceptional_values": "finite_only",
        "relative_tolerance": 1e-6,
    }, "float comparison policy drift")
    require(all(value >= 0 for value in document["timing_ns"].values()),
            "negative timing")
    require(math.isfinite(document["f32"]["max_absolute_error"]),
            "non-finite float error")


def without_timing(document: dict) -> dict:
    result = dict(document)
    result.pop("timing_ns", None)
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report")
    parser.add_argument("--compare")
    args = parser.parse_args()
    first = json.loads(pathlib.Path(args.report).read_text(encoding="utf-8"))
    validate(first)
    if args.compare:
        second = json.loads(
            pathlib.Path(args.compare).read_text(encoding="utf-8")
        )
        validate(second)
        require(
            without_timing(first) == without_timing(second),
            "deterministic compute evidence differs outside timing",
        )
    print(f"validated {args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
