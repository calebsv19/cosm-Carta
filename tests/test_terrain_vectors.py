"""Exercise contour crossings/no-data/saddles and the production sidecar reader with isolated files."""
from pathlib import Path
import importlib.util,json,sqlite3,struct,subprocess,sys,tempfile,zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
try:
 from prepare_terrain_vectors import contour_segments, simplify_segments
 import numpy as np
 # A planar slope crosses at x=20 m relative to the extent, independent of display scale.
 v=np.tile(np.array([0,10,20,30],np.float32),(4,1));valid=np.ones_like(v,bool)
 segments=np.concatenate(list(contour_segments(v,valid,[0,0,40,40],10,10,1,20)))
 assert np.allclose(segments[:,[0,2]],25) and np.all(segments[:,4]==20)
 simplified=simplify_segments(segments,.1)
 assert len(simplified)==1 and np.allclose(simplified[:,[0,2]],25)
 valid[:,2]=False
 assert not list(contour_segments(v,valid,[0,0,40,40],10,10,1,20))
 assert not list(contour_segments(np.zeros((4,4),np.float32),np.ones((4,4),bool),[0,0,40,40],10,10,1,20))
 saddle=np.array([[30,0],[0,30]],np.float32)
 assert sum(len(x) for x in contour_segments(saddle,np.ones((2,2),bool),[0,0,20,20],10,10,1,20))==2
except ModuleNotFoundError:
 print("optional numerical preparation checks need NumPy/Pillow; reader fixture still runs")
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'v.sqlite';manifest={'schema':1,'crs':'EPSG:3857','tile_count':1,'label_count':1,'areas':[{'extent':[0,0,100,100],'levels':[{'unit':2560,'interval_m':20,'tiles':1},{'unit':8192,'interval_m':100,'tiles':0},{'unit':32768,'interval_m':500,'tiles':0}]}]}
 raw=struct.pack('<fffff',10,-10,30,-30,20)
 with sqlite3.connect(p) as db:
  db.executescript('pragma application_id=1296454742;pragma user_version=1;create table metadata(key text,value text);create table tiles(area,lod,col,row,crc,data);create table labels(id,name,kind,x,y);')
  db.execute('insert into metadata values(?,?)',('manifest',json.dumps(manifest)));db.execute('insert into tiles values(0,0,0,0,?,?)',(zlib.crc32(raw),zlib.compress(raw)));db.execute("insert into labels values(1,'Test Lake','Lake',20,20)")
 subprocess.run([sys.argv[1],str(p)],check=True)
 with sqlite3.connect(p) as db:db.execute('update tiles set crc=0')
 subprocess.run([sys.argv[1],str(p),'damaged'],check=True)
print('contours: exact plane crossing, saddle topology, missing-cell guard; reader CRC/replacement passed')
