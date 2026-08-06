#!/usr/bin/env python3
"""Validate the deterministic S2 shader identity contract."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def validate_manifest(
    path: pathlib.Path,
    *,
    source_override: pathlib.Path | None = None,
    spirv_override: pathlib.Path | None = None,
    expected_kernel_id: str | None = None,
    require_source_sha256: bool = False,
) -> dict:
    document = json.loads(path.read_text(encoding="utf-8"))

    require(
        document.get("schema") == "codework_vk_shader_manifest_v1",
        "unexpected shader manifest schema",
    )
    require(document.get("schema_version") == 1, "unexpected schema version")
    require(document.get("entry_point") == "main", "unexpected entry point")
    require(document.get("local_size") == [64, 1, 1], "unexpected local size")
    require(
        document.get("descriptor_layout")
        == [
            {
                "access": "readonly",
                "binding": 0,
                "descriptor_type": "storage_buffer",
            },
            {
                "access": "writeonly",
                "binding": 1,
                "descriptor_type": "storage_buffer",
            },
        ],
        "unexpected descriptor layout",
    )
    if expected_kernel_id is not None:
        require(
            document.get("kernel_id") == expected_kernel_id,
            "unexpected kernel id",
        )

    source = source_override or pathlib.Path(document["source"])
    source_digest = document.get("source_sha256")
    if require_source_sha256:
        require(
            isinstance(source_digest, str) and len(source_digest) == 64,
            "source SHA-256 is required",
        )
    if source_digest is not None:
        actual_source_digest = hashlib.sha256(source.read_bytes()).hexdigest()
        require(
            actual_source_digest == source_digest,
            "source digest mismatch",
        )

    spirv = spirv_override or pathlib.Path(document["spirv"])
    spirv_digest = hashlib.sha256(spirv.read_bytes()).hexdigest()
    require(
        spirv_digest == document.get("spirv_sha256"),
        "SPIR-V digest mismatch",
    )
    return document


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest")
    parser.add_argument("--source")
    parser.add_argument("--spirv")
    parser.add_argument("--kernel-id")
    parser.add_argument("--require-source-sha256", action="store_true")
    args = parser.parse_args()
    path = pathlib.Path(args.manifest)
    try:
        validate_manifest(
            path,
            source_override=pathlib.Path(args.source) if args.source else None,
            spirv_override=pathlib.Path(args.spirv) if args.spirv else None,
            expected_kernel_id=args.kernel_id,
            require_source_sha256=args.require_source_sha256,
        )
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(str(error)) from error
    print(f"validated {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
