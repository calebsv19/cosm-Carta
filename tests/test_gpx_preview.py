"""Verify bounded GPX preview semantics with source-preserving synthetic fixtures."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

TOOL = Path(__file__).resolve().parents[1] / 'tools/gpx_preview.py'
spec = importlib.util.spec_from_file_location('gpx_preview', TOOL)
gpx = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gpx)


def document(body, prefix=''):
    """Build equivalent default/prefixed GPX namespaces for parser parity checks."""
    if prefix:
        body = body.replace('<', '<p:').replace('<p:/', '</p:')
        return ('<p:gpx xmlns:p="%s" version="1.1" creator="fixture">%s</p:gpx>' % (gpx.NAMESPACE, body)).encode()
    return ('<gpx xmlns="%s" version="1.1" creator="fixture">%s</gpx>' % (gpx.NAMESPACE, body)).encode()


class GpxPreviewTest(unittest.TestCase):
    """Exercise geometry, optional fields, origin distinction and bounded refusals."""

    def test_tracks_segments_namespaces_and_gaps(self):
        """Use an equatorial oracle and a distant second segment to detect accidental bridging."""
        body = ('<trk><name>Loop &amp; return</name><trkseg>'
                '<trkpt lat="0" lon="0"><ele>10</ele><time>2026-10-04T10:00:00Z</time></trkpt>'
                '<trkpt lat="0" lon="0.001"><ele>12</ele><time>2026-10-04T10:01:00Z</time></trkpt>'
                '</trkseg><trkseg><trkpt lat="10" lon="10"/></trkseg></trk>'
                '<trk><trkseg><trkpt lat="20" lon="20"/></trkseg></trk>')
        source_tree = ET.fromstring(document(body))
        result = gpx.preview_bytes(document(body), 'recorded')
        ns = {'g': gpx.NAMESPACE}
        self.assertEqual(result['track_count'], len(source_tree.findall('g:trk', ns)))
        self.assertEqual(result['segment_count'], len(source_tree.findall('g:trk/g:trkseg', ns)))
        self.assertEqual(result['track_point_count'], len(source_tree.findall('g:trk/g:trkseg/g:trkpt', ns)))
        prefixed = gpx.preview_bytes(document(body, prefix='p'), 'recorded')
        prefixed['source_sha256'] = result['source_sha256']
        prefixed['source_bytes'] = result['source_bytes']
        self.assertEqual(result, prefixed)
        self.assertEqual((result['track_count'], result['segment_count'], result['track_point_count']), (2, 3, 4))
        self.assertAlmostEqual(result['track_distance_m'], 6378137 * math.radians(.001), places=7)
        self.assertEqual(result['segments'][0]['elapsed_s'], 60)
        self.assertIsNone(result['segments'][1]['elapsed_s'])
        self.assertIsNone(result['sum_segment_elapsed_s'])
        self.assertEqual(result['tracks'][0]['name'], 'Loop & return')
        self.assertEqual(result['segments'][0]['elevation_count'], 2)
        self.assertEqual(result['track_bounds']['max_lat'], 20)
        self.assertFalse(result['imported'])
        self.assertEqual(result['duplicate_status'], 'not_checked')

    def test_optional_fields_and_reversal(self):
        """Refuse fabricated duration/ascent while keeping counts of unusable optional fields."""
        body = ('<trk><trkseg><trkpt lat="47" lon="-122"><time>2026-10-04T10:01:00Z</time></trkpt>'
                '<trkpt lat="47" lon="-122"><time>2026-10-04T10:00:00Z</time><ele>NaN</ele></trkpt>'
                '<trkpt lat="47" lon="-122"><time>invalid</time></trkpt>'
                '<trkpt lat="47" lon="-122"/></trkseg></trk>')
        result = gpx.preview_bytes(document(body))
        segment = result['segments'][0]
        self.assertEqual((segment['time_count'], segment['time_reversal_count'], segment['unusable_time_count'], segment['missing_time_count']), (2, 1, 1, 1))
        self.assertEqual(segment['invalid_elevation_count'], 1)
        self.assertIsNone(segment['elapsed_s'])
        self.assertEqual(result['declared_kind'], 'unknown')
        self.assertEqual(result['elevation_datum'], 'unspecified')
        self.assertEqual(result['track_distance_m'], 0)
        self.assertNotIn('ascent_m', result)
        self.assertIsNone(gpx.timestamp('2026-10-04T10:01:00'))
        self.assertIsNone(gpx.timestamp('2026-10-04T10:01:00+99:00'))
        timed = gpx.preview_bytes(document('<trk><trkseg><trkpt lat="0" lon="0"><time>2026-10-04T10:00:00Z</time></trkpt><trkpt lat="0" lon="0"><time>2026-10-04T12:01:00+02:00</time></trkpt></trkseg></trk>'))
        self.assertEqual(timed['sum_segment_elapsed_s'], 60)

    def test_route_waypoint_and_extension_distinction(self):
        """Route geometry and extension-named points never count as an actual recorded track."""
        data = document('<rte><rtept lat="0" lon="0"/><rtept lat="0" lon="1"/></rte><wpt lat="2" lon="2"/><extensions><trkpt lat="1" lon="1"/></extensions>')
        result = gpx.preview_bytes(data, 'planned')
        self.assertEqual((result['route_count'], result['route_point_count'], result['waypoint_count']), (1, 2, 1))
        self.assertEqual(result['track_point_count'], 0)
        self.assertIsNone(result['track_bounds'])
        self.assertTrue(result['warnings'])

    def test_source_hash_and_cli_read_only(self):
        """Keep exact full-resolution source bytes and return a reproducible CLI receipt."""
        data = document('<trk><trkseg><trkpt lat="0" lon="0"/><trkpt lat="0" lon="0.001"/></trkseg></trk>')
        with tempfile.TemporaryDirectory(prefix='carta-gpx-test-') as temp:
            path = Path(temp) / 'recorded.gpx'
            path.write_bytes(data)
            result = subprocess.run([sys.executable, str(TOOL), str(path), '--kind', 'recorded'], capture_output=True, text=True, check=True)
            receipt = json.loads(result.stdout)
            self.assertEqual(receipt['source_sha256'], hashlib.sha256(data).hexdigest())
            self.assertEqual(path.read_bytes(), data)
            self.assertEqual(list(Path(temp).iterdir()), [path])
            path.write_text('<gpx>broken')
            bad = subprocess.run([sys.executable, str(TOOL), str(path)], capture_output=True, text=True)
            self.assertEqual(bad.returncode, 1)
            self.assertFalse(json.loads(bad.stdout)['ok'])

    def test_refusals_and_limits(self):
        """Reject malformed coordinates, declarations, future schemas and each allocation budget."""
        bad = [b'', b'<gpx>', b'<!DOCTYPE gpx [<!ENTITY x "expanded">]>' + document('<trk/>'),
               b'<?xml version="1.0" encoding="UTF-16"?>' + document(''),
               document('').replace(b'version="1.1"', b'version="2.0"'),
               document('<trk><trkseg><trkpt lat="NaN" lon="0"/></trkseg></trk>'),
               document('<trk><trkseg><trkpt lat="91" lon="0"/></trkseg></trk>'),
               document('<trk><trkseg><trkpt lat="0"/></trkseg></trk>'),
               document('<trk><trkseg><trkpt lat="0" lon="0"><ele>1</ele><ele>2</ele></trkpt></trkseg></trk>')]
        for data in bad:
            with self.subTest(data=data[:80]), self.assertRaises((ValueError, ET.ParseError)):
                gpx.preview_bytes(data)
        cases = [('MAX_BYTES', 16, document('')), ('MAX_DEPTH', 2, document('<trk><trkseg/></trk>')),
                 ('MAX_POINTS', 1, document('<rte><rtept lat="0" lon="0"/><rtept lat="1" lon="1"/></rte>')),
                 ('MAX_SEGMENTS', 1, document('<trk><trkseg/><trkseg/></trk>')),
                 ('MAX_TRACKS', 1, document('<trk/><trk/>')),
                 ('MAX_NODES', 1, document('<trk/>')), ('MAX_TEXT', 10, document('<trk><name>longer-than-ten</name></trk>'))]
        for key, value, data in cases:
            with self.subTest(key=key), patch.object(gpx, key, value), self.assertRaises(ValueError):
                gpx.preview_bytes(data)
        with self.assertRaises(ValueError):
            gpx.preview_bytes(document(''), 'completed')

    def test_large_step_and_antimeridian_are_reported(self):
        """Preserve unusual GPS source geometry and warn rather than silently snapping it."""
        result = gpx.preview_bytes(document('<trk><trkseg><trkpt lat="0" lon="179"/><trkpt lat="0" lon="-179"/></trkseg></trk>'))
        self.assertAlmostEqual(result['track_distance_m'], 6378137 * math.radians(2), places=6)
        self.assertEqual(result['segments'][0]['large_step_count'], 1)
        self.assertEqual(len(result['warnings']), 2)


if __name__ == '__main__':
    unittest.main()
