#!/usr/bin/env python3
"""Summarize S5 frame evidence and losslessly compact generated native captures."""
import argparse
import csv
import json
import math
import struct
import zlib
from pathlib import Path


def percentile(values, fraction):
    """Return a nearest-rank percentile without discarding long frames."""
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)] if ordered else None


def compact_bmp(path):
    """Preserve each captured BGRA pixel in a standards-compliant lossless PNG."""
    data = path.read_bytes()
    if data[:2] != b'BM':
        raise ValueError(f'Not a native BMP: {path}')
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    if width <= 0 or height == 0 or planes != 1 or bits != 32 or compression != 3:
        raise ValueError(f'Unsupported native BMP: {path}')
    masks = struct.unpack_from('<IIII', data, 54)
    if masks != (0xff0000, 0xff00, 0xff, 0xff000000):
        raise ValueError(f'Unexpected native pixel masks: {path}')
    stride = width * 4
    if offset + stride * abs(height) != len(data):
        raise ValueError(f'Truncated capture: {path}')
    rows = []
    order = range(abs(height) - 1, -1, -1) if height > 0 else range(abs(height))
    for y in order:
        bgra = data[offset + y * stride:offset + (y + 1) * stride]
        rgba = bytearray(stride)
        rgba[0::4], rgba[1::4], rgba[2::4], rgba[3::4] = bgra[2::4], bgra[1::4], bgra[0::4], bgra[3::4]
        rows.append(b'\0' + rgba)
    raw = b''.join(rows)
    packed = zlib.compress(raw)
    if zlib.decompress(packed) != raw:
        raise ValueError('Capture round trip failed')
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    output = path.with_suffix('.png')
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, abs(height), 8, 6, 0, 0, 0)) + chunk(b'IDAT', packed) + chunk(b'IEND', b'')
    output.write_bytes(png)
    if output.read_bytes() != png:
        raise ValueError('Capture write readback failed')
    path.unlink()  # Only generated diagnostic BMPs supplied by the caller are compacted.
    return {'path': str(output), 'width': width, 'height': abs(height), 'source_bytes': len(data), 'png_bytes': len(png)}


def summarize(path):
    """Report full-run and phase frame percentiles, stage timings and resource peaks."""
    rows = [row for row in csv.DictReader(path.open()) if row.get('capture') is not None]
    results = {'path': str(path), 'frames': len(rows), 'last_time': float(rows[-1]['time']) if rows else None, 'phases': {}}
    for phase in sorted({row['phase'] for row in rows}, key=int):
        group = [row for row in rows if row['phase'] == phase]
        frame = [float(row['frame_ms']) for row in group]
        results['phases'][phase] = {'frames': len(group), 'p50_ms': percentile(frame, .5), 'p95_ms': percentile(frame, .95), 'max_ms': max(frame),
            'update_p50_ms': percentile([float(row['update_ms']) for row in group], .5),
            'render_p50_ms': percentile([float(row['render_ms']) for row in group], .5),
            'builds_end': int(group[-1]['builds']), 'gpu_bytes_end': int(group[-1]['gpu_mesh_bytes']),
            'queue_p50_ms': percentile([float(row['queue_ms']) for row in group], .5),
            'integrate_p95_ms': percentile([float(row['integrate_ms']) for row in group], .95),
            'present_p50_ms': percentile([float(row['present_ms']) for row in group], .5),
            'cpu_bytes_end': int(group[-1]['cpu_tile_bytes']), 'missing_max': max(int(row['missing']) for row in group)}
    for key in ('gpu_mesh_bytes', 'cpu_tile_bytes', 'rss_peak_bytes'):
        results[key + '_peak'] = max((int(row[key]) for row in rows), default=0)
    for key in ('l0_pending_age_ms', 'l0_pending', 'upload_queue', 'cpu_budget_refusals', 'gpu_budget_refusals', 'hold_hits', 'cpu_evictions', 'fill_failures'):
        if rows and rows[0].get(key) is not None:
            results[key + '_peak'] = max(float(row[key]) for row in rows)
    return results


def main():
    """Write a small JSON report and optionally compact named generated captures."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', type=Path, action='append', default=[])
    parser.add_argument('--capture', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = {'schema': 1, 'runs': [summarize(path) for path in args.csv], 'captures': [compact_bmp(path) for path in args.capture]}
    args.output.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
