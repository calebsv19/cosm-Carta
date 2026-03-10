# core_pack Roadmap

## Mission
`core_pack` is the versioned, chunk-based container for ecosystem data interchange. It provides durable storage and efficient retrieval while allowing schema evolution without breaking existing data.

## North Star End State
- Stable `.pack` format used broadly for export/import/tooling workflows.
- Fast append and indexed random-access reads.
- Robust integrity and compatibility behavior across versions.
- Clean bridge between legacy data (VF2D and others) and canonical `core_data`-oriented payloads.

## Current Surface
- Public API: `include/core_pack.h`
- Implementation: `src/core_pack.c`, `src/core_pack_vf2d.c`
- CLI: `tools/pack_cli.c`
- Tests: `tests/core_pack_test.c`

## Product-Level Behavior Goals
- Reliable writer lifecycle (`open -> add chunks -> close`).
- Reader support for sequential, indexed, and partial read modes.
- Explicit endian handling and codec behavior (`none`, `rle8`).
- Backward-compatible handling for legacy VF2D conversion path.

## Capability Layers

### Layer 1: Format Stability and Validation
- Lock down v1 layout guarantees and documented chunk/index/footer semantics.
- Expand corruption/truncation tests for all reader paths.
- Strengthen CLI validation reporting for malformed files.

### Layer 2: Integrity and Metadata
- Add optional checksums/hashes per chunk.
- Add optional pack-level manifest metadata conventions.
- Expose validation APIs for tools (`validate`, `stats`, `integrity-report`).

### Layer 3: Codec and Storage Growth
- Add additional codecs where justified (keep codec contract explicit).
- Add chunk flags/attributes for codec and semantic metadata.
- Improve read performance for large packs via optimized indexing and selective decode.

### Layer 4: Multi-Frame and Interop Workflows
- Define conventions for sequence/timeline pack structures.
- Add helper APIs for frame catalogs and lookup by index/time tags.
- Add migration utilities for legacy and intermediate formats.

## Complexity Targets
- Keep core on-disk format simple and inspectable.
- Keep conversion helpers isolated from generic reader/writer core.
- Ensure all advanced features stay optional and backward compatible.

## Testing Strategy
- Unit tests for writer/reader/index/codec/slice paths.
- Corruption and fuzz-style tests for parser robustness.
- Roundtrip tests using real exported data samples from apps.
- CLI integration tests for inspect/roundtrip/convert commands.

## API Stability Policy
- Reader/writer APIs should evolve additively.
- Existing chunk behaviors remain stable for v1 files.
- Any format bump requires:
  - versioned parser path
  - explicit compatibility matrix
  - migration tooling updates

## Integration Expectations
- PhysicsSim export already writes `.pack` in parallel with VF2D.
- DataLab consumes current VF2D-derived chunk set (`VFHD`, `DENS`, `VELX`, `VELY`, `JSON`).
- Additional apps should adopt via additive export paths before cutover.

## Definition of Done (Long-Term)
- `.pack` is a trusted interchange default for ecosystem tools.
- Reader/writer validation and integrity tooling are production-grade.
- Legacy conversion paths are stable and well-documented.
- Multi-frame workflows are supported without breaking single-frame simplicity.
