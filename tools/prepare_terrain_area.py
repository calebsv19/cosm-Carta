#!/usr/bin/env python3
"""Prepare one bounded, independently removable USGS relief/vector terrain patch."""
import argparse
import hashlib
import json
import math
import os
import sqlite3
import time
import urllib.parse
import urllib.request
from pathlib import Path
from prepare_terrain_overview import ELEVATION, APP_ID, read_dem, relief, tiles
from prepare_terrain_vectors import prepare as prepare_vectors

MIB = 1024 * 1024
FILES = ('terrain.sqlite', 'terrain.json', 'terrain_vectors.sqlite', 'terrain_vectors.json', 'source.json')
GNIS = 'https://carto.nationalmap.gov/arcgis/rest/services/geonames/MapServer'


def disk_bytes(root):
    """Count every staging/output file rather than only the primary database."""
    return sum(p.stat().st_size for p in root.rglob('*') if p.is_file())


def fetch(url, destination, work, output, staging_budget):
    """Stream a bounded official response while counting downloads and prepared outputs."""
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme != 'https' or parsed.hostname not in ('elevation.nationalmap.gov', 'carto.nationalmap.gov'):
        raise ValueError('unexpected source host')
    remaining = min(64*MIB, 80*MIB-disk_bytes(work), staging_budget-disk_bytes(work)-disk_bytes(output)-MIB)
    if remaining <= 0:
        raise ValueError('staging budget exhausted')
    with urllib.request.urlopen(url, timeout=60) as response, destination.open('xb') as file:
        if urllib.parse.urlparse(response.geturl()).hostname != parsed.hostname:
            raise ValueError('unexpected response host')
        declared = response.headers.get('Content-Length')
        if declared and int(declared) > remaining:
            raise ValueError('response exceeds total staging budget')
        received = 0
        while True:
            chunk = response.read(min(1024 * 1024, remaining - received + 1))
            if not chunk:
                break
            received += len(chunk)
            if received > remaining:
                raise ValueError('response exceeds total staging budget')
            file.write(chunk)
    return destination.read_bytes()


def project(lon, lat):
    """Convert geographic route bounds to the app's existing Mercator meter frame."""
    return 6378137 * math.radians(lon), 6378137 * math.log(math.tan(math.pi / 4 + math.radians(lat) / 2))


def prepare(work, output, bounds, name, area_id, fetch_sources=False, max_mib=32, staging_mib=128, resolution=20):
    """Build a complete patch with source/no-data receipts and hard whole-output budgets."""
    if not 1 <= max_mib <= 32 or not 1 <= staging_mib <= 128 or not 10 <= resolution <= 100:
        raise ValueError('invalid area or staging budget/resolution')
    if not area_id or len(area_id) >= 64 or area_id[0] == '-' or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-' for c in area_id):
        raise ValueError('invalid owned area ID')
    if output.exists() and any(output.iterdir()):
        raise ValueError('output must be a new empty staging directory')
    output.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    lat0, lon0, lat1, lon1 = bounds
    if not (-85 < lat0 < lat1 < 85 and -180 <= lon0 < lon1 <= 180):
        raise ValueError('invalid source route bounds')
    # Ground-distance padding is converted to projected distance at the route's center latitude.
    latitude = (lat0 + lat1) * .5
    context = 2000 / math.cos(math.radians(latitude))
    x0, y0 = project(lon0, lat0)
    x1, y1 = project(lon1, lat1)
    crop = [x0-context, y0-context, x1+context, y1+context]
    spacing = resolution / math.cos(math.radians(latitude))
    width, height = math.ceil((crop[2]-crop[0])/spacing), math.ceil((crop[3]-crop[1])/spacing)
    staging_budget = staging_mib * MIB
    if fetch_sources:
        service = json.loads(fetch(ELEVATION+'?f=pjson', work/'elevation-service.json', work, output, staging_budget))
        if width > min(4096, service['maxImageWidth']) or height > min(4096, service['maxImageHeight']):
            raise ValueError('crop exceeds bounded source dimensions')
        params = dict(f='json', bbox=','.join(map(str,crop)), bboxSR=3857, imageSR=3857,
                      size=f'{width},{height}', format='tiff', adjustAspectRatio='false', pixelType='F32',
                      interpolation='RSP_BilinearInterpolation', renderingRule=json.dumps({'rasterFunction':'None'}))
        url = ELEVATION+'/exportImage?'+urllib.parse.urlencode(params)
        export = json.loads(fetch(url, work/'elevation-export.json', work, output, staging_budget))
        if 'href' not in export:
            raise ValueError(export)
        export['request_url'] = url
        (work/'elevation-export.json').write_text(json.dumps(export, indent=2))
        fetch(export['href'], work/'dem.tif', work, output, staging_budget)
        features = []
        envelope = dict(xmin=crop[0], ymin=crop[1], xmax=crop[2], ymax=crop[3], spatialReference={'wkid':3857})
        for layer in (3, 5, 7):
            params = dict(f='json', where='1=1', geometry=json.dumps(envelope), geometryType='esriGeometryEnvelope',
                          inSR=3857, outSR=3857, spatialRel='esriSpatialRelIntersects',
                          outFields='gaz_id,gaz_name,gaz_featureclass', returnGeometry='true', resultRecordCount=2000)
            url = GNIS+f'/{layer}/query?'+urllib.parse.urlencode(params)
            data = json.loads(fetch(url, work/f'gnis-{layer}.json', work, output, staging_budget))
            if data.get('error') or data.get('exceededTransferLimit'):
                raise ValueError('name query incomplete or refused')
            data['request_url'] = url
            (work/f'gnis-{layer}.json').write_text(json.dumps(data, indent=2))
            for feature in data['features']:
                a, g = feature['attributes'], feature['geometry']
                point = g.get('points', [[g.get('x'), g.get('y')]])[0]
                if a['gaz_featureclass'] in ('Summit','Lake','Reservoir','Ridge','Falls','Island','Bay'):
                    features.append(dict(id=a['gaz_id'], name=a['gaz_name'], kind=a['gaz_featureclass'], x=point[0], y=point[1]))
        (work/'labels.json').write_text(json.dumps(features, ensure_ascii=False))
    if disk_bytes(work) > staging_budget:
        raise ValueError('existing sources exceed staging budget')
    source = json.loads((work/'elevation-export.json').read_text())
    values, valid, dx, dy = read_dem(work/'dem.tif', source)
    if not valid.any() or valid.mean() < .95:
        raise ValueError('insufficient elevation coverage')
    e = source['extent']
    actual = [e[k] for k in ('xmin','ymin','xmax','ymax')]
    if max(abs(a-b) for a,b in zip(actual,crop)) > max(dx,dy):
        raise ValueError('source extent deviates from requested crop')
    picture, metrics = relief(values, valid, e, dx, dy, contours=False)
    raster = output/'terrain.sqlite'
    with sqlite3.connect(raster) as db:
        db.executescript(f'pragma application_id={APP_ID};pragma user_version=1;pragma journal_mode=DELETE;create table metadata(key text primary key,value text not null);create table tiles(style integer,lod integer,col integer,row integer,crc integer not null,rgba blob not null,primary key(style,lod,col,row)) without rowid;')
        styles=[]
        # Keep the existing two-mode reader contract; both modes have clean relief, with native vectors.
        for mode in (0,1):
            levels, compressed, palette = tiles(db,mode,picture)
            if db.execute('pragma page_count').fetchone()[0]*db.execute('pragma page_size').fetchone()[0]>max_mib*MIB-MIB:
                raise ValueError('raster exceeds remaining whole-output budget')
            styles.append(dict(name='Relief', area=name, mode=mode, extent=actual, levels=levels,
                               compressed_bytes=compressed, palette=palette, encoding='indexed8_zlib'))
        manifest=dict(schema=1,name=name,crs='EPSG:3857',tile_size=256,gutter=1,styles=styles,
                      metrics=metrics,relief_contours='independent vector sidecar',vertical_units='meters',
                      vertical_datum='USGS multi-resolution service; source cell datum not independently resolved')
        db.execute('insert into metadata values (?,?)',('manifest',json.dumps(manifest,separators=(',',':'))))
        db.commit()
        if db.execute('pragma integrity_check').fetchone()[0] != 'ok':
            raise ValueError('raster integrity failure')
    manifest.update(pack_bytes=raster.stat().st_size, pack_sha256=hashlib.sha256(raster.read_bytes()).hexdigest())
    (output/'terrain.json').write_text(json.dumps(manifest,indent=2))
    if disk_bytes(work)+disk_bytes(output) > staging_budget:
        raise ValueError('staging budget exceeded after raster preparation')
    vector_budget=max_mib*MIB-disk_bytes(output)-MIB
    vectors = prepare_vectors([work], work/'labels.json', output/'terrain_vectors.sqlite', max_bytes=vector_budget)
    receipts = {p.name:json.loads(p.read_text()) for p in work.glob('*.json') if p.name != 'labels.json'}
    (output/'source.json').write_text(json.dumps(dict(schema=1,source='USGS 3DEP + GNIS / The National Map',
        route_bounds=bounds,context_ground_m=2000,extent=actual,resolution_ground_m=resolution,
        sample_spacing_projected_m=[dx,dy],sample_spacing_ground_m=[dx*math.cos(math.radians(latitude)),dy*math.cos(math.radians(latitude))],
        metrics=metrics,source_sha256=hashlib.sha256((work/'dem.tif').read_bytes()).hexdigest(),
        receipts=receipts,retrieved_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())),indent=2))
    sizes = {filename:(output/filename).stat().st_size for filename in FILES}
    size = sum(sizes.values())
    if size > max_mib*MIB or disk_bytes(work)+disk_bytes(output) > staging_budget:
        raise ValueError('whole-output or staging budget exceeded; nothing installed')
    entry=dict(id=area_id,name=name,source='USGS 3DEP + GNIS / The National Map',extent=actual,
               resolution_m=resolution,bytes=size,files=sizes,raster_path=f'{area_id}/terrain.sqlite',
               vector_path=f'{area_id}/terrain_vectors.sqlite')
    return dict(entry=entry,staging_peak_lower_bound_bytes=disk_bytes(work)+disk_bytes(output),
                hashes={filename:hashlib.sha256((output/filename).read_bytes()).hexdigest() for filename in FILES})


def main():
    """Require explicit route bounds and new output paths before downloading or preparing anything."""
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--bounds',type=float,nargs=4,required=True,metavar=('LAT0','LON0','LAT1','LON1'))
    p.add_argument('--name',required=True);p.add_argument('--id',required=True);p.add_argument('--fetch',action='store_true')
    p.add_argument('--max-mib',type=int,default=32);p.add_argument('--staging-mib',type=int,default=128)
    p.add_argument('--resolution',type=float,default=20);p.add_argument('--receipt',type=Path,required=True)
    a=p.parse_args();result=prepare(a.work,a.out,a.bounds,a.name,a.id,a.fetch,a.max_mib,a.staging_mib,a.resolution)
    a.receipt.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
if __name__=='__main__':main()
