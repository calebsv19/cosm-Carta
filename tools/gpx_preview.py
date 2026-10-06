#!/usr/bin/env python3
"""Inspect a bounded GPX 1.1 file locally without importing, simplifying or changing it."""
import argparse
from datetime import datetime
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

NAMESPACE = 'http://www.topografix.com/GPX/1/1'
PREFIX = '{' + NAMESPACE + '}'
MAX_BYTES = 16 * 1024 * 1024
MAX_POINTS = 200_000
MAX_SEGMENTS = 1024
MAX_TRACKS = 128
MAX_NODES = 1_000_000
MAX_DEPTH = 32
MAX_TEXT = 4096
TIME_PATTERN = re.compile(r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$')


def ground_distance_m(a, b):
    """Match Carta's spherical ground-distance contract without projected-unit inflation."""
    la, lb = math.radians(a[0]), math.radians(b[0])
    h = math.sin((lb - la) / 2) ** 2 + math.cos(la) * math.cos(lb) * math.sin(math.radians(b[1] - a[1]) / 2) ** 2
    h = min(1.0, max(0.0, h))
    return 2 * 6378137.0 * math.atan2(math.sqrt(h), math.sqrt(1 - h))


def coordinate(element):
    """Refuse missing, nonfinite or out-of-range WGS84 points without dropping them silently."""
    try:
        lat, lon = float(element.attrib['lat']), float(element.attrib['lon'])
    except (KeyError, ValueError) as error:
        raise ValueError('Every GPX point needs numeric lat/lon') from error
    if not math.isfinite(lat) or not math.isfinite(lon) or not -90 <= lat <= 90 or not -180 <= lon <= 180:
        raise ValueError('GPX point outside finite WGS84 coordinate range')
    return lat, lon


def timestamp(text):
    """Parse only explicit timezone timestamps; unavailable times never become durations."""
    if not text or not TIME_PATTERN.fullmatch(text):
        return None
    try:
        return datetime.fromisoformat(text.replace('Z', '+00:00'))
    except ValueError:
        return None


def preview_bytes(data, kind='unknown'):
    """Return source identity, structure and honest metrics while preserving segment boundaries."""
    if kind not in ('unknown', 'recorded', 'planned'):
        raise ValueError('kind must be unknown, recorded or planned')
    if not data or len(data) > MAX_BYTES:
        raise ValueError('GPX must contain 1 byte through 16 MiB')
    try:
        source = data.decode('utf-8-sig')
    except UnicodeDecodeError as error:
        raise ValueError('This preview supports UTF-8 GPX only') from error
    if '\x00' in source or re.search(r'<!\s*(?:DOCTYPE|ENTITY)\b', source, re.IGNORECASE):
        raise ValueError('NUL, DTD and entity declarations are not supported')
    declaration = re.match(r'\s*<\?xml\b.*?\?>', source, re.DOTALL)
    if declaration:
        encoding = re.search(r'encoding\s*=\s*[\'"]([^\'"]+)', declaration.group(), re.IGNORECASE)
        if encoding and encoding.group(1).lower() not in ('utf-8', 'utf8', 'us-ascii'):
            raise ValueError('This preview supports UTF-8 GPX only')
    parser = ET.XMLPullParser(events=('start', 'end'))
    stack = []
    tracks, segments = [], []
    current_track = current_segment = current_point = None
    previous_point = previous_time = first_time = last_time = None
    node_count = all_points = route_count = route_points = waypoints = 0
    bounds = None
    creator = ''
    root_seen = False
    for offset in range(0, len(data), 65536):
        parser.feed(data[offset:offset + 65536])
        for event, element in parser.read_events():
            if event == 'start':
                stack.append(element.tag)
                node_count += 1
                if len(stack) > MAX_DEPTH or node_count > MAX_NODES:
                    raise ValueError('GPX depth/node limit exceeded')
                if any(len(key) > MAX_TEXT or len(value) > MAX_TEXT for key, value in element.attrib.items()):
                    raise ValueError('GPX attribute text limit exceeded')
                path = tuple(stack)
                if len(stack) == 1:
                    if root_seen or element.tag != PREFIX + 'gpx' or element.get('version') != '1.1':
                        raise ValueError('Expected namespace-qualified GPX 1.1; other versions require an adapter')
                    root_seen = True
                    creator = element.get('creator', '')
                    if len(creator) > MAX_TEXT:
                        raise ValueError('GPX creator text limit exceeded')
                elif path == (PREFIX + 'gpx', PREFIX + 'trk'):
                    if len(tracks) >= MAX_TRACKS:
                        raise ValueError('GPX track limit exceeded')
                    current_track = {'index': len(tracks), 'name': None, 'segment_count': 0, 'point_count': 0}
                    tracks.append(current_track)
                elif path == (PREFIX + 'gpx', PREFIX + 'trk', PREFIX + 'trkseg'):
                    if len(segments) >= MAX_SEGMENTS:
                        raise ValueError('GPX segment limit exceeded')
                    current_segment = {'track_index': current_track['index'], 'segment_index': current_track['segment_count'],
                                       'point_count': 0, 'distance_m': 0.0, 'elevation_count': 0,
                                       'invalid_elevation_count': 0, 'time_count': 0, 'missing_time_count': 0,
                                       'unusable_time_count': 0, 'time_reversal_count': 0, 'large_step_count': 0}
                    segments.append(current_segment)
                    current_track['segment_count'] += 1
                    previous_point = previous_time = first_time = last_time = None
                elif path in ((PREFIX + 'gpx', PREFIX + 'trk', PREFIX + 'trkseg', PREFIX + 'trkpt'),
                               (PREFIX + 'gpx', PREFIX + 'rte', PREFIX + 'rtept'),
                               (PREFIX + 'gpx', PREFIX + 'wpt')):
                    all_points += 1
                    if all_points > MAX_POINTS:
                        raise ValueError('GPX point limit exceeded')
                    point = coordinate(element)
                    if path[-1] == PREFIX + 'trkpt':
                        current_point = {'coordinate': point}
                    elif path[-1] == PREFIX + 'rtept':
                        route_points += 1
                    else:
                        waypoints += 1
                elif path == (PREFIX + 'gpx', PREFIX + 'rte'):
                    route_count += 1
                continue
            path = tuple(stack)
            if len(element.text or '') > MAX_TEXT or len(element.tail or '') > MAX_TEXT:
                raise ValueError('GPX element text limit exceeded')
            if path == (PREFIX + 'gpx', PREFIX + 'trk', PREFIX + 'name'):
                current_track['name'] = (element.text or '').strip()
            elif len(path) == 5 and path[:4] == (PREFIX + 'gpx', PREFIX + 'trk', PREFIX + 'trkseg', PREFIX + 'trkpt') and path[-1] in (PREFIX + 'ele', PREFIX + 'time'):
                key = path[-1][len(PREFIX):]
                if key in current_point:
                    raise ValueError('Duplicate GPX point elevation/time field')
                current_point[key] = (element.text or '').strip()
            elif path == (PREFIX + 'gpx', PREFIX + 'trk', PREFIX + 'trkseg', PREFIX + 'trkpt'):
                point = current_point['coordinate']
                if bounds is None:
                    bounds = {'min_lat': point[0], 'max_lat': point[0], 'min_lon': point[1], 'max_lon': point[1]}
                else:
                    for key, value in [('min_lat', point[0]), ('min_lon', point[1])]:
                        bounds[key] = min(bounds[key], value)
                    for key, value in [('max_lat', point[0]), ('max_lon', point[1])]:
                        bounds[key] = max(bounds[key], value)
                if previous_point is not None:
                    step = ground_distance_m(previous_point, point)
                    current_segment['distance_m'] += step
                    current_segment['large_step_count'] += int(step > 1000)
                previous_point = point
                current_segment['point_count'] += 1
                current_track['point_count'] += 1
                if 'ele' in current_point:
                    try:
                        elevation = float(current_point['ele'])
                    except ValueError:
                        elevation = math.nan
                    current_segment['elevation_count' if math.isfinite(elevation) else 'invalid_elevation_count'] += 1
                if 'time' not in current_point:
                    current_segment['missing_time_count'] += 1
                else:
                    time = timestamp(current_point['time'])
                    if time is None:
                        current_segment['unusable_time_count'] += 1
                    else:
                        current_segment['time_count'] += 1
                        if previous_time is not None and time < previous_time:
                            current_segment['time_reversal_count'] += 1
                        previous_time = last_time = time
                        if first_time is None:
                            first_time = time
                current_point = None
            elif path == (PREFIX + 'gpx', PREFIX + 'trk', PREFIX + 'trkseg'):
                complete = current_segment['point_count'] > 0 and not any(current_segment[key] for key in
                    ('missing_time_count', 'unusable_time_count', 'time_reversal_count'))
                current_segment['elapsed_s'] = (last_time - first_time).total_seconds() if complete else None
                current_segment['first_valid_time'] = first_time.isoformat() if first_time else None
                current_segment['last_valid_time'] = last_time.isoformat() if last_time else None
                current_segment = None
            elif path == (PREFIX + 'gpx', PREFIX + 'trk'):
                current_track = None
            element.clear()
            stack.pop()
    parser.close()
    if not root_seen:
        raise ValueError('GPX root missing')
    point_count = sum(s['point_count'] for s in segments)
    timed = bool(segments) and all(s['elapsed_s'] is not None for s in segments)
    warnings = []
    if route_count or waypoints:
        warnings.append('Routes/waypoints are counted separately and are not recorded-track geometry')
    if not point_count:
        warnings.append('No track points; export an activity as GPX Track for recorded-path preview')
    if any(s['time_reversal_count'] for s in segments):
        warnings.append('Time reversal detected; affected elapsed durations unavailable')
    if any(s['large_step_count'] for s in segments):
        warnings.append('Track contains steps over 1000 m; source geometry was not corrected')
    if any(s['unusable_time_count'] or s['invalid_elevation_count'] for s in segments):
        warnings.append('Some optional fields are unusable; no values were fabricated')
    if bounds and bounds['max_lon'] - bounds['min_lon'] > 180:
        warnings.append('Longitude extent crosses or spans the antimeridian; naive bounds need display handling')
    return {'format': 'carta-gpx-preview-v1', 'ok': True, 'source_sha256': hashlib.sha256(data).hexdigest(),
            'source_bytes': len(data), 'gpx_version': '1.1', 'creator': creator,
            'declared_kind': kind, 'imported': False, 'duplicate_status': 'not_checked',
            'track_count': len(tracks), 'segment_count': len(segments), 'track_point_count': point_count,
            'route_count': route_count, 'route_point_count': route_points, 'waypoint_count': waypoints,
            'track_bounds': bounds, 'track_distance_m': sum(s['distance_m'] for s in segments),
            'sum_segment_elapsed_s': sum(s['elapsed_s'] for s in segments) if timed else None,
            'distance_policy': 'spherical radius 6378137 m; within segments only; no snapping/simplification',
            'elevation_datum': 'unspecified', 'tracks': tracks, 'segments': segments, 'warnings': warnings}


def preview_file(path, kind='unknown'):
    """Read at most the file budget plus one refusal byte without modifying the source."""
    with Path(path).open('rb') as source:
        data = source.read(MAX_BYTES + 1)
    return preview_bytes(data, kind)


def main():
    """Print a local preview receipt or a structured refusal without personal-store writes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gpx', type=Path)
    parser.add_argument('--kind', choices=('unknown', 'recorded', 'planned'), default='unknown')
    args = parser.parse_args()
    try:
        result = preview_file(args.gpx, args.kind)
    except (OSError, ValueError, ET.ParseError) as error:
        print(json.dumps({'ok': False, 'error': str(error)}))
        return 1
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0


if __name__ == '__main__':
    sys.exit(main())
