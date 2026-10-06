#!/usr/bin/env python3
"""Exercise production terrain admission with tiny synthetic data isolated from personal files."""
import json,sqlite3,subprocess,sys,tempfile,zlib,shutil
from pathlib import Path
with tempfile.TemporaryDirectory(prefix='mapforge-terrain-test-') as folder:
    path=Path(folder)/'valid.sqlite';broken=Path(folder)/'broken.sqlite'
    levels=[dict(width=256//2**i,height=256//2**i,cols=1,rows=1) for i in range(5)]
    palette=[[0,0,0,0] for _ in range(256)];palette[7]=[72,90,120,255]
    manifest=dict(schema=1,crs='EPSG:3857',tile_size=256,gutter=1,styles=[dict(extent=[-1000,-1000,1000,1000],levels=levels) for _ in range(2)])
    manifest['styles'][0].update(palette=palette,encoding='indexed8_zlib')
    raw=bytes([72,90,120,255])*258*258
    with sqlite3.connect(path) as db:
        db.executescript('pragma application_id=1296454736; pragma user_version=1;create table metadata(key text primary key,value text);create table tiles(style,lod,col,row,crc,rgba,primary key(style,lod,col,row));')
        db.execute('insert into metadata values(?,?)',('manifest',json.dumps(manifest)))
        for s in range(2):
            for l in range(5):db.execute('insert into tiles values(?,?,?,?,?,?)',(s,l,0,0,zlib.crc32(raw),zlib.compress(bytes([7])*258*258 if s==0 else raw)))
    shutil.copyfile(path,broken)
    subprocess.run([sys.argv[1],str(path),str(broken)],check=True)
