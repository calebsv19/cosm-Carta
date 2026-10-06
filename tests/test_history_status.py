"""Exercise personal truth, collection membership, migration and geometry preservation through commands."""
import hashlib,json,os,sqlite3,subprocess,sys,tempfile
from pathlib import Path
binary=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='carta-s10-status-') as tmp:
    root=Path(tmp);env=dict(os.environ,MAPFORGE_RUNTIME_DIR=tmp)
    def call(q,ok=True):
        request=root/'request.json';request.write_text(json.dumps(q));p=subprocess.run([str(binary),'--request',str(request)],env=env,capture_output=True,text=True)
        assert p.stdout,(q,p.stderr);r=json.loads(p.stdout);assert r['ok']==ok,(q,r);return r
    def save(q,prefix='history'):
        p=call(dict(q,operation=prefix+'.preview'));return call(dict(q,operation=prefix+'.commit',candidate_token=p['candidate_token'],expected_store_token=p['store_token']))
    def document():
        with sqlite3.connect(root/'pins/places.sqlite') as c:return json.loads(c.execute('select payload from document').fetchone()[0])
    call({'operation':'places.search'})
    source=root/'source.gpx';data=b'<gpx xmlns="http://www.topografix.com/GPX/1/1" version="1.1"><trk><trkseg><trkpt lat="47.5" lon="-121.7"/><trkpt lat="47.501" lon="-121.7"/></trkseg></trk></gpx>';source.write_bytes(data)
    activity=save({'request_id':'recording','kind':'recorded','name':'Observed movement','source_path':str(source)})['record']
    falls=save({'request_id':'falls','kind':'recommended','name':'Teneriffe Falls','external_id':'falls','source_path':str(source),'recording_id':activity['id'],'evidence':'User states recommended route followed'})['record']
    bryant=save({'request_id':'bryant','kind':'recommended','name':'Bryant Peak','external_id':'bryant','source_path':str(source)})['record']
    peak=save({'request_id':'peak','kind':'peak','name':'Mount Teneriffe','lat':47.5,'lon':-121.7})['record']
    si=save({'request_id':'si','kind':'peak','name':'Mount Si','lat':47.49,'lon':-121.74})['record']
    before=document();preview=call({'operation':'history.migrate.preview','request_id':'migrate'});assert document()==before
    result=save({'request_id':'migrate'},'history.migrate');assert call({'operation':'history.migrate.commit','request_id':'migrate','candidate_token':preview['candidate_token'],'expected_store_token':preview['store_token']})==result
    assert document()['hiking_status_version']==1
    refs={v['id']:v for v in document()['hiking']};assert refs[falls['id']]['progress']=='unreviewed';assert refs[falls['id']]['legacy_import_claim'];assert not refs[peak['id']]['tracked']
    collection={'id':'completed-exact','name':'Completed','role':'completed','url':'https://www.alltrails.com/explore/list/test','observed_at':'2026-10-05','members':[falls['id']]}
    save({'request_id':'exact-list','collections':[collection]},'history.collections')
    assert [v['id'] for v in call({'operation':'history.list','progress':'completed'})['records']]==[activity['id'],falls['id']]
    # A completed Falls itinerary cannot complete either summit, and an imported badge cannot undo a user correction.
    assert not any(v['kind']=='peak' for v in call({'operation':'history.list','progress':'completed'})['records'])
    def status(v,value,rid):return save({'request_id':rid,'action':'status','record_id':v['id'],'expected_revision':v['revision'],'progress':value,'status_evidence':'Explicit fixture user correction'})['record']
    bryant=status(bryant,'not_done','bryant-not-done');peak=status(peak,'not_done','teneriffe-not-summited');si=status(si,'not_done','si-not-done')
    collection['members']=[falls['id'],bryant['id']];save({'request_id':'list-reimport','collections':[collection]},'history.collections')
    refs={v['id']:v for v in document()['hiking']};assert refs[bryant['id']]['progress']=='not_done';assert refs[bryant['id']]['collections']==['completed-exact'];assert not refs[si['id']]['tracked']
    prior={k:peak[k] for k in ('progress','state','tracked','status_origin','status_evidence') if k in peak}
    temp=status(peak,'planned','temporary-plan')
    restored=save({'request_id':'undo-exact','action':'status_restore','record_id':peak['id'],'expected_revision':temp['revision'],'status_snapshot':prior})['record']
    assert all(restored.get(k)==peak.get(k) for k in ('progress','state','tracked','status_origin','status_evidence'));peak=restored
    planned=status(peak,'planned','plan-peak');assert planned['tracked'] and planned['state']=='wishlist'
    partial=status(planned,'partial','partial-peak');assert partial['state']=='visited'
    done=status(partial,'completed','summit-peak');assert done['state']=='summited'
    call({'operation':'history.preview','request_id':'invalid','action':'status','record_id':done['id'],'expected_revision':done['revision'],'progress':'maybe'},False)
    # Invalid identities and stale snapshots refuse without changing any authoritative bytes.
    before=document();bad=dict(collection,members=['missing']);call({'operation':'history.collections.preview','request_id':'bad-members','collections':[bad]},False);assert document()==before
    pending={'request_id':'stale-list','collections':[dict(collection,members=[])]};p=call(dict(pending,operation='history.collections.preview'))
    bryant=status(refs[bryant['id']],'planned','plan-bryant')
    call(dict(pending,operation='history.collections.commit',candidate_token=p['candidate_token'],expected_store_token=p['store_token']),False)
    # Far-away reference metadata stays cataloged without a mandatory map or GPX download.
    far=save({'request_id':'far','kind':'recommended','name':'Far away goal','external_id':'far-trail','source_url':'https://www.alltrails.com/trail/example'})['record'];assert 'asset_hash' not in far
    goals={'id':'weekend','name':'Weekend goals','role':'planned','members':[bryant['id'],far['id']]};save({'request_id':'goal-list','collections':[goals]},'history.collections')
    # Membership removal changes collection-derived claims only, preserving user-owned progress and immutable sources.
    save({'request_id':'remove-completed','collections':[dict(collection,members=[])]},'history.collections')
    refs={v['id']:v for v in document()['hiking']};assert refs[falls['id']]['progress']=='unreviewed';assert refs[bryant['id']]['progress']=='planned'
    with sqlite3.connect(root/'pins/places.sqlite') as c:
        assets=c.execute('select hash,original from hiking_assets').fetchall();assert assets==[(hashlib.sha256(data).hexdigest(),data)]
    backup=root/'backup.sqlite';call({'operation':'repository.backup','destination':str(backup)})
    with sqlite3.connect(backup) as c:assert json.loads(c.execute('select payload from document').fetchone()[0])['hiking_collections']==document()['hiking_collections']
print('S10 status: conservative migration, exact membership, user corrections, separate summits, status transitions, stale guards, metadata-only goals and complete backup passed')
