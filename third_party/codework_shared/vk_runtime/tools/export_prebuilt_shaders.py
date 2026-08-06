#!/usr/bin/env python3
"""Export deterministic, source-bound SPIR-V artifacts for compiler-free hosts."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import sys

sys.dont_write_bytecode = True


KERNELS = (
    ("codework_f32_transform_v1", "f32_transform"),
    ("codework_u32_transform_v1", "u32_transform"),
)


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_atomic(source: pathlib.Path, destination: pathlib.Path) -> None:
    temporary = destination.with_name(f".{destination.name}.tmp")
    shutil.copyfile(source, temporary)
    os.replace(temporary, destination)


def write_json_atomic(path: pathlib.Path, document: dict) -> None:
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    os.replace(temporary, path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", default=".")
    parser.add_argument("--compiled-dir", default="build/shaders")
    parser.add_argument("--output-dir", default="prebuilt/shaders")
    parser.add_argument(
        "--validator",
        default="tests/validate_shader_manifest.py",
    )
    args = parser.parse_args()

    source_root = pathlib.Path(args.source_root)
    compiled_dir = pathlib.Path(args.compiled_dir)
    output_dir = pathlib.Path(args.output_dir)
    validator_path = pathlib.Path(args.validator)
    if compiled_dir.resolve() == output_dir.resolve():
        raise SystemExit("compiled and prebuilt output directories must differ")

    sys.path.insert(0, str(validator_path.parent.resolve()))
    from validate_shader_manifest import validate_manifest

    output_dir.mkdir(parents=True, exist_ok=True)
    artifacts = []
    for kernel_id, stem in KERNELS:
        source = source_root / "shaders" / f"{stem}.comp"
        spirv = compiled_dir / f"{stem}.spv"
        shader_manifest = compiled_dir / f"{stem}.manifest.json"
        try:
            document = validate_manifest(
                shader_manifest,
                source_override=source,
                spirv_override=spirv,
                expected_kernel_id=kernel_id,
                require_source_sha256=True,
            )
        except (OSError, ValueError, json.JSONDecodeError) as error:
            raise SystemExit(f"{stem}: {error}") from error

        exported_spirv = output_dir / spirv.name
        exported_manifest = output_dir / shader_manifest.name
        copy_atomic(spirv, exported_spirv)
        copy_atomic(shader_manifest, exported_manifest)
        artifacts.append(
            {
                "kernel_id": kernel_id,
                "shader_manifest": exported_manifest.name,
                "shader_manifest_sha256": digest(exported_manifest),
                "source": pathlib.PurePosixPath(document["source"]).as_posix(),
                "source_sha256": document["source_sha256"],
                "spirv": exported_spirv.name,
                "spirv_sha256": digest(exported_spirv),
            }
        )

    bundle = {
        "artifacts": artifacts,
        "schema": "codework_vk_prebuilt_shader_bundle_v1",
        "schema_version": 1,
    }
    write_json_atomic(output_dir / "bundle.manifest.json", bundle)
    print(f"exported {len(artifacts)} shaders to {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
