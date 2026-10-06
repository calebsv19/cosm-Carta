"""Verify real command recovery, independent XML geometry and source byte preservation."""
import copy, hashlib, json, math, os, sqlite3, subprocess, sys, tempfile
from pathlib import Path
import xml.etree.ElementTree as ET
binary=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='carta-history-test-') as tmp:
    root=Path(tmp);env=dict(os.environ,MAPFORGE_RUNTIME_DIR=tmp)
    def call(q,good=True):
        p=root/'request.json';p.write_text(json.dumps(q))
        run=subprocess.run([str(binary),'--request',str(p)],env=env,capture_output=True,text=True)
        assert run.stdout,(q,run.stderr)
        r=json.loads(run.stdout);assert bool(r['ok'])==good,(q,r);assert (run.returncode==0)==good;return r
    def prepared(q):
        p=call(dict(q,operation='history.preview'))
        return dict(q,operation='history.commit',candidate_token=p['candidate_token'],expected_store_token=p['store_token'])
    q={'request_id':'home','name':'Fixture home','location':{'lat':47,'lon':-122,'quality':'manual','source':'fixture'}}
    p=call(dict(q,operation='intake.preview'))
    call(dict(q,operation='intake.commit',candidate_token=p['candidate_token'],expected_store_token=p['store_token']))
    db=root/'pins/places.sqlite'
    def payload():
        with sqlite3.connect(db) as c:return json.loads(c.execute('select payload from document').fetchone()[0])
    original_places=copy.deepcopy(payload()['pins'])
    source=root/'segmented.gpx';data=b'''<gpx xmlns="http://www.topografix.com/GPX/1/1" version="1.1"><trk><name>Two disconnected walks</name><trkseg><trkpt lat="0" lon="0"><ele>10</ele><time>2026-01-01T12:00:00Z</time></trkpt><trkpt lat="0" lon="0.001"><ele>12</ele><time>2026-01-01T12:01:00Z</time></trkpt></trkseg><trkseg><trkpt lat="1" lon="1"/><trkpt lat="1" lon="1.001"/></trkseg></trk></gpx>''';source.write_bytes(data)
    q={'request_id':'first-track','kind':'recorded','source_path':str(source),'external_id':'fixture-activity','name':'Recorded outing'}
    preview=call(dict(q,operation='history.preview'));m=preview['record']['metrics']
    assert m['point_count']==4 and m['segment_count']==2 and m['time_points']==2
    expected=6378137*math.pi/180*.001*(1+math.cos(math.pi/180))
    assert abs(m['distance_m']-expected)<.001,(m,expected)
    commit=prepared(q);r=call(commit);ident=r['record']['id'];assert call(commit)==r
    assert call({'operation':'history.get','record_id':ident})['record']==r['record']
    with sqlite3.connect(db) as c:
        assert c.execute('select original from hiking_assets').fetchone()[0]==data
        assert c.execute('select hash from hiking_assets').fetchone()[0]==hashlib.sha256(data).hexdigest()
    assert call(dict(q,request_id='different-request',operation='history.preview'))['duplicate']
    source.write_bytes(data.replace(b'lon="0.001"',b'lon="0.002"'))
    call(dict(q,request_id='conflicting-source',operation='history.preview'),False)
    source.write_bytes(data)
    point=call({'operation':'history.point','record_id':ident,'index':2})['point'];assert point['segment']==1 and point['elevation_m'] is None
    dest=root/'roundtrip.gpx';call({'operation':'history.export','record_id':ident,'destination':str(dest)});assert dest.read_bytes()==data
    call({'operation':'history.export','record_id':ident,'destination':str(dest)},False)
    stale=prepared({'request_id':'stale','kind':'manual','name':'Unknown-date hike'})
    p=prepared({'request_id':'manual','kind':'manual','name':'No recording','notes':'Recommended route followed'})
    manual=call(p)['record'];assert 'visited_on' not in manual
    call(stale,False)
    archive={'request_id':'archive','action':'archive','record_id':ident,'expected_revision':1}
    call(prepared(archive));assert len(call({'operation':'history.list'})['records'])==1
    all_records=call({'operation':'history.list','include_archived':True})['records'];assert len(all_records)==2
    restore={'request_id':'restore','action':'restore','record_id':ident,'expected_revision':2};call(prepared(restore))
    # A source change after preview must refuse the commit.
    changed={'request_id':'changed','kind':'recommended','source_path':str(source),'external_id':'fixture-trail','name':'Recommended route','evidence':'User states recommended route followed'}
    ready=prepared(changed);source.write_bytes(data.replace(b'lon="0.001"',b'lon="0.002"'));call(ready,False);source.write_bytes(data)
    recommended=call(prepared(changed))['record'];assert recommended['kind']=='recommended'
    call({'operation':'history.list','limit':0},False);call({'operation':'history.list','offset':'0'},False)
    bad=root/'bad.gpx'
    for b in [b'<gpx/>',data.replace(b'lat="0"',b'lat="bogus"'),data.replace(b'<trk>',b'<trk><!DOCTYPE a>'),data.replace(b'version="1.1"',b'version="1.0"')]:
        bad.write_bytes(b);call(dict(q,operation='history.preview',source_path=str(bad)),False)
    # Oversized fields, excessive segments, illegal UTF-8 and NUL bytes fail before publication.
    for b in [data.replace(b'Two disconnected walks',b'x'*4097),data.replace(b'<trk>',b'<trk>'+b'<trkseg/>'*1025),data.replace(b'Two disconnected walks',b'bad'+bytes([255])),data+bytes([0])]:
        bad.write_bytes(b);call(dict(q,operation='history.preview',source_path=str(bad)),False)
    symlink=root/'link.gpx';symlink.symlink_to(source);call(dict(q,operation='history.preview',source_path=str(symlink)),False)
    peak=call(prepared({'request_id':'peak','kind':'peak','name':'Fixture summit','lat':47.4,'lon':-121.5,'evidence':'Fixture coordinates'}))['record']
    visit=call(prepared({'request_id':'visit','kind':'visit','name':'Summit visit','peak_id':peak['id'],'notes':'User confirmed summit'}))['record'];assert visit['peak_id']==peak['id']
    # Proximity is a read-only suggestion and never certifies a summit.
    before=payload();near=call({'operation':'history.nearby','record_id':ident,'radius_m':200});assert near['confirmation_required'] and payload()==before
    call({'operation':'history.nearby','record_id':ident,'radius_m':100000},False)
    # Restore complete assets into a fresh namespace using the real recovery command.
    backup=root/'snapshot.sqlite';counts=call({'operation':'repository.backup','destination':str(backup)})
    assert counts['peaks']==1 and counts['visits']==1 and counts['assets_included']
    corrupt=root/'corrupt.sqlite'
    with sqlite3.connect(backup) as c, sqlite3.connect(corrupt) as d:c.backup(d)
    with sqlite3.connect(corrupt) as c:c.execute('update hiking_assets set original=?',(b'broken source',))
    before=payload();call({'operation':'repository.restore.preview','source':str(corrupt)},False);assert payload()==before
    call({'operation':'history.list','include_archived':'yes'},False)
    optional=root/'optional.gpx';optional.write_bytes(data.replace(b'<ele>10</ele>',b'<ele>bad</ele>').replace(b'12:01:00Z',b'11:59:00Z'))
    metrics=call(dict(q,operation='history.preview',source_path=str(optional),external_id='optional',request_id='optional'))['record']['metrics']
    assert metrics['invalid_optional_fields']==1 and metrics['time_reversals']==1
    invalid=root/'invalid-waypoint.gpx';invalid.write_bytes(data.replace(b'<trk>',b'<wpt lat="oops" lon="0"/><trk>'))
    call(dict(q,operation='history.preview',source_path=str(invalid)),False)
    old_env=env;other=root/'other';other.mkdir();env=dict(env,MAPFORGE_RUNTIME_DIR=str(other))
    call({'operation':'places.search'})
    p=call({'operation':'repository.restore.preview','source':str(backup)})
    call({'operation':'repository.restore','request_id':'recovery','source':str(backup),'source_token':p['source_token'],'expected_store_token':p['store_token']})
    exported=root/'restored.gpx';call({'operation':'history.export','record_id':ident,'destination':str(exported)});assert exported.read_bytes()==data
    env=old_env;assert payload()['pins']==original_places
    assert len(call({'operation':'history.list'})['records'])==5
print('history: segmented distance/source identity, duplicate/replay, stale/source-change refusal, archive/reopen, original export, manual peak visits and complete backup restore passed')
