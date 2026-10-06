#!/usr/bin/env python3
"""Resolve a deliberate address lookup and call Carta's guarded intake service."""
import argparse
import hashlib
import datetime
import json
import math
import os
from pathlib import Path
import re
import subprocess
import tempfile
import urllib.parse
import urllib.request

SEATTLE_LOCATOR = 'https://gisdata.seattle.gov/cosgis/rest/services/locators/AddressPoints/GeocodeServer'


def normalize_address(value):
    """Normalize common US street abbreviations while retaining unit identity."""
    replacements = {'avenue': 'ave', 'street': 'st', 'road': 'rd', 'boulevard': 'blvd',
                    'northeast': 'ne', 'northwest': 'nw', 'southeast': 'se', 'southwest': 'sw',
                    'washington': 'wa', 'apartment': 'unit', 'apt': 'unit'}
    tokens = re.findall(r'[a-z0-9]+', value.lower())
    return ' '.join(replacements.get(token, token) for token in tokens)


def seattle_candidates(address, response=None):
    """Return address-point candidates; never downgrade a fuzzy match to exact."""
    if response is None:
        query = urllib.parse.urlencode({'SingleLine': address, 'outSR': '4326', 'outFields': '*', 'f': 'json'})
        request = urllib.request.Request(SEATTLE_LOCATOR + '/findAddressCandidates?' + query,
                                         headers={'User-Agent': 'Carta-local-place-intake/1'})
        with urllib.request.urlopen(request, timeout=15) as stream:
            response = json.loads(stream.read(2_000_001))
    if 'error' in response:
        raise ValueError('Seattle address locator refused the lookup')
    candidates = []
    for candidate in response.get('candidates', [])[:20]:
        attributes = candidate.get('attributes', {})
        point = candidate.get('location', {})
        lat, lon = point.get('y'), point.get('x')
        if not isinstance(lat, (float, int)) or not isinstance(lon, (float, int)):
            continue
        if not math.isfinite(lat) or not math.isfinite(lon) or not (-85.05112878 <= lat <= 85.05112878 and -180 <= lon <= 180):
            continue
        exact = (candidate.get('score') == 100 and attributes.get('Addr_type') == 'PointAddress'
                 and attributes.get('Status') == 'M' and attributes.get('City', '').upper() == 'SEATTLE')
        # Compare the complete supplied address, including unit, rather than score alone.
        matched = candidate.get('address', '')
        exact = exact and normalize_address(address) == normalize_address(matched)
        candidates.append({'lat': lat, 'lon': lon, 'source': 'Seattle AddressPoints',
                           'quality': 'address_point' if exact else 'approximate',
                           'matched_address': matched, 'score': candidate.get('score'),
                           'address_type': attributes.get('Addr_type'), 'result_id': str(attributes.get('MAF_ID', '')),
                           'source_url': SEATTLE_LOCATOR,
                           'lookup_at': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                           'retention': 'Seattle public GIS; source attribution retained'})
    return candidates


def call_command(binary, request):
    """Use the native/closed-owner command path and inspect its structured receipt."""
    with tempfile.TemporaryDirectory(prefix='carta-intake-') as directory:
        path = Path(directory) / 'request.json'
        path.write_text(json.dumps(request), encoding='utf-8')
        os.chmod(path, 0o600)
        result = subprocess.run([str(binary), '--request', str(path)], text=True, capture_output=True)
    try:
        reply = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise ValueError(result.stderr.strip() or 'Place service returned no receipt') from error
    if result.returncode or not reply.get('ok'):
        raise ValueError(reply.get('error', 'Place service failed'))
    return reply


def main():
    """Resolve, preview and optionally commit one explicit user intake request."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--resolve', choices=['supplied', 'seattle_address_points'], default='supplied')
    parser.add_argument('--commit', action='store_true')
    args = parser.parse_args()
    request = json.loads(args.input.read_text(encoding='utf-8'))
    client_input_token = hashlib.sha256(json.dumps(request, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    existing = call_command(args.binary, {'operation': 'intake.receipt', 'request_id': request.get('request_id', '')})
    if existing.get('receipt'):
        receipt = existing['receipt']
        if receipt.get('client_input_token') != client_input_token:
            raise ValueError('request_id already used for different intake input')
        stored = call_command(args.binary, {'operation': 'places.get', 'place_id': receipt['place_id']})
        print(json.dumps({'receipt': receipt, 'stored': stored}, indent=2))
        return 0
    request['client_input_token'] = client_input_token
    if args.resolve == 'seattle_address_points' and not request.get('location'):
        candidates = seattle_candidates(request.get('address', ''))
        exact = [candidate for candidate in candidates if candidate['quality'] == 'address_point']
        if len(exact) != 1:
            print(json.dumps({'ok': False, 'status': 'location_review_required', 'candidates': candidates}, indent=2))
            return 1
        request['location'] = exact[0]
        request['location']['input_address'] = request.get('address', '')
        request['address'] = exact[0]['matched_address']
    request['operation'] = 'intake.preview'
    preview = call_command(args.binary, request)
    if not args.commit:
        print(json.dumps({'preview': preview, 'location': request.get('location')}, indent=2))
        return 0
    request['operation'] = 'places.update' if request.get('expected_revision') is not None else 'intake.commit'
    request['expected_store_token'] = preview['store_token']
    request['candidate_token'] = preview['candidate_token']
    receipt = call_command(args.binary, request)
    stored = call_command(args.binary, {'operation': 'places.get', 'place_id': receipt['place_id']})
    print(json.dumps({'receipt': receipt, 'stored': stored}, indent=2))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (ValueError, OSError, TimeoutError) as error:
        print(json.dumps({'ok': False, 'error': str(error)}))
        raise SystemExit(1)
