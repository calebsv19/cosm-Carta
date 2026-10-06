#!/usr/bin/env python3
"""Shared build-operations helpers for the MEW1 development-lane contract."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Iterable


IDENTITY_SCHEMA = "codework_local_development_build_identity_v1"
LANE_STATE_SCHEMA = "codework_mew1_lane_state_v1"
HEX_SHA256 = re.compile(r"^[0-9a-f]{64}$")


class Mew1Error(RuntimeError):
    """A bounded MEW1 readback or identity operation failed."""


def run_bytes(argv: list[str], *, check: bool = True) -> bytes:
    try:
        completed = subprocess.run(
            argv,
            check=check,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except OSError as error:
        raise Mew1Error(f"unable to run {argv[0]}: {error}") from error
    except subprocess.CalledProcessError as error:
        detail = error.stderr.decode("utf-8", "replace").strip()
        raise Mew1Error(detail or f"command failed: {' '.join(argv)}") from error
    return completed.stdout


def git(repo: Path, *args: str) -> bytes:
    return run_bytes(["git", "-C", str(repo), *args])


def update_record(digest: "hashlib._Hash", label: bytes, payload: bytes) -> None:
    digest.update(label)
    digest.update(b"\0")
    digest.update(str(len(payload)).encode("ascii"))
    digest.update(b"\0")
    digest.update(payload)
    digest.update(b"\0")


def source_state_fingerprint(repo: Path) -> str:
    repo = repo.resolve()
    digest = hashlib.sha256()
    update_record(digest, b"head", git(repo, "rev-parse", "HEAD").strip())
    update_record(
        digest,
        b"tracked_diff",
        git(repo, "diff", "--binary", "--no-ext-diff", "HEAD", "--"),
    )
    update_record(digest, b"submodules", git(repo, "submodule", "status", "--recursive"))
    untracked = git(repo, "ls-files", "--others", "--exclude-standard", "-z").split(b"\0")

    for encoded_path in sorted(path for path in untracked if path):
        relative = os.fsdecode(encoded_path)
        path = repo / relative
        update_record(digest, b"untracked_path", encoded_path)
        try:
            if path.is_symlink():
                update_record(digest, b"untracked_symlink", os.fsencode(os.readlink(path)))
            elif path.is_file():
                update_record(digest, b"untracked_file", path.read_bytes())
            else:
                update_record(digest, b"untracked_other", b"")
        except OSError as error:
            raise Mew1Error(f"unable to fingerprint {relative}: {error}") from error
    return digest.hexdigest()


def git_text(repo: Path, *args: str) -> str:
    return git(repo, *args).decode("utf-8", "strict").strip()


def branch_name(repo: Path) -> str:
    output = subprocess.run(
        ["git", "-C", str(repo), "symbolic-ref", "--quiet", "--short", "HEAD"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if output.returncode == 0:
        return output.stdout.strip()
    return "detached"


def status_paths(repo: Path) -> list[str]:
    raw = git(repo, "status", "--porcelain=v1", "-z", "--untracked-files=all")
    return [os.fsdecode(item) for item in raw.split(b"\0") if item]


def read_version(repo: Path, version_file: str) -> str:
    path = repo / version_file
    try:
        return path.read_text(encoding="utf-8").strip()
    except OSError as error:
        raise Mew1Error(f"unable to read version file {path}: {error}") from error


def lane_record(repo: Path, version_file: str) -> dict[str, Any]:
    paths = status_paths(repo)
    return {
        "root": str(repo.resolve()),
        "branch": branch_name(repo),
        "commit": git_text(repo, "rev-parse", "HEAD"),
        "version": read_version(repo, version_file),
        "clean": not paths,
        "status_paths": paths,
        "source_fingerprint_sha256": source_state_fingerprint(repo),
    }


def parse_worktree_inventory(repo: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    current: dict[str, Any] = {}
    for line in git_text(repo, "worktree", "list", "--porcelain").splitlines() + [""]:
        if not line:
            if current:
                records.append(current)
                current = {}
            continue
        key, _, value = line.partition(" ")
        if key in {"bare", "detached", "locked", "prunable"} and not value:
            current[key] = True
        elif key == "branch":
            current[key] = value.removeprefix("refs/heads/")
        else:
            current[key] = value
    return records


def derive_lane_state(canonical: dict[str, Any], main_edit: dict[str, Any],
                      canonical_only: int, main_edit_only: int) -> str:
    if not main_edit["clean"]:
        return "active_dirty"
    if canonical_only:
        return "canonical_drift"
    if main_edit_only:
        return "checkpointed"
    if canonical["commit"] == main_edit["commit"]:
        return "synced_clean"
    raise Mew1Error("unable to derive lane state from the observed topology")


def lane_state(canonical_root: Path, main_edit_root: Path,
               version_file: str) -> dict[str, Any]:
    canonical = lane_record(canonical_root.resolve(), version_file)
    main_edit = lane_record(main_edit_root.resolve(), version_file)
    counts = git_text(
        canonical_root.resolve(),
        "rev-list",
        "--left-right",
        "--count",
        f"{canonical['commit']}...{main_edit['commit']}",
    ).split()
    if len(counts) != 2:
        raise Mew1Error("git returned an invalid ahead/behind count")
    canonical_only, main_edit_only = (int(value) for value in counts)
    return {
        "schema_version": LANE_STATE_SCHEMA,
        "observed_at_utc": datetime.now(timezone.utc).isoformat(),
        "state": derive_lane_state(canonical, main_edit, canonical_only, main_edit_only),
        "canonical": canonical,
        "main_edit": main_edit,
        "divergence": {
            "canonical_only": canonical_only,
            "main_edit_only": main_edit_only,
        },
        "worktrees": parse_worktree_inventory(canonical_root.resolve()),
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError as error:
        raise Mew1Error(f"unable to hash {path}: {error}") from error
    return digest.hexdigest()


def write_identity(args: argparse.Namespace) -> dict[str, Any]:
    source_root = Path(args.source_root).resolve()
    dirty = bool(status_paths(source_root))
    payload = {
        "schema_version": args.schema_version,
        "package_class": "local_development",
        "profile": args.profile,
        "program": args.program,
        "product": args.product,
        "program_version": args.version,
        "architecture": args.architecture,
        "toolchain": args.toolchain,
        "source": {
            "branch": branch_name(source_root),
            "commit": git_text(source_root, "rev-parse", "HEAD"),
            "dirty": dirty,
            "fingerprint_sha256": source_state_fingerprint(source_root),
        },
        "build_label": args.build_label,
        "packaged_binary_sha256": sha256_file(Path(args.binary)),
        "built_at_utc": datetime.now(timezone.utc).isoformat(),
    }
    output = Path(args.output)
    try:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except OSError as error:
        raise Mew1Error(f"unable to write identity {output}: {error}") from error
    return payload


def require_string(payload: dict[str, Any], key: str, failures: list[str]) -> str:
    value = payload.get(key)
    if not isinstance(value, str) or not value:
        failures.append(key)
        return ""
    return value


def verify_identity(args: argparse.Namespace) -> dict[str, Any]:
    try:
        identity = json.loads(Path(args.identity).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise Mew1Error(f"unable to read identity: {error}") from error
    if not isinstance(identity, dict):
        raise Mew1Error("identity root must be an object")

    accepted = set(args.accepted_schema or [IDENTITY_SCHEMA])
    failures: list[str] = []
    schema = require_string(identity, "schema_version", failures)
    if schema not in accepted:
        failures.append("schema_version_accepted")
    for key in (
        "profile", "program", "product", "program_version", "architecture",
        "toolchain", "build_label", "built_at_utc",
    ):
        require_string(identity, key, failures)
    if identity.get("package_class") != "local_development":
        failures.append("package_class")
    source = identity.get("source")
    if not isinstance(source, dict):
        failures.append("source")
        source = {}
    for key in ("branch", "commit", "fingerprint_sha256"):
        require_string(source, key, failures)
    if not isinstance(source.get("dirty"), bool):
        failures.append("source.dirty")
    if not HEX_SHA256.fullmatch(str(source.get("fingerprint_sha256", ""))):
        failures.append("source.fingerprint_sha256")
    binary_digest = str(identity.get("packaged_binary_sha256", ""))
    if not HEX_SHA256.fullmatch(binary_digest):
        failures.append("packaged_binary_sha256")
    elif binary_digest != sha256_file(Path(args.binary)):
        failures.append("packaged_binary_sha256_match")

    expected = {
        "profile": args.profile,
        "program": args.program,
        "product": args.product,
        "program_version": args.version,
    }
    for key, value in expected.items():
        if value is not None and identity.get(key) != value:
            failures.append(f"expected_{key}")

    if args.source_root:
        source_root = Path(args.source_root).resolve()
        current = {
            "branch": branch_name(source_root),
            "commit": git_text(source_root, "rev-parse", "HEAD"),
            "dirty": bool(status_paths(source_root)),
            "fingerprint_sha256": source_state_fingerprint(source_root),
        }
        for key, value in current.items():
            if source.get(key) != value:
                failures.append(f"source.{key}_match")

    if failures:
        raise Mew1Error("build identity verification failed: " + ", ".join(sorted(set(failures))))
    return {"schema_version": schema, "status": "ok"}


def process_rows() -> tuple[list[tuple[int, int, str]], set[int]]:
    output = run_bytes(["ps", "-axo", "pid=,ppid=,comm="]).decode("utf-8", "replace")
    rows: list[tuple[int, int, str]] = []
    parents: dict[int, int] = {}
    for line in output.splitlines():
        fields = line.strip().split(None, 2)
        if len(fields) != 3 or not fields[0].isdigit() or not fields[1].isdigit():
            continue
        pid, ppid = int(fields[0]), int(fields[1])
        rows.append((pid, ppid, fields[2]))
        parents[pid] = ppid
    excluded: set[int] = set()
    current = os.getpid()
    while current and current not in excluded:
        excluded.add(current)
        current = parents.get(current, 0)
    return rows, excluded


def process_audit(match: str, path: Path | None, limit: int) -> dict[str, Any]:
    if len(match.strip()) < 3:
        raise Mew1Error("process match must contain at least three non-space characters")
    try:
        rows, excluded = process_rows()
    except Mew1Error as error:
        raise Mew1Error(f"process audit requires a host_required environment: {error}") from error
    all_matches = [
        {"pid": pid, "executable": executable}
        for pid, _ppid, executable in rows
        if pid not in excluded and match in executable
    ]
    truncated = len(all_matches) > limit
    matches = all_matches[:limit]
    method = "ps_literal"
    if not matches and path is not None:
        completed = subprocess.run(
            ["lsof", "-Fpcn", "--", str(path.resolve())],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if completed.returncode not in (0, 1):
            raise Mew1Error(completed.stderr.strip() or "lsof process audit failed")
        lsof_records: list[dict[str, Any]] = []
        current: dict[str, Any] = {}
        for line in completed.stdout.splitlines():
            if line.startswith("p"):
                if current:
                    lsof_records.append(current)
                current = {"pid": int(line[1:])}
            elif line.startswith("c"):
                current["executable"] = line[1:]
            elif line.startswith("n"):
                current["path"] = line[1:]
        if current:
            lsof_records.append(current)
        all_matches = [record for record in lsof_records if record.get("pid") not in excluded]
        truncated = len(all_matches) > limit
        matches = all_matches[:limit]
        method = "lsof_path_fallback"
    return {
        "schema_version": "codework_mew1_process_audit_v1",
        "match": match,
        "path": str(path.resolve()) if path is not None else None,
        "method": method,
        "running": bool(matches),
        "matches": matches,
        "truncated": truncated,
    }


def add_identity_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--binary", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--program", required=True)
    parser.add_argument("--product", required=True)
    parser.add_argument("--version", required=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    fingerprint = commands.add_parser("fingerprint", help="hash a complete visible Git source state")
    fingerprint.add_argument("--repo", required=True)

    state = commands.add_parser("lane-state", help="read canonical/main-edit topology as JSON")
    state.add_argument("--canonical-root", required=True)
    state.add_argument("--main-edit-root", required=True)
    state.add_argument("--version-file", default="VERSION")

    writer = commands.add_parser("write-identity", help="write a local-development build identity")
    writer.add_argument("--output", required=True)
    writer.add_argument("--source-root", required=True)
    add_identity_arguments(writer)
    writer.add_argument("--architecture", required=True)
    writer.add_argument("--toolchain", required=True)
    writer.add_argument("--build-label", required=True)
    writer.add_argument("--schema-version", default=IDENTITY_SCHEMA)

    verifier = commands.add_parser("verify-identity", help="verify identity, source, and binary bytes")
    verifier.add_argument("--identity", required=True)
    verifier.add_argument("--source-root")
    add_identity_arguments(verifier)
    verifier.add_argument("--accepted-schema", action="append")

    processes = commands.add_parser("process-audit", help="perform a bounded read-only process audit")
    processes.add_argument("--match", required=True)
    processes.add_argument("--path")
    processes.add_argument("--limit", type=int, default=64)
    return parser


def emit(payload: Any) -> None:
    if isinstance(payload, str):
        print(payload)
    else:
        print(json.dumps(payload, indent=2, sort_keys=True))


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "fingerprint":
            emit(source_state_fingerprint(Path(args.repo)))
        elif args.command == "lane-state":
            emit(lane_state(Path(args.canonical_root), Path(args.main_edit_root), args.version_file))
        elif args.command == "write-identity":
            emit(write_identity(args))
        elif args.command == "verify-identity":
            emit(verify_identity(args))
        elif args.command == "process-audit":
            if args.limit < 1 or args.limit > 256:
                raise Mew1Error("process limit must be between 1 and 256")
            emit(process_audit(args.match, Path(args.path) if args.path else None, args.limit))
    except Mew1Error as error:
        print(f"mew1: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
