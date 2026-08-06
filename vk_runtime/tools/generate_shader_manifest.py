#!/usr/bin/env python3
"""Generate a deterministic CodeWork shader identity manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kernel-id", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--spirv", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--compiler", default="glslc")
    parser.add_argument("--flag", action="append", default=[])
    args = parser.parse_args()

    source = pathlib.Path(args.source)
    spirv = pathlib.Path(args.spirv)
    output = pathlib.Path(args.output)
    compiler_version = subprocess.run(
        [args.compiler, "--version"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()[0]
    source_digest = hashlib.sha256(source.read_bytes()).hexdigest()
    spirv_digest = hashlib.sha256(spirv.read_bytes()).hexdigest()
    document = {
        "compiler": pathlib.Path(args.compiler).name,
        "compiler_version": compiler_version,
        "descriptor_layout": [
            {
                "binding": 0,
                "descriptor_type": "storage_buffer",
                "access": "readonly",
            },
            {
                "binding": 1,
                "descriptor_type": "storage_buffer",
                "access": "writeonly",
            },
        ],
        "entry_point": "main",
        "flags": args.flag,
        "kernel_id": args.kernel_id,
        "local_size": [64, 1, 1],
        "schema": "codework_vk_shader_manifest_v1",
        "schema_version": 1,
        "source": source.as_posix(),
        "source_sha256": source_digest,
        "spirv": spirv.as_posix(),
        "spirv_sha256": spirv_digest,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
