#!/usr/bin/env python3
"""Proves direct archive payload parity and emitted OSM mode/direction semantics."""
import argparse
import array
import json
import hashlib
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import tempfile


def run(*args, **kwargs):
    """Runs a contract step and stops immediately on failure."""
    return subprocess.run(args, check=True, stdout=subprocess.DEVNULL, **kwargs)


def main():
    """Builds disposable dual/direct fixtures and inspects their actual published bytes."""
    parser = argparse.ArgumentParser()
    parser.add_argument('--tools-dir', required=True)
    options = parser.parse_args()
    tools = Path(options.tools_dir).resolve()
    with tempfile.TemporaryDirectory(prefix='carta-utility-pack-') as temp:
        root = Path(temp)
        source = root / 'fixture.osm'
        source.write_text('''<osm version="0.6">
<bounds minlat="47.66" minlon="-122.36" maxlat="47.67" maxlon="-122.34"/>
<node id="1" lat="47.66" lon="-122.36"/>
<node id="2" lat="47.67" lon="-122.35"/>
<node id="3" lat="47.66" lon="-122.34"/>
<way id="10">
<nd ref="1"/><nd ref="2"/><nd ref="3"/>
<tag k="highway" v="residential"/>
<tag k="oneway" v="yes"/>
</way>
<way id="20">
<nd ref="1"/><nd ref="3"/>
<tag k="highway" v="path"/>
<tag k="access" v="private"/>
<tag k="foot" v="yes"/>
<tag k="oneway:foot" v="-1"/>
</way>
<way id="30">
<nd ref="1"/><nd ref="3"/>
<tag k="highway" v="service"/>
<tag k="foot" v="no"/>
</way>
<way id="40">
<nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="1"/>
<tag k="leisure" v="park"/>
</way>
</osm>'''.replace('<nd ref="1"/><nd ref="2"/><nd ref="3"/><nd ref="1"/>', '<nd ref="1"/>\n<nd ref="2"/>\n<nd ref="3"/>\n<nd ref="1"/>').replace('<nd ref="1"/><nd ref="2"/><nd ref="3"/>', '<nd ref="1"/>\n<nd ref="2"/>\n<nd ref="3"/>').replace('<nd ref="1"/><nd ref="3"/>', '<nd ref="1"/>\n<nd ref="3"/>'))
        for name, flags in [('direct', ['--archive-only', '--max-archive-mib', '16']), ('dual', ['--no-legacy-tiles', '--emit-archive'])]:
            run(str(tools / 'mapforge_region'), '--region', name, '--osm', str(source), '--out', str(root / name), '--min-z', '10', '--max-z', '15', *flags)
        assert not (root / 'direct/tiles').exists()
        query = 'SELECT z,x,y,layer,band,tile_data FROM mapforge_tiles ORDER BY z,x,y,layer,band'
        with sqlite3.connect(root / 'direct/tiles.mbtiles') as a, sqlite3.connect(root / 'dual/tiles.mbtiles') as b:
            assert a.execute(query).fetchall() == b.execute(query).fetchall()
            assert a.execute('PRAGMA quick_check').fetchone()[0] == 'ok'
        env = os.environ.copy(); env['MAPFORGE_REGIONS_DIR'] = str(root)
        run(str(tools / 'mapforge_region_validate'), '--region', 'direct', '--strict', '--strict-contract', env=env)
        run(str(tools / 'mapforge_graph'), '--region', 'direct', '--osm', str(source), '--out', str(root / 'direct'))
        data = (root / 'direct/graph/graph.bin').read_bytes()
        version, nodes, edges = struct.unpack_from('III', data, 4)
        assert version == 4
        offset = 16
        def read(kind, count):
            """Decodes an array in the public graph layout."""
            nonlocal offset
            values = array.array(kind)
            values.frombytes(data[offset:offset + values.itemsize * count]); offset += values.itemsize * count
            return values
        x = read('d', nodes); read('d', nodes); starts = read('I', nodes + 1); to = read('I', edges)
        read('f', edges); read('f', edges); classes = read('B', edges)
        read('f', edges); read('f', edges); read('f', edges); access = read('B', edges)
        assert set(access) <= {1, 2, 3} and 0 not in access
        mask_by_class = {c: [access[e] for e in range(edges) if classes[e] == c] for c in set(classes)}
        assert sorted(mask_by_class[5]) == [2, 2, 3, 3], mask_by_class
        assert mask_by_class[8] == [2], mask_by_class
        assert mask_by_class[6] == [1, 1], mask_by_class
        before = {str(p.relative_to(root / 'direct')): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in (root / 'direct').rglob('*') if p.is_file()}
        old = (root / 'direct/meta.json').read_bytes()
        # A rejected build must leave the active package intact.
        bad = subprocess.run([str(tools / 'mapforge_region'), '--region', 'direct', '--osm', str(source), '--out', str(root / 'direct'), '--archive-only', '--max-z', '19'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        assert bad.returncode != 0 and (root / 'direct/meta.json').read_bytes() == old
        dense = root / 'dense.osm'
        with dense.open('w') as file:
            file.write('<osm version="0.6">\n')
            for node in range(1, 50001):
                file.write(f'<node id="{node}" lat="{47.66 + (node % 100) * .000001}" lon="{-122.35 + (node % 97) * .000001}"/>\n')
            file.write('<way id="1">\n')
            for node in range(1, 50001):
                file.write(f'<nd ref="{node}"/>\n')
            file.write('<tag k="highway" v="residential"/>\n</way>\n</osm>\n')
        limited = subprocess.run([str(tools / 'mapforge_region'), '--region', 'direct', '--osm', str(dense),
                                  '--out', str(root / 'direct'), '--archive-only', '--max-archive-mib', '1',
                                  '--min-z', '10', '--max-z', '12', '--replace'],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        assert limited.returncode != 0 and 'budget exceeded' in limited.stdout + limited.stderr, limited
        after = {str(p.relative_to(root / 'direct')): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in (root / 'direct').rglob('*') if p.is_file()}
        assert after == before, 'Budget rejection changed the active package'
    print('utility pack: archive-only parity/discovery, v4 permissions/directions and budget/failed-build preservation passed')


if __name__ == '__main__':
    main()
