# Scene Export Receipt v1

Status: stable shared publication contract

Schema family: `codework_scene_export`

Schema variant: `scene_export_receipt_v1`

Owner: `core_scene_compile`

## Boundary

`scene_authoring.json` is the editable, portable scene source exchanged between
authoring programs. `scene_runtime.json` is deterministic derived output and is
never an editable source. A LineDrawing layout JSON remains an app-private fast
load document; it is not a cross-program scene or a promoted runtime.

Compilation and publication are separate operations. Compilation binds source
and a canonical dependency manifest into deterministic runtime bytes.
Publication adds a receipt and makes all four files visible with one directory
rename.

## Published bundle

```text
<scene-bundle>/
  scene_authoring.json
  scene_runtime.json
  scene_dependencies.json
  scene_export_receipt.json
  dependencies/
    <dependency-kind>/
      <content-sha256>
```

Publication is create-only. If `<scene-bundle>` already exists, the publisher
fails without modifying it. The caller must select a new iteration name. Files
are written and synchronized in a sibling staging directory; the staging
directory is then renamed to the final path. Failed staging work is removed.

## Receipt fields

The receipt records:

- compiler name, semantic version, and normalization version;
- SHA-256 and byte count for authoring and runtime artifacts;
- dependency-manifest path, SHA-256, byte count, and dependency count;
- bundle SHA-256;
- publication mode and publication time.

The bundle digest is SHA-256 over this canonical UTF-8 input:

```text
authoring:<authoring sha256>
runtime:<runtime sha256>
dependencies:<dependency-set sha256>
compiler:<compiler version>
normalization:<normalization version>
```

Publication time is receipt evidence and is excluded from the bundle digest.
The runtime digest cannot be embedded in runtime JSON because that would create
a self-referential digest.

## Dependency digest

The compiler builds or validates `scene_dependency_manifest_v2`. Each entry
contains a dependency kind, stable identity, content SHA-256, byte count, and
the exact derived bundle-relative payload path
`dependencies/<kind>/<sha256>`.
Entries are sorted by kind and identity and duplicate kind/identity pairs are
rejected. The SHA-256 of the complete canonical manifest is the dependency
digest embedded in runtime and receipt provenance. Empty scenes still publish
an explicit empty manifest; the digest is therefore the digest of that
canonical document rather than SHA-256 of zero bytes.

Producers own discovery, content hashing, and retaining the exact dependency
bytes through the publication call. Shared compilation owns content-addressed
path derivation, staging writes, and payload verification. Dependency-kind
interpretation and mapping verified payloads into renderer or solver objects
remain consumer responsibilities.

## Consumer rules

- Load `scene_authoring.json` for editing.
- Call `core_scene_compile_verify_bundle` before accepting runtime input. It
  verifies all required artifacts, receipt metadata, manifest canonicality,
  runtime provenance, compiler compatibility, bundle identity, and every
  content-addressed payload's regular-file type, byte count, and digest through
  no-follow directory traversal.
- Supply an externally expected bundle digest when a scheduler, queue, or
  handoff receipt already binds one; self-consistency alone is not an
  authenticity claim.
- Reject unknown receipt schema variants or unsupported compiler versions.
- Do not infer promotion, simulation completion, or render readiness from a
  successful export receipt. Those require separate typed receipts.
