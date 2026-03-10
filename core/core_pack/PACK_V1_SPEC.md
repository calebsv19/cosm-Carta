# `.pack` v1 Freeze Specification

Status: Frozen v1 contract

This document is the normative wire-format contract for `.pack` v1.

## 1. Container Layout

On-disk layout:
1. `CorePackHeader`
2. zero or more chunk records (`CorePackChunkHeader` + payload)
3. `CorePackIndexHeader`
4. `CorePackIndexEntryDisk[count]`
5. `CorePackFooter`

### 1.1 Header
`CorePackHeader` fields:
- `magic` (`u32`): `'CPAK'`
- `version` (`u32`): format version token for v1

### 1.2 Chunk record
`CorePackChunkHeader` fields:
- `type[4]` (`char[4]`): chunk tag
- `size` (`u64`): payload bytes following this header

Payload may be raw or encoded (see codec header behavior in implementation).

### 1.3 Index
`CorePackIndexHeader` fields:
- `magic` (`u32`): `'CPKI'`
- `version` (`u32`): format version token for v1
- `count` (`u64`): number of index entries

`CorePackIndexEntryDisk` fields:
- `type[4]` (`char[4]`)
- `data_offset` (`u64`)
- `size` (`u64`)

### 1.4 Footer
`CorePackFooter` fields:
- `magic` (`u32`): `'CPKF'`
- `version` (`u32`): format version token for v1
- `index_offset` (`u64`): byte offset of `CorePackIndexHeader`

## 2. Endianness and Numeric Encoding
- All integer fields in pack headers/index/footer are little-endian.
- Profile payloads in v1 are little-endian unless explicitly defined otherwise by profile spec.
- Floating-point payload values use IEEE-754 binary formats (`f32`, `f64`) as written by producer.

## 3. Version Compatibility Contract
Freeze marker constants:
- `PACK_FORMAT_VERSION_MAJOR = 1`
- `PACK_FORMAT_VERSION_MINOR = 0`

Reader compatibility for v1 runtime:
- Must reject unknown `MAJOR`.
- Must reject `MINOR` greater than supported minor.
- May accept same major with same/lower minor.

Legacy v1 token compatibility:
- Existing v1 files using raw token `1` are interpreted as `1.0`.

## 4. Chunk Ordering Policy (Recommended)
Order is recommendation, not strict parser requirement.

### 4.1 Physics profile order
1. `VFHD`
2. `DENS`
3. `VELX`
4. `VELY`
5. `JSON` (optional)

### 4.2 DAW profile order
1. `DAWH`
2. `WMIN`
3. `WMAX`
4. `MRKS`
5. `JSON` (optional)

## 5. Profile Required/Optional Chunks

### 5.1 Physics v1 profile
Required chunks:
- `VFHD`: canonical frame metadata
- `DENS`: `f32[w*h]`
- `VELX`: `f32[w*h]`
- `VELY`: `f32[w*h]`

Optional chunks:
- `JSON`: manifest and debug metadata
- unknown optional chunks: readers should ignore unless specifically required by caller

### 5.2 DAW v1 profile
Required chunks:
- `DAWH`: DAW waveform header metadata
- `WMIN`: waveform min envelope (`f32[point_count]`)
- `WMAX`: waveform max envelope (`f32[point_count]`)
- `MRKS`: marker records table

Optional chunks:
- `JSON`: profile metadata/manifests
- unknown optional chunks: readers should ignore unless specifically required by caller

## 6. Allowed vs Disallowed Changes in v1

Allowed without v2:
- Add new optional chunks that old readers can ignore.
- Add optional JSON metadata fields.
- Add new tooling validation around existing unchanged semantics.

Disallowed in v1:
- Change binary layout or meaning of existing required chunks.
- Remove required chunks from a profile.
- Change numeric units/coordinate conventions for existing required fields.

## 7. Error Handling Rules
Reader/tooling should fail with format error when:
- container magic/version is invalid for supported compatibility range
- index/footer layout is malformed
- required profile chunks are missing
- required chunk payload size/shape is invalid for profile contract

Reader/tooling should not fail solely because:
- unknown optional chunks are present
- optional `JSON` chunk is missing

## 8. Change Control After Freeze
Any `.pack` wire-level change requires:
1. Proposed change note (`what`, `why`, compatibility impact)
2. Spec update first
3. Fixture update + parser test update
4. Version bump decision recorded
