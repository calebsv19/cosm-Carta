#!/usr/bin/env python3
"""Prepare a bounded offline USGS overview; the native app has no Python/network dependency."""
import argparse, hashlib, json, math, os, sqlite3, tempfile, time, urllib.parse, urllib.request, zlib
from pathlib import Path
import numpy as np
from PIL import Image
TILE=256; GUTTER=1; APP_ID=0x4d465450
ELEVATION='https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer'
TOPO='https://basemap.nationalmap.gov/arcgis/rest/services/USGSTopo/MapServer'

# Bound every official response before saving it in the explicitly chosen work directory.
def fetch(url, destination, cap):
    with urllib.request.urlopen(url, timeout=60) as response:
        data=response.read(cap+1)
    if len(data)>cap: raise ValueError('source response exceeds budget')
    destination.write_bytes(data)
    return data

# Retrieve one explicit crop, honoring advertised limits and retaining exact returned georeferencing.
def download(work, bounds, detail=False):
    lon0,lat0,lon1,lat1=bounds
    def project(lon,lat): return 6378137*math.radians(lon),6378137*math.log(math.tan(math.pi/4+math.radians(lat)/2))
    x0,y0=project(lon0,lat0);x1,y1=project(lon1,lat1)
    for key,base,fmt in [('elevation',ELEVATION,'tiff'),('topo',TOPO,'png32')]:
        service=json.loads(fetch(base+'?f=pjson',work/(key+'-service.json'),1024*1024))
        width=(min(4096,service['maxImageWidth']) if detail else math.ceil((x1-x0)/45)) if key=='elevation' else min(4096,service['maxImageWidth'])
        height=(math.ceil((y1-y0)/45) if key=='elevation' and not detail else round(width*(y1-y0)/(x1-x0)))
        if width>service['maxImageWidth'] or height>service['maxImageHeight']: raise ValueError('extent exceeds service dimensions')
        params={'f':'json','bbox':','.join(map(str,[x0,y0,x1,y1])),'bboxSR':3857,'imageSR':3857,'size':f'{width},{height}','format':fmt,'adjustAspectRatio':'false'}
        if key=='elevation': params.update(pixelType='F32',interpolation='RSP_BilinearInterpolation',renderingRule=json.dumps({'rasterFunction':'None'}))
        else: params.update(transparent='false',dpi=96)
        url=base+('/exportImage?' if key=='elevation' else '/export?')+urllib.parse.urlencode(params)
        reply=json.loads(fetch(url,work/(key+'-export.json'),1024*1024))
        if 'href' not in reply: raise ValueError(reply)
        if urllib.parse.urlparse(reply['href']).hostname!=urllib.parse.urlparse(base).hostname: raise ValueError('unexpected export host')
        reply['request_url']=url
        (work/(key+'-export.json')).write_text(json.dumps(reply,indent=2))
        fetch(reply['href'],work/('dem.tif' if key=='elevation' else 'topo.png'),64*1024*1024)

# Validate north-up metric TIFF georeferencing against the returned service extent.
def read_dem(path, export):
    image=Image.open(path)
    if image.mode!='F' or image.size!=(export['width'],export['height']): raise ValueError('expected one float32 elevation band')
    tags=image.tag_v2;scale=tags.get(33550);tie=tags.get(33922)
    if not scale or not tie or tags.get(34264): raise ValueError('unsupported DEM georeferencing')
    extent=export['extent'];dx=(extent['xmax']-extent['xmin'])/image.width;dy=(extent['ymax']-extent['ymin'])/image.height
    if max(abs(scale[0]-dx),abs(scale[1]-dy),abs(tie[3]-extent['xmin']),abs(tie[4]-extent['ymax']))>.1: raise ValueError('DEM transform differs from source receipt')
    geokeys=tags.get(34735,());keys={geokeys[i]:geokeys[i+3] for i in range(4,len(geokeys),4) if geokeys[i+1]==0}
    if keys.get(3072) not in (3857,102100): raise ValueError('expected EPSG:3857 DEM')
    values=np.array(image,dtype=np.float32);valid=np.isfinite(values)&(values>-1000)&(values<10000)
    # Sparse GeoTIFF tiles have zero offsets; Pillow otherwise decodes TIFF header bytes as heights.
    offsets=tags.get(324);counts=tags.get(325);tw=tags.get(322);th=tags.get(323)
    if offsets and counts and tw and th:
        cols=math.ceil(image.width/tw)
        if len(offsets)!=cols*math.ceil(image.height/th) or len(counts)!=len(offsets): raise ValueError('invalid TIFF tile directory')
        for i,(offset,count) in enumerate(zip(offsets,counts)):
            if offset==0 or count==0:
                row,col=divmod(i,cols);valid[row*th:min((row+1)*th,image.height),col*tw:min((col+1)*tw,image.width)]=False
    values=np.where(valid,values,0)
    nodata=tags.get(42113)
    if nodata:
        try: valid &= values!=float(str(nodata).strip('\x00'))
        except ValueError: raise ValueError('unrecognized no-data tag')
    return values,valid,dx,dy

# Derive physical-slope hillshade and 100 m/500 m contour pixels without crossing no-data cells.
def relief(values,valid,extent,dx,dy,contours=True):
    h,w=values.shape
    clean=np.where(valid,values,0)
    latitude=2*np.arctan(np.exp((extent['ymax']-(np.arange(h)+.5)*dy)/6378137))-math.pi/2
    spacing=np.cos(latitude)[:,None]
    gy,gx=np.gradient(clean,dy,dx);gx=gx/spacing;gy=-gy/spacing
    normal=np.sqrt(1+gx*gx+gy*gy);lighting=np.zeros_like(values)
    for azimuth in [315,270,45]:
        a=math.radians(azimuth);alt=math.radians(45)
        lighting+=np.maximum(0,(-gx*math.sin(a)*math.cos(alt)-gy*math.cos(a)*math.cos(alt)+math.sin(alt))/normal)/3
    shade=.48+.52*lighting
    levels=[-1000,0,250,500,1000,1500,2200,10000]
    colors=np.array([[192,216,228],[205,224,199],[199,217,182],[191,204,168],[211,210,181],[219,209,193],[236,231,220],[249,249,247]])
    rgb=np.stack([np.interp(clean,levels,colors[:,c])*shade for c in range(3)],axis=-1)
    safe=valid.copy();safe[1:,:]&=valid[:-1,:];safe[:-1,:]&=valid[1:,:];safe[:,1:]&=valid[:,:-1];safe[:,:-1]&=valid[:,1:]
    contour=np.zeros_like(valid);major=np.zeros_like(valid)
    for step,target in [(100,contour),(500,major)]:
        bands=np.floor(clean/step)
        target[1:,:]|=(bands[1:,:]!=bands[:-1,:]);target[:,1:]|=(bands[:,1:]!=bands[:,:-1]);target &= safe & (clean>25)
    if contours:
        rgb[contour]=rgb[contour]*.80;rgb[major]=rgb[major]*.70
    rgba=np.empty((h,w,4),dtype=np.uint8);rgba[:,:,:3]=np.clip(rgb,0,255);rgba[:,:,3]=safe*255
    return Image.fromarray(rgba),{'valid_cells':int(valid.sum()),'no_data_cells':int((~valid).sum()),'contour_cells':int(contour.sum()),'elevation_min_m':float(values[valid].min()),'elevation_max_m':float(values[valid].max()),'contour_interval_m':100,'index_contour_interval_m':500,'hillshade':'three azimuths 315/270/45, altitude 45; physical slope corrected for Mercator scale; no vertical exaggeration'}

# Tile one image with neighboring texel gutters and a fixed five-level offline pyramid.
def tiles(db,style,image):
    # A shared per-raster palette compresses map colors efficiently without reducing spatial resolution.
    reference=image.convert('RGB').quantize(colors=255,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    colors=reference.getpalette()[:255*3];colors += [0]*(255*3-len(colors))
    reference.putpalette(colors+colors[-3:])
    palette=np.array([[0,0,0,0]]+[colors[i:i+3]+[255] for i in range(0,255*3,3)],dtype=np.uint8)
    levels=[];written=0
    for lod in range(5):
        w=math.ceil(image.width/2**lod);h=math.ceil(image.height/2**lod)
        scaled=image if lod==0 else image.resize((w,h),Image.Resampling.BOX)
        mapped=scaled.convert('RGB').quantize(palette=reference,dither=Image.Dither.NONE)
        pixels=np.minimum(np.asarray(mapped),254).astype(np.uint8)+1
        pixels[np.asarray(scaled)[:,:,3]==0]=0
        padded=np.pad(pixels,((1,1),(1,1)),mode='edge');cols=math.ceil(w/TILE);rows=math.ceil(h/TILE)
        for row in range(rows):
            for col in range(cols):
                raw=np.zeros((TILE+2,TILE+2),dtype=np.uint8);x,y=col*TILE,row*TILE
                part=padded[y:min(y+TILE+2,h+2),x:min(x+TILE+2,w+2)];raw[:part.shape[0],:part.shape[1]]=part
                compressed=zlib.compress(raw.tobytes(),6);written+=len(compressed)
                db.execute('insert into tiles values(?,?,?,?,?,?)',(style,lod,col,row,zlib.crc32(palette[raw].tobytes()),compressed))
        levels.append({'width':w,'height':h,'cols':cols,'rows':rows})
    return levels,written,palette.tolist()

# Atomically publish only a complete, size-bounded terrain database and its source receipt.
def prepare(work,destination,replace=False,max_mib=128,details=()):
    if destination.exists() and not replace: raise ValueError('destination exists; explicit --replace required')
    if not 1<=max_mib<=256: raise ValueError('invalid pack budget')
    sources=[work]+list(details)
    if len(sources)>5: raise ValueError('at most four detail areas per overview')
    destination.parent.mkdir(parents=True,exist_ok=True)
    temp=destination.with_name(destination.name+'.preparing')
    if temp.exists(): raise ValueError('unfinished preparation exists; inspect it before retry')
    try:
        with sqlite3.connect(temp) as db:
            db.execute(f'pragma application_id={APP_ID}');db.execute('pragma user_version=1');db.execute('pragma journal_mode=DELETE');db.execute('pragma synchronous=FULL')
            db.execute('create table metadata(key text primary key,value text not null)')
            db.execute('create table tiles(style integer,lod integer,col integer,row integer,crc integer not null,rgba blob not null,primary key(style,lod,col,row)) without rowid')
            styles=[];all_metrics=[];provenance=[]
            for area_index,folder in enumerate(sources):
                elevation=json.loads((folder/'elevation-export.json').read_text());topo=json.loads((folder/'topo-export.json').read_text())
                values,valid,dx,dy=read_dem(folder/'dem.tif',elevation);image,metrics=relief(values,valid,elevation['extent'],dx,dy)
                context=Image.open(folder/'topo.png').convert('RGBA')
                if context.size!=(topo['width'],topo['height']): raise ValueError('context dimensions differ from source receipt')
                area='Overview' if area_index==0 else folder.name.removeprefix('mapforge-s8-detail-')
                all_metrics.append(dict(area=area,**metrics,sample_spacing_projected_m=[dx,dy],sample_spacing_ground_m_at_center=dx*math.cos(math.radians(47.69))))
                for mode,(name,source,picture) in enumerate([('Relief',elevation,image),('Topo',topo,context)]):
                    levels,compressed,palette=tiles(db,len(styles),picture);e=source['extent']
                    styles.append({'name':name,'area':area,'mode':mode,'extent':[e[k] for k in ['xmin','ymin','xmax','ymax']],'levels':levels,'compressed_bytes':compressed,'palette':palette,'encoding':'indexed8_zlib'})
                for key,file,base in [('elevation','dem.tif',ELEVATION),('topo','topo.png',TOPO)]:
                    service=json.loads((folder/(key+'-service.json')).read_text());source=json.loads((folder/(key+'-export.json')).read_text())
                    provenance.append({'area':area,'service':base,'attribution':service['copyrightText'],'source_sha256':hashlib.sha256((folder/file).read_bytes()).hexdigest(),'source_bytes':(folder/file).stat().st_size,'request_url':source.get('request_url'),'extent':source['extent'],'retrieved_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime((folder/file).stat().st_mtime))})
                del values,valid,image,context
            manifest={'schema':1,'name':'Seattle–Cascades hiking overview with local detail','crs':'EPSG:3857','tile_size':TILE,'gutter':GUTTER,'styles':styles,'metrics':all_metrics,'vertical_units':'meters','vertical_datum':'USGS multi-resolution service; not independently resolved for every source cell','created_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'sources':provenance}
            db.execute('insert into metadata values(?,?)',('manifest',json.dumps(manifest,ensure_ascii=False,separators=(',',':'))));db.commit()
            if db.execute('pragma integrity_check').fetchone()[0]!='ok':raise ValueError('pack integrity failed')
            manifest['tile_count']=db.execute('select count(*) from tiles').fetchone()[0]
        if temp.stat().st_size>max_mib*1024*1024:raise ValueError('finished pack exceeds budget')
        with temp.open('rb') as f:os.fsync(f.fileno())
        temp.chmod(0o644);os.replace(temp,destination)
        manifest['pack_bytes']=destination.stat().st_size;manifest['pack_sha256']=hashlib.sha256(destination.read_bytes()).hexdigest()
        destination.with_suffix('.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False))
        return manifest
    finally:
        if temp.exists():temp.unlink()

# Require an explicit source directory and output before making or replacing terrain files.
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--work',type=Path,required=True);parser.add_argument('--out',type=Path,required=True);parser.add_argument('--fetch',action='store_true');parser.add_argument('--replace',action='store_true');parser.add_argument('--detail',type=Path,action='append',default=[]);parser.add_argument('--max-mib',type=int,default=128);parser.add_argument('--bounds',type=float,nargs=4,default=[-122.48,47.35,-121.38,48.03]);args=parser.parse_args()
    if not (-180<=args.bounds[0]<args.bounds[2]<=180 and -85<args.bounds[1]<args.bounds[3]<85):parser.error('invalid lon/lat bounds')
    args.work.mkdir(parents=True,exist_ok=True)
    if args.fetch:download(args.work,args.bounds)
    print(json.dumps(prepare(args.work,args.out,args.replace,args.max_mib,args.detail),indent=2))
if __name__=='__main__':main()
