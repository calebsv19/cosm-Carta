#!/usr/bin/env python3
"""Build bounded offline contour geometry; no Python or network is needed by Carta."""
import argparse, hashlib, json, math, os, sqlite3, tempfile, time, zlib
from pathlib import Path
import numpy as np
from prepare_terrain_overview import read_dem, relief, tiles

APP_ID = 0x4d465456
# Interpolate genuine contour crossings at DEM sample centers and resolve saddle cells by their center.
def contour_segments(values, valid, extent, dx, dy, step, interval):
    v = values[::step, ::step]; ok = valid[::step, ::step].copy()
    # Any missing source sample in a decimated cell rejects that whole cell.
    h, w = v.shape
    safe = np.ones((h-1,w-1),bool)
    for oy in range(step+1):
        for ox in range(step+1):
            sample=valid[oy:oy+(h-1)*step:step, ox:ox+(w-1)*step:step]
            safe[:sample.shape[0],:sample.shape[1]] &= sample
    corners = [v[:-1,:-1], v[:-1,1:], v[1:,1:], v[1:,:-1]]
    pairs = {1:[(3,0)],2:[(0,1)],3:[(3,1)],4:[(1,2)],6:[(0,2)],7:[(3,2)],
             8:[(2,3)],9:[(0,2)],11:[(1,2)],12:[(3,1)],13:[(0,1)],14:[(3,0)]}
    edge_ends = [(0,1),(1,2),(2,3),(3,0)]
    cx = [0,1,1,0]; cy = [0,0,1,1]
    if not valid.any(): return
    for elevation in range(interval, int(values[valid].max())+1, interval):
        code = sum((c >= elevation).astype(np.uint8)*(1<<i) for i,c in enumerate(corners))
        for case in range(1,15):
            rows,cols=np.where((code==case)&safe)
            if not len(rows): continue
            groups=[(rows,cols,pairs[case])] if case not in (5,10) else []
            if case in (5,10):
                high=sum(c[rows,cols] for c in corners)/4 >= elevation
                for flag in [False,True]:
                    mask=high==flag
                    # Same topological rule under complementary corner states.
                    ps=[(0,1),(2,3)] if (case==5)==flag else [(0,3),(1,2)]
                    groups.append((rows[mask],cols[mask],ps))
            for rr,cc,ps in groups:
                def crossing(edge):
                    a,b=edge_ends[edge];av=corners[a][rr,cc];bv=corners[b][rr,cc]
                    f=(elevation-av)/(bv-av)
                    x=(cc+cx[a]+f*(cx[b]-cx[a]))*step*dx+dx*.5
                    y=(rr+cy[a]+f*(cy[b]-cy[a]))*step*dy+dy*.5
                    return x,y
                for a,b in ps:
                    x0,y0=crossing(a);x1,y1=crossing(b)
                    yield np.column_stack((x0,-y0,x1,-y1,np.full(len(rr),elevation))).astype('<f4')

# Simplify connected contour chains within a bounded projected tolerance, preserving endpoints and gaps.
def simplify_segments(data, tolerance):
    nodes=[];lookup={};edges=[];adj=[]
    for segment in data:
        ends=[]
        for x,y in [(segment[0],segment[1]),(segment[2],segment[3])]:
            key=(round(float(x)*100),round(float(y)*100),float(segment[4]))
            if key not in lookup:lookup[key]=len(nodes);nodes.append((x,y,segment[4]));adj.append([])
            ends.append(lookup[key])
        if ends[0]==ends[1]:continue
        edge=len(edges);edges.append(ends)
        for node in ends:adj[node].append(edge)
    visited=set();out=[]
    starts=[i for i,neighbours in enumerate(adj) if len(neighbours)!=2]+list(range(len(nodes)))
    for start in starts:
        for edge in adj[start]:
            if edge in visited:continue
            chain=[start];current=start
            while edge not in visited:
                visited.add(edge);a,b=edges[edge];current=b if a==current else a;chain.append(current)
                if current==start or len(adj[current])!=2:break
                other=[e for e in adj[current] if e not in visited]
                if not other:break
                edge=other[0]
            points=np.array([nodes[i] for i in chain],dtype=np.float64)
            keep={0,len(points)-1};stack=[(0,len(points)-1)]
            while stack:
                a,b=stack.pop()
                if b<=a+1:continue
                delta=points[b,:2]-points[a,:2];length=np.dot(delta,delta)
                offsets=points[a+1:b,:2]-points[a,:2]
                t=np.clip(offsets@delta/max(length,1e-12),0,1)
                distance=np.sum((offsets-t[:,None]*delta)**2,axis=1);index=int(np.argmax(distance))
                if distance[index]>tolerance*tolerance:
                    split=a+1+index;keep.add(split);stack.extend([(a,split),(split,b)])
            chosen=points[sorted(keep)]
            for a,b in zip(chosen[:-1],chosen[1:]):out.append([a[0],a[1],b[0],b[1],a[2]])
    return np.asarray(out,dtype='<f4').reshape((-1,5))

# Partition small segments by every touched tile so edge-crossing geometry cannot disappear.
def contour_tiles(values, valid, extent, dx, dy, step, interval, unit):
    groups={}
    for batch in contour_segments(values,valid,extent,dx,dy,step,interval):
        c0=np.floor(np.minimum(batch[:,0],batch[:,2])/unit).astype(int)
        c1=np.floor(np.maximum(batch[:,0],batch[:,2])/unit).astype(int)
        r0=np.floor(-np.maximum(batch[:,1],batch[:,3])/unit).astype(int)
        r1=np.floor(-np.minimum(batch[:,1],batch[:,3])/unit).astype(int)
        for dc,dr in [(0,0),(1,0),(0,1),(1,1)]:
            cc=c0+dc;rr=r0+dr;mask=(cc<=c1)&(rr<=r1)
            keys=rr[mask]*65536+cc[mask];data=batch[mask]
            for key in np.unique(keys):groups.setdefault((int(key%65536),int(key//65536)),[]).append(data[keys==key])
    for key,chunks in groups.items():
        raw=simplify_segments(np.concatenate(chunks),max(dx,dy)*step*.20).tobytes()
        if len(raw)>512*1024:raise ValueError('vector tile exceeds decoded ceiling')
        yield key,raw

# Replace only a fully verified sidecar; keep source provenance separate from personal history.
def prepare(sources, labels, destination, replace=False, max_bytes=64*1024*1024):
    if destination.exists() and not replace:raise ValueError('destination exists; use --replace')
    if not 4096 <= max_bytes <= 64*1024*1024:raise ValueError('invalid vector budget')
    destination.parent.mkdir(parents=True,exist_ok=True)
    fd,name=tempfile.mkstemp(prefix='.vectors-',dir=destination.parent);os.close(fd);tmp=Path(name)
    areas=[];total=0
    try:
        with sqlite3.connect(tmp) as db:
            db.executescript(f'pragma application_id={APP_ID};pragma user_version=1;create table metadata(key text primary key,value text);create table tiles(area integer,lod integer,col integer,row integer,crc integer,data blob,primary key(area,lod,col,row));create table labels(id integer primary key,name text,kind text,x real,y real);')
            for i,source in enumerate(sources):
                meta=json.loads((source/'elevation-export.json').read_text());e=meta['extent']
                values,valid,dx,dy=read_dem(source/'dem.tif',meta)
                extent=[e[k] for k in ['xmin','ymin','xmax','ymax']]
                levels=[]
                for lod,(step,interval,unit) in enumerate([(4,20,2560),(8,100,8192),(16,500,32768)]):
                    count=segments=0
                    for (col,row),raw in contour_tiles(values,valid,extent,dx,dy,step,interval,unit):
                        db.execute('insert into tiles values(?,?,?,?,?,?)',(i,lod,col,row,zlib.crc32(raw),zlib.compress(raw,9)));count+=1;segments+=len(raw)//20
                        if db.execute('pragma page_count').fetchone()[0]*db.execute('pragma page_size').fetchone()[0]>max_bytes:raise ValueError('vector preparation exceeds remaining area budget')
                    levels.append({'unit':unit,'interval_m':interval,'sample_step':step,'simplification_tolerance_projected_m':max(dx,dy)*step*.20,'tiles':count,'segments':segments})
                    total+=count
                areas.append({'area':source.name,'extent':extent,'levels':levels,'source_sha256':hashlib.sha256((source/'dem.tif').read_bytes()).hexdigest(),'source_bytes':(source/'dem.tif').stat().st_size,'request_url':meta['request_url'],'source_dx_m':dx,'source_dy_m':dy,'no_data_cells':int((~valid).sum())})
                print(source.name,levels,flush=True)
            for v in json.loads(labels.read_text()):
                if not v['name'] or len(v['name'].encode())>120 or not all(math.isfinite(v[k]) for k in ['x','y']):raise ValueError('invalid label')
                db.execute('insert or ignore into labels values(?,?,?,?,?)',(v['id'],v['name'],v['kind'],v['x'],v['y']))
            manifest={'schema':1,'crs':'EPSG:3857','areas':areas,'tile_count':total,'label_count':db.execute('select count(*) from labels').fetchone()[0],'contour_units':'meters','algorithm':'marching squares, linear crossings at DEM sample centers, center-resolved saddles, no-data cell guard, connected-chain Douglas Peucker simplification','labels_source':'https://carto.nationalmap.gov/arcgis/rest/services/geonames/MapServer','attribution':'USGS 3DEP and GNIS / The National Map','created_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())}
            db.execute('insert into metadata values(?,?)',('manifest',json.dumps(manifest)));db.commit()
            if db.execute('pragma quick_check').fetchone()[0]!='ok':raise ValueError('integrity failure')
        if tmp.stat().st_size>max_bytes:raise ValueError('vector sidecar exceeds area budget')
        os.replace(tmp,destination);manifest['bytes']=destination.stat().st_size;manifest['sha256']=hashlib.sha256(destination.read_bytes()).hexdigest()
        destination.with_suffix('.json').write_text(json.dumps(manifest,indent=2));return manifest
    finally:
        if tmp.exists():tmp.unlink()

# Rebuild only relief rasters without burned-in contours; preserve the existing Topo product unchanged.
def clean_relief(sources, pack):
    from PIL import Image
    from prepare_terrain_overview import relief
    fd,name=tempfile.mkstemp(prefix='.relief-',dir=pack.parent);os.close(fd);tmp=Path(name)
    try:
        with sqlite3.connect('file:'+str(pack)+'?mode=ro',uri=True) as src,sqlite3.connect(tmp) as db:
            src.backup(db);manifest=json.loads(db.execute("select value from metadata where key='manifest'").fetchone()[0])
            for i,folder in enumerate(sources):
                meta=json.loads((folder/'elevation-export.json').read_text());v,valid,dx,dy=read_dem(folder/'dem.tif',meta)
                image,metrics=relief(v,valid,meta['extent'],dx,dy,contours=False)
                style=i*2;db.execute('delete from tiles where style=?',(style,));levels,written,palette=tiles(db,style,image)
                manifest['styles'][style].update(levels=levels,compressed_bytes=written,palette=palette)
            manifest['relief_contours']='independent vector sidecar; no contours burned into relief'
            db.execute("update metadata set value=? where key='manifest'",(json.dumps(manifest),));db.commit()
            if db.execute('pragma quick_check').fetchone()[0]!='ok':raise ValueError('relief integrity failure')
        if tmp.stat().st_size>128*1024*1024:raise ValueError('relief pack exceeds budget')
        os.replace(tmp,pack);manifest['bytes']=pack.stat().st_size;manifest['sha256']=hashlib.sha256(pack.read_bytes()).hexdigest();pack.with_suffix('.json').write_text(json.dumps(manifest,indent=2))
    finally:
        if tmp.exists():tmp.unlink()

# Require explicit source caches and destinations for reproducible offline preparation.
def main():
    p=argparse.ArgumentParser();p.add_argument('--source',action='append',type=Path,required=True);p.add_argument('--labels',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--replace',action='store_true');p.add_argument('--clean-relief',type=Path)
    a=p.parse_args();result=prepare(a.source,a.labels,a.out,a.replace)
    if a.clean_relief:clean_relief(a.source,a.clean_relief)
    print(json.dumps({'bytes':result['bytes'],'tiles':result['tile_count'],'labels':result['label_count']}))
if __name__=='__main__':main()
