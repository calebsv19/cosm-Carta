from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


TOOL_PATH = Path(__file__).resolve().parents[1] / "mew1.py"
SPEC = importlib.util.spec_from_file_location("mew1", TOOL_PATH)
assert SPEC and SPEC.loader
MEW1 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MEW1)


def run(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(TOOL_PATH), *args],
        check=check,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def git(repo: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()


class GitFixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.canonical = root / "canonical"
        self.main_edit = root / "main_edit"
        self.canonical.mkdir()
        subprocess.check_call(["git", "-C", str(self.canonical), "init", "-b", "main"])
        git(self.canonical, "config", "user.name", "MEW1 Test")
        git(self.canonical, "config", "user.email", "mew1@example.invalid")
        (self.canonical / "VERSION").write_text("1.2.3\n", encoding="utf-8")
        (self.canonical / "source.txt").write_text("base\n", encoding="utf-8")
        git(self.canonical, "add", "VERSION", "source.txt")
        git(self.canonical, "commit", "-m", "base")
        git(
            self.canonical,
            "worktree", "add", "-b", "codex/test-main-edit", str(self.main_edit), "HEAD",
        )


class Mew1Tests(unittest.TestCase):
    def test_fingerprint_tracks_visible_source_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixture = GitFixture(Path(directory))
            initial = MEW1.source_state_fingerprint(fixture.canonical)
            (fixture.canonical / "untracked.txt").write_text("new\n", encoding="utf-8")
            changed = MEW1.source_state_fingerprint(fixture.canonical)
            self.assertNotEqual(initial, changed)
            self.assertEqual(changed, MEW1.source_state_fingerprint(fixture.canonical))

    def test_lane_state_classifies_clean_dirty_and_diverged_states(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixture = GitFixture(Path(directory))
            state = MEW1.lane_state(fixture.canonical, fixture.main_edit, "VERSION")
            self.assertEqual(state["state"], "synced_clean")
            self.assertEqual(state["divergence"], {"canonical_only": 0, "main_edit_only": 0})

            (fixture.main_edit / "source.txt").write_text("edit\n", encoding="utf-8")
            self.assertEqual(
                MEW1.lane_state(fixture.canonical, fixture.main_edit, "VERSION")["state"],
                "active_dirty",
            )
            git(fixture.main_edit, "add", "source.txt")
            git(fixture.main_edit, "commit", "-m", "edit")
            self.assertEqual(
                MEW1.lane_state(fixture.canonical, fixture.main_edit, "VERSION")["state"],
                "checkpointed",
            )
            (fixture.canonical / "canonical.txt").write_text("drift\n", encoding="utf-8")
            git(fixture.canonical, "add", "canonical.txt")
            git(fixture.canonical, "commit", "-m", "canonical drift")
            self.assertEqual(
                MEW1.lane_state(fixture.canonical, fixture.main_edit, "VERSION")["state"],
                "canonical_drift",
            )

    def test_identity_round_trip_and_binary_tamper_rejection(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixture = GitFixture(Path(directory))
            binary = fixture.root / "program-bin"
            identity = fixture.root / "identity.json"
            binary.write_bytes(b"program-v1")
            common = (
                "--binary", str(binary), "--profile", "main-edit",
                "--program", "test_program", "--product", "Test Main Edit",
                "--version", "1.2.3",
            )
            run(
                "write-identity", "--output", str(identity),
                "--source-root", str(fixture.canonical), *common,
                "--architecture", "test-arch", "--toolchain", "test-toolchain",
                "--build-label", "main-edit-1.2.3-test",
            )
            verified = run(
                "verify-identity", "--identity", str(identity),
                "--source-root", str(fixture.canonical), *common,
            )
            self.assertEqual(json.loads(verified.stdout)["status"], "ok")
            binary.write_bytes(b"tampered")
            rejected = run(
                "verify-identity", "--identity", str(identity),
                "--source-root", str(fixture.canonical), *common, check=False,
            )
            self.assertNotEqual(rejected.returncode, 0)
            self.assertIn("packaged_binary_sha256_match", rejected.stderr)

    def test_compatibility_schema_requires_explicit_acceptance(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            fixture = GitFixture(Path(directory))
            binary = fixture.root / "program-bin"
            identity = fixture.root / "identity.json"
            binary.write_bytes(b"program-v1")
            common = (
                "--binary", str(binary), "--profile", "main-edit",
                "--program", "test_program", "--product", "Test Main Edit",
                "--version", "1.2.3",
            )
            run(
                "write-identity", "--output", str(identity),
                "--source-root", str(fixture.canonical), *common,
                "--architecture", "test-arch", "--toolchain", "test-toolchain",
                "--build-label", "main-edit-1.2.3-test",
                "--schema-version", "optic_local_development_build_identity_v1",
            )
            rejected = run(
                "verify-identity", "--identity", str(identity), *common, check=False,
            )
            self.assertNotEqual(rejected.returncode, 0)
            accepted = run(
                "verify-identity", "--identity", str(identity), *common,
                "--accepted-schema", "optic_local_development_build_identity_v1",
            )
            self.assertEqual(json.loads(accepted.stdout)["status"], "ok")

    def test_process_audit_is_bounded_and_read_only(self) -> None:
        rows = [(pid, 1, "/test/mew1-bounded-process") for pid in range(200, 205)]
        with mock.patch.object(MEW1, "process_rows", return_value=(rows, {123})):
            result = MEW1.process_audit("mew1-bounded-process", None, 4)
        self.assertTrue(result["running"])
        self.assertEqual(len(result["matches"]), 4)
        self.assertTrue(result["truncated"])
        self.assertEqual(result["method"], "ps_literal")


if __name__ == "__main__":
    unittest.main()
