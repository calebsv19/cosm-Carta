"""Exercise Carta job-root creation and rejection without production paths."""
from pathlib import Path
import tempfile
import unittest
import hashlib
from prepare_release_root import prepare_root
from finalize_release_root import finalize_root


class ReleaseRootTests(unittest.TestCase):
    def test_finalize_exact_evidence_and_preserve_failure_scratch(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp).resolve()
            scratch = root / "scratch"
            scratch.mkdir()
            basename = "Carta-0.4.0-macOS-arm64-stable"
            archive = scratch / (basename + ".zip")
            archive.write_bytes(b"test archive")
            checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
            sidecar = scratch / (basename + ".zip.sha256")
            sidecar.write_text(checksum + "  " + archive.name + "\n")
            manifest = scratch / (basename + ".manifest.txt")
            manifest.write_text("sha256=invalid\n")
            with self.assertRaises(ValueError):
                finalize_root(root, basename)
            self.assertTrue(scratch.is_dir())
            self.assertEqual(list(root.iterdir()), [scratch])
            manifest.write_text(f"sha256={checksum}\nartifact={archive.name}\nzip={archive.name}\nsha256_file={sidecar.name}\n")
            (scratch / "audit.txt").write_text("task scratch")
            finalize_root(root, basename)
            self.assertEqual({p.name for p in root.iterdir()}, {
                basename + suffix for suffix in (".zip", ".zip.sha256", ".manifest.txt")})

    def test_source_and_data_targets(self):
        with tempfile.TemporaryDirectory() as temp:
            source, data = Path(temp).resolve() / "source", Path(temp).resolve() / "data"
            source.mkdir()
            for value in ("build/release-authenticated/job_one",
                          str(data / "map_forge/build/release-authenticated/job_two/targets/target_one")):
                root = prepare_root(value, source, data, create=False)
                self.assertFalse(root.exists())
                self.assertEqual(prepare_root(value, source, data), root)
                self.assertTrue(root.is_dir())
                with self.assertRaises(ValueError):
                    prepare_root(value, source, data)

    def test_reject_escape_symlink_and_non_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            source, data = Path(temp).resolve() / "source", Path(temp).resolve() / "data"
            source.mkdir()
            for value in ("build/release-authenticated/job/../escape", "/tmp/job_escape",
                          "build/release-authenticated/job/unknown/target", "build//release-authenticated/job"):
                with self.assertRaises(ValueError):
                    prepare_root(value, source, data)
            (source / "build").symlink_to(data, target_is_directory=True)
            with self.assertRaises(ValueError):
                prepare_root("build/release-authenticated/job_one", source, data)
            (source / "build").unlink()
            (source / "build").write_text("not a directory")
            with self.assertRaises(ValueError):
                prepare_root("build/release-authenticated/job_one", source, data)


if __name__ == "__main__":
    unittest.main()
