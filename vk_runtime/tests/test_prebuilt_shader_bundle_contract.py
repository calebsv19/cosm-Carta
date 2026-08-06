#!/usr/bin/env python3
"""Exercise positive and tamper-rejection paths for prebuilt shader bundles."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile


def run_validator(
    validator: pathlib.Path,
    bundle_dir: pathlib.Path,
    source_root: pathlib.Path,
    *,
    expected_fragment: str | None = None,
) -> None:
    result = subprocess.run(
        [
            sys.executable,
            str(validator),
            str(bundle_dir),
            "--source-root",
            str(source_root),
        ],
        capture_output=True,
        text=True,
    )
    output = result.stdout + result.stderr
    if expected_fragment is None:
        if result.returncode != 0:
            raise SystemExit(f"valid bundle rejected:\n{output}")
        return
    if result.returncode == 0:
        raise SystemExit(f"tampered bundle accepted: {expected_fragment}")
    if expected_fragment not in output:
        raise SystemExit(
            f"unexpected rejection for {expected_fragment!r}:\n{output}"
        )


def copy_fixture(
    temporary_root: pathlib.Path,
    source_root: pathlib.Path,
    bundle_dir: pathlib.Path,
) -> tuple[pathlib.Path, pathlib.Path]:
    fixture_root = temporary_root / "fixture"
    fixture_bundle = fixture_root / "prebuilt" / "shaders"
    (fixture_root / "shaders").mkdir(parents=True)
    shutil.copytree(bundle_dir, fixture_bundle)
    for source in ("f32_transform.comp", "u32_transform.comp"):
        shutil.copyfile(
            source_root / "shaders" / source,
            fixture_root / "shaders" / source,
        )
    return fixture_root, fixture_bundle


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle-dir", default="prebuilt/shaders")
    parser.add_argument("--source-root", default=".")
    parser.add_argument(
        "--validator",
        default="tests/validate_prebuilt_shader_bundle.py",
    )
    args = parser.parse_args()

    bundle_dir = pathlib.Path(args.bundle_dir).resolve()
    source_root = pathlib.Path(args.source_root).resolve()
    validator = pathlib.Path(args.validator).resolve()
    run_validator(validator, bundle_dir, source_root)

    with tempfile.TemporaryDirectory(prefix="vk-prebuilt-contract-") as temp:
        temporary_root = pathlib.Path(temp)

        fixture_root, fixture_bundle = copy_fixture(
            temporary_root / "source-case",
            source_root,
            bundle_dir,
        )
        source = fixture_root / "shaders" / "u32_transform.comp"
        source.write_bytes(source.read_bytes() + b"\n")
        run_validator(
            validator,
            fixture_bundle,
            fixture_root,
            expected_fragment="bundle source digest mismatch",
        )

        fixture_root, fixture_bundle = copy_fixture(
            temporary_root / "spirv-case",
            source_root,
            bundle_dir,
        )
        spirv = fixture_bundle / "u32_transform.spv"
        data = bytearray(spirv.read_bytes())
        data[-1] ^= 0x01
        spirv.write_bytes(data)
        run_validator(
            validator,
            fixture_bundle,
            fixture_root,
            expected_fragment="bundle SPIR-V digest mismatch",
        )

        fixture_root, fixture_bundle = copy_fixture(
            temporary_root / "manifest-case",
            source_root,
            bundle_dir,
        )
        shader_manifest = fixture_bundle / "u32_transform.manifest.json"
        shader_manifest.write_bytes(shader_manifest.read_bytes() + b"\n")
        run_validator(
            validator,
            fixture_bundle,
            fixture_root,
            expected_fragment="shader manifest digest mismatch",
        )

    print("prebuilt shader bundle contract tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
