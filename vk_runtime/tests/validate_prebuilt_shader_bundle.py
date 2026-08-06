#!/usr/bin/env python3
"""Validate a deterministic compiler-free vk_runtime shader bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys

sys.dont_write_bytecode = True
from validate_shader_manifest import validate_manifest


EXPECTED_KERNELS = {
    "codework_f32_transform_v1": "f32_transform",
    "codework_u32_transform_v1": "u32_transform",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe_filename(value: object, field: str) -> str:
    require(isinstance(value, str) and value, f"{field} must be a string")
    path = pathlib.PurePosixPath(value)
    require(
        not path.is_absolute() and len(path.parts) == 1 and ".." not in path.parts,
        f"{field} must be one relative filename",
    )
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle_dir")
    parser.add_argument("--source-root", default=".")
    args = parser.parse_args()

    bundle_dir = pathlib.Path(args.bundle_dir)
    source_root = pathlib.Path(args.source_root)
    bundle_path = bundle_dir / "bundle.manifest.json"
    try:
        bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
        require(
            bundle.get("schema") == "codework_vk_prebuilt_shader_bundle_v1",
            "unexpected prebuilt bundle schema",
        )
        require(bundle.get("schema_version") == 1, "unexpected schema version")
        artifacts = bundle.get("artifacts")
        require(isinstance(artifacts, list), "artifacts must be an array")
        require(len(artifacts) == len(EXPECTED_KERNELS), "unexpected artifact count")

        seen = set()
        for artifact in artifacts:
            require(isinstance(artifact, dict), "artifact must be an object")
            kernel_id = artifact.get("kernel_id")
            require(kernel_id in EXPECTED_KERNELS, "unexpected kernel id")
            require(kernel_id not in seen, "duplicate kernel id")
            seen.add(kernel_id)
            stem = EXPECTED_KERNELS[kernel_id]
            require(
                artifact.get("source") == f"shaders/{stem}.comp",
                "unexpected source path",
            )
            source = source_root / artifact["source"]
            require(
                digest(source) == artifact.get("source_sha256"),
                "bundle source digest mismatch",
            )

            spirv_name = safe_filename(artifact.get("spirv"), "spirv")
            require(spirv_name == f"{stem}.spv", "unexpected SPIR-V filename")
            spirv = bundle_dir / spirv_name
            require(
                digest(spirv) == artifact.get("spirv_sha256"),
                "bundle SPIR-V digest mismatch",
            )

            manifest_name = safe_filename(
                artifact.get("shader_manifest"),
                "shader_manifest",
            )
            require(
                manifest_name == f"{stem}.manifest.json",
                "unexpected shader manifest filename",
            )
            shader_manifest = bundle_dir / manifest_name
            require(
                digest(shader_manifest)
                == artifact.get("shader_manifest_sha256"),
                "shader manifest digest mismatch",
            )
            document = validate_manifest(
                shader_manifest,
                source_override=source,
                spirv_override=spirv,
                expected_kernel_id=kernel_id,
                require_source_sha256=True,
            )
            require(
                document["source_sha256"] == artifact["source_sha256"],
                "source identity differs between manifests",
            )
            require(
                document["spirv_sha256"] == artifact["spirv_sha256"],
                "SPIR-V identity differs between manifests",
            )
        require(seen == set(EXPECTED_KERNELS), "missing kernel artifact")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        raise SystemExit(str(error)) from error

    print(f"validated {bundle_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
