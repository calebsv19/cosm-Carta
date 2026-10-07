# Carta packaging

`release-output-root-conformance` creates an absent job or bound target beneath
the source `build/release-authenticated` parent or the configured
`CODEWORK_DATA_ROOT/map_forge/build/release-authenticated` parent. Absolute data
roots and relative source roots use the same traversal, symlink, occupancy, and
identity checks. The app bundle is built under the selected root's `package/`
directory beneath freshly created `scratch/`. After archive, checksum and
manifest readback, finalization retains exactly the ZIP and its two sidecars
at the selected root and removes that new scratch directory. Failed scratch is
preserved. Checksum and manifest rows reference final basenames.

Run `python3 tools/packaging/test_release_root.py` for isolated ordinary creation
and rejection checks. This package lane does not replace a Desktop app or copy
personal runtime/history/region payloads.

Package self-test and release configuration audit run the launcher under a
temporary HOME/runtime, so Vulkan support files, theme state and launcher logs
stay outside the user's Carta runtime. Only those test subprocesses use the
temporary HOME; signing and notarization keep the established keychain session.
