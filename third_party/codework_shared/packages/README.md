# Shared Packages

This directory defines package metadata for standalone shared-library distribution.

## Purpose
Provide a stable packaging contract for `shared/*` modules that can be exported,
uploaded to VPS staging, verified, and published for website downloads.

## Layout
1. `catalog/modules.tsv`
- canonical module catalog (name, kind, shared path, starter flag, dependencies)

2. `schemas/package-manifest.schema.json`
- schema for per-package manifest files generated during export

3. `schemas/releases-index.schema.json`
- schema for the website-facing release index generated during export

4. `releases.json`
- tracked baseline index template; export scripts generate dated indexes under
  `release-export/shared-packages/<date>/releases.json`

5. `manifests/`
- optional checked-in manifest lane for curated/pinned package descriptors

6. `dist/`
- optional checked-in lane for curated static package payloads (default empty)

## Export / Upload Scripts
1. `bin/export_shared_packages.sh`
2. `bin/upload_shared_packages_to_vps.sh`

These scripts follow the same staged verification flow as app release artifacts:
export -> rsync to staging -> remote checksum verification -> publish.
