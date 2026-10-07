#!/usr/bin/env python3
"""Finalize exactly three verified Carta deliverables from fresh package scratch."""
import argparse
import hashlib
from pathlib import Path
import shutil


def finalize_root(root, basename):
    """Keep verified package evidence and remove only this root's new scratch."""
    root = Path(root)
    if not basename or Path(basename).name != basename or basename in {".", ".."}:
        raise ValueError("Invalid artifact basename")
    scratch = root / "scratch"
    if root.is_symlink() or scratch.is_symlink() or not scratch.is_dir():
        raise ValueError("Fresh package scratch must be an ordinary directory")
    if set(root.iterdir()) != {scratch}:
        raise ValueError("Release root already contains unrelated output")
    names = [basename + suffix for suffix in (".zip", ".zip.sha256", ".manifest.txt")]
    paths = [scratch / name for name in names]
    if any(path.is_symlink() or not path.is_file() for path in paths):
        raise ValueError("Package evidence is missing or symlinked")
    with paths[0].open("rb") as archive:
        archive_sha = hashlib.file_digest(archive, "sha256").hexdigest()
    if paths[1].read_text().split() != [archive_sha, names[0]]:
        raise ValueError("Package checksum does not bind archive bytes and basename")
    manifest = dict(line.split("=", 1) for line in paths[2].read_text().splitlines() if "=" in line)
    if any(manifest.get(key) != value for key, value in {
            "sha256": archive_sha, "artifact": names[0], "zip": names[0],
            "sha256_file": names[1]}.items()):
        raise ValueError("Package manifest does not bind final deliverables")
    for source, name in zip(paths, names):
        with source.open("rb") as src, (root / name).open("xb") as dst:
            shutil.copyfileobj(src, dst)
    for source, name in zip(paths, names):
        if source.read_bytes() != (root / name).read_bytes():
            raise ValueError("Finalized package evidence differs from scratch")
    shutil.rmtree(scratch)
    if {path.name for path in root.iterdir()} != set(names):
        raise ValueError("Finalized package root contains unexpected output")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True)
    parser.add_argument("--basename", required=True)
    args = parser.parse_args()
    finalize_root(args.root, args.basename)


if __name__ == "__main__":
    main()
