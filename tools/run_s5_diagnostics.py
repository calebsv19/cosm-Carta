#!/usr/bin/env python3
"""Run a bounded S5 replay with isolated state, identity receipts and compact native captures."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path
from s5_native_report import compact_bmp, summarize


def digest(path):
    """Hash one file without reading large packs into memory."""
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    """Record source and pack identity, then run one time-bounded disposable corpus."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--regions', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=['vulkan', 'sdl'], default='vulkan')
    parser.add_argument('--passes', type=int, choices=range(1, 5), default=2)
    parser.add_argument('--region-only', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    binary, regions = args.binary.resolve(), args.regions.resolve()
    files = sorted([*root.glob('src/**/*.c'), *root.glob('include/**/*.h')])
    identity = {'schema': 1, 'binary': str(binary), 'binary_sha256': digest(binary),
                'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                'backend_requested': args.backend, 'passes': args.passes, 'region_only': args.region_only,
                'validation_requested': os.environ.get('MAPFORGE_REQUIRE_VK_VALIDATION') == '1',
                'theme': 'soft_light; disposable theme persistence', 'logical_initial_size': [1280, 720],
                'cpu_layer_limits_mib': {'roads_each': 64, 'other_each': 32}, 'gpu_payload_limit_mib': 512,
                'capture_limit_bytes': 256 * 1024 * 1024, 'frame_row_limit': 100000, 'trace_stride': 32,
                'font_preset_requested': os.environ.get('MAPFORGE_FONT_PRESET', 'app default'),
                'source_sha256': {str(p.relative_to(root)): digest(p) for p in files},
                'shared_versions': {name: (root / 'third_party/codework_shared' / name / 'VERSION').read_text().strip() for name in ('vk_renderer', 'vk_runtime')},
                'region_sha256': {str(p.relative_to(regions)): digest(p) for name in ('north_seattle_service_area', 'seattle') for p in sorted((regions / name).glob('*')) if p.is_file()}}
    (output / 'identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    env = os.environ.copy()
    # A temporary runtime prevents replay camera, configuration or pins from entering personal state.
    with tempfile.TemporaryDirectory(prefix='carta-s5-') as runtime:
        env.update(MAPFORGE_RUNTIME_DIR=runtime, MAPFORGE_THEME_PERSIST_PATH=str(Path(runtime) / 'theme.txt'),
                   MAPFORGE_THEME_PRESET='soft_light', MAPFORGE_RENDER_BACKEND=args.backend,
                   MAPFORGE_REGIONS_DIR=str(regions), MAPFORGE_START_REGION='north_seattle_service_area',
                   MAPFORGE_VIEWPORT_SCENARIO='s5', MAPFORGE_VIEWPORT_SCENARIO_DURATION_SEC='300',
                   MAPFORGE_S5_PASSES=str(args.passes), MAPFORGE_S5_CSV=str(output / 'frames.csv'),
                   MAPFORGE_VK_DEBUG='1', MAPFORGE_POLYGON_PRESENT_TRACE='1', MAPFORGE_POLYGON_PRESENT_TRACE_STRIDE='32')
        env.pop('MAPFORGE_FOCUS_PLACE_ID', None)
        env.pop('MAPFORGE_RUNTIME_CONFIG_PATH', None)
        env.pop('MAPFORGE_S5_REGION_ONLY', None)
        env.pop('MAPFORGE_S5_CAPTURE_PREFIX', None)
        if args.region_only:
            env['MAPFORGE_S5_REGION_ONLY'] = '1'
        if args.backend == 'vulkan':
            env['MAPFORGE_S5_CAPTURE_PREFIX'] = str(output / 'native')
        else:
            env.update(SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software')
        with (output / 'native.log').open('w') as log:
            result = subprocess.run([str(binary)], cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=30 + args.passes * 32)
    captures = [compact_bmp(p) for p in sorted(output.glob('native-*.bmp'))]
    summary = summarize(output / 'frames.csv')
    log = (output / 'native.log').read_text()
    summary.update(exit_code=result.returncode, captures=captures,
                   validation_messages=log.count('[vk_runtime validation]'),
                   retained_presentation_misses=sum('no_draw_after_miss=1' in line and 'hold(hit=1 valid=1)' in line for line in log.splitlines()),
                   no_draw_after_miss=log.count('no_draw_after_miss=1'),
                   invariant_failures=log.count('tile_presenter invariant fail'),
                   lifecycle_invalid=log.count('[s5] lifecycle_invalid'),
                   region_failures=log.count('[s5] region switch failed'),
                   native_confirmed='Render backend: vulkan (vulkan_available=yes)' in log)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({key: summary[key] for key in ('frames', 'last_time', 'exit_code', 'no_draw_after_miss', 'invariant_failures', 'lifecycle_invalid', 'region_failures', 'native_confirmed')}))
    if result.returncode or summary['invariant_failures'] or summary['lifecycle_invalid'] or summary['region_failures'] or summary['validation_messages'] or summary['retained_presentation_misses']:
        raise SystemExit(1)
    if args.backend == 'vulkan' and not summary['native_confirmed']:
        raise SystemExit('Requested native run used another backend')
    if not args.region_only and (summary['last_time'] or 0) < args.passes * 32 - .5:
        raise SystemExit('Incomplete diagnostic replay')


if __name__ == '__main__':
    main()
