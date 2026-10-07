#!/usr/bin/env python3
"""Validate and create an absent Carta release job directory."""
import argparse
import os
from pathlib import Path
import re


def prepare_root(value, source_root, data_root, *, create=True):
    """Accept only a fresh job or bound target below source/data release parents."""
    if not value or any(c in value for c in "\n\r\t"):
        raise ValueError("RELEASE_ROOT must be a nonempty path")
    if any(p in {".", ".."} for p in value.split("/")) or "//" in value or value.endswith("/"):
        raise ValueError("RELEASE_ROOT must not contain traversal or empty segments")
    source_root = Path(source_root).resolve()
    path = Path(value)
    if not path.is_absolute():
        path = source_root / path
    bases = (source_root / "build/release-authenticated",
             Path(data_root).absolute() / "map_forge/build/release-authenticated")
    relative = next((path.relative_to(base) for base in bases if path.is_relative_to(base)), None)
    if relative is None:
        raise ValueError("RELEASE_ROOT is outside the source/data release parents")
    parts = relative.parts
    if not (len(parts) == 1 or (len(parts) == 3 and parts[1] == "targets")):
        raise ValueError("RELEASE_ROOT must name a job or its bound package target")
    if any(not re.fullmatch(r"[A-Za-z0-9_-]{3,128}", p) for p in parts):
        raise ValueError("RELEASE_ROOT job/target identity is invalid")
    for ancestor in (*reversed(path.parents), path):
        if ancestor.is_symlink():
            raise ValueError("RELEASE_ROOT must not traverse a symlink")
        if ancestor.exists() and not ancestor.is_dir():
            raise ValueError("RELEASE_ROOT ancestor is not a directory")
    if path.exists():
        raise ValueError("RELEASE_ROOT must be absent before packaging")
    if create:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.mkdir()
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True)
    parser.add_argument("--source-root", default=os.getcwd())
    parser.add_argument("--data-root", default=os.environ.get("CODEWORK_DATA_ROOT", str(Path.home() / "CodeWorkData")))
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    print(prepare_root(args.output, args.source_root, args.data_root, create=not args.validate_only))


if __name__ == "__main__":
    main()
