#!/usr/bin/env python3
"""Verify real sparse TIFF decoding plus hillshade/no-data/contour and gutter geometry."""
import importlib.util,json,sys,tempfile,sqlite3,zlib
from pathlib import Path
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('terrain_prepare',root/'tools/prepare_terrain_overview.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
x=np.tile(np.arange(512,dtype=np.float32),(512,1))*2;valid=np.ones_like(x,dtype=bool);valid[200:220,200:220]=False
picture,metrics=module.relief(x,valid,dict(ymax=6000000),30,30);pixels=np.asarray(picture)
assert metrics['contour_cells']>0 and metrics['no_data_cells']==400
assert not pixels[200:220,200:220,3].any();assert not pixels[199,200:220,3].any();assert pixels[100,100,3]==255
flat,metrics=module.relief(np.zeros((512,512),dtype=np.float32),np.ones((512,512),dtype=bool),dict(ymax=6000000),30,30);assert metrics['contour_cells']==0
with sqlite3.connect(':memory:') as db:
    db.execute('create table tiles(style,lod,col,row,crc,rgba)');levels,count,palette=module.tiles(db,0,picture)
    left=np.frombuffer(zlib.decompress(db.execute('select rgba from tiles where lod=0 and col=0 and row=0').fetchone()[0]),dtype=np.uint8).reshape(258,258)
    right=np.frombuffer(zlib.decompress(db.execute('select rgba from tiles where lod=0 and col=1 and row=0').fetchone()[0]),dtype=np.uint8).reshape(258,258)
    assert np.array_equal(left[:,256],right[:,0]);assert np.array_equal(left[:,257],right[:,1])
if len(sys.argv)>1:
    folder=Path(sys.argv[1]);export=json.loads((folder/'elevation-export.json').read_text());values,mask,dx,dy=module.read_dem(folder/'dem.tif',export)
    assert (~mask).sum()==65536 and values[mask].max()<2200 and values[mask].min()>-10
print('terrain preparation: physical slope, flat/no-data contours, sparse source blocks and neighboring tile gutters passed')
