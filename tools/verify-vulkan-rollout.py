#!/usr/bin/env python3
import argparse
import hashlib
import io
import json
import subprocess
import tarfile
import re
import struct
from pathlib import Path


def version(path: Path) -> tuple[int, int, int]:
    text = path.read_text(encoding="utf-8").strip()
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", text)
    if not match:
        raise SystemExit(f"invalid version: {path}: {text!r}")
    return tuple(int(part) for part in match.groups())


def bmp_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 26 or data[:2] != b"BM":
        raise SystemExit(f"invalid BMP capture: {path}")
    width, height = struct.unpack_from("<ii", data, 18)
    if width <= 0 or height == 0:
        raise SystemExit(f"invalid BMP dimensions: {path}: {width}x{height}")
    return width, abs(height)


# Verify the committed module without importing the shared workspace's dirty tree.
def verify_text_source(root: Path, canonical: Path) -> None:
    pin=json.loads((Path(__file__).parent / "text-render-source-pin.json").read_text())
    commit=pin["commit"]
    if not re.fullmatch(r"[0-9a-f]{40}",commit): raise SystemExit("invalid text source pin")
    raw=subprocess.check_output(["git","-C",str(canonical),"archive",commit,"kit/kit_render"])
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        names=set()
        for item in archive.getmembers():
            if not item.isfile(): continue
            names.add(item.name)
            path=root/item.name
            if not path.is_file() or path.is_symlink() or path.read_bytes()!=archive.extractfile(item).read():
                raise SystemExit("text source mismatch: "+item.name)
            if hashlib.sha256(path.read_bytes()).hexdigest()!=pin["files"].get(item.name):
                raise SystemExit("text pin manifest mismatch: "+item.name)
    if names!=set(pin["files"]): raise SystemExit("text pin manifest file mismatch")
    print("Carta text source contract: "+commit+" files="+str(len(names)))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--canonical-shared-root", type=Path)
    parser.add_argument("--shared-root", type=Path, required=True)
    parser.add_argument("--initial-capture", type=Path)
    parser.add_argument("--resized-capture", type=Path)
    args = parser.parse_args()

    root = args.shared_root.resolve()
    canonical=args.canonical_shared_root
    if canonical is None:
        workspace=next((p for p in Path(__file__).resolve().parents if (p/"bin/shared_archive_sync.py").is_file()), None)
        if workspace is None: raise SystemExit("canonical shared source is required")
        canonical=workspace/"shared"
    verify_text_source(root, canonical)
    runtime = root / "vk_runtime"
    renderer = root / "vk_renderer"
    required = [
        runtime / "include/vk_runtime.h",
        runtime / "VERSION",
        renderer / "include/vk_renderer.h",
        renderer / "include/vk_renderer_device.h",
        renderer / "VERSION",
    ]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise SystemExit("missing Vulkan rollout inputs: " + ", ".join(missing))

    runtime_version = version(runtime / "VERSION")
    renderer_version = version(renderer / "VERSION")
    if runtime_version < (0, 6, 0):
        raise SystemExit(f"vk_runtime >= 0.6.0 required, found {runtime_version}")
    if renderer_version < (1, 3, 1):
        raise SystemExit(f"vk_renderer >= 1.3.1 required, found {renderer_version}")

    if args.initial_capture or args.resized_capture:
        if not args.initial_capture or not args.resized_capture:
            raise SystemExit("both capture paths are required")
        initial_size = bmp_size(args.initial_capture)
        resized_size = bmp_size(args.resized_capture)
        if initial_size == resized_size:
            raise SystemExit(f"capture dimensions did not change: {initial_size}")
        print(
            "map_forge Vulkan captures: "
            f"initial={initial_size[0]}x{initial_size[1]} "
            f"resized={resized_size[0]}x{resized_size[1]}"
        )

    print(
        "map_forge Vulkan rollout contract: "
        f"shared_root={root} vk_runtime={'.'.join(map(str, runtime_version))} "
        f"vk_renderer={'.'.join(map(str, renderer_version))}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
