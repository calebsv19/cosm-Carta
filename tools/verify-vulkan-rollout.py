#!/usr/bin/env python3
import argparse
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shared-root", type=Path, required=True)
    parser.add_argument("--initial-capture", type=Path)
    parser.add_argument("--resized-capture", type=Path)
    args = parser.parse_args()

    root = args.shared_root.resolve()
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
