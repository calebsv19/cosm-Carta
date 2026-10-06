"""Exercise the real repository command boundary with disposable synthetic records."""
import copy
import importlib.util
import json
import math
import struct
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
spec = importlib.util.spec_from_file_location('intake', Path(__file__).resolve().parents[1] / 'tools/place_intake.py')
intake = importlib.util.module_from_spec(spec)
spec.loader.exec_module(intake)

with tempfile.TemporaryDirectory(prefix='carta-place-test-') as temp:
    root = Path(temp)
    os.environ['MAPFORGE_RUNTIME_DIR'] = temp
    def call(request, good=True):
        path = root/'request.json'
        path.write_text(json.dumps(request))
        p = subprocess.run([str(binary),'--request',str(path)],capture_output=True,text=True)
        reply=json.loads(p.stdout)
        assert bool(reply['ok']) == good, (request,reply,p.stderr)
        assert (p.returncode==0) == good
        return reply
    def prepare(request):
        preview=call(dict(request,operation='intake.preview'))
        return dict(request,operation='intake.commit',expected_store_token=preview['store_token'],candidate_token=preview['candidate_token'])
    legacy={'version':2,'future_root':{'keep':True},'pins':[{'id':'legacy-id','name':'Legacy','lat':47.66,'lon':-122.3,'collections':['hiking','future_collection'],'future_pin':{'keep':42}}]}
    (root/'pins').mkdir()
    (root/'pins/places.local.json').write_text(json.dumps(legacy))
    assert call({'operation':'places.get','place_id':'legacy-id'})['places'][0]['future_pin']=={'keep':42}
    assert json.loads((root/'pins/places.local.json').read_text())==legacy
    db=root/'pins/places.sqlite'
    q={'request_id':'synthetic-one','name':'Synthetic home','address':'100 Test Ave NE, Seattle, WA 98105','location':{'lat':47.66,'lon':-122.31,'source':'synthetic','quality':'manual','input_address':'Original unmatched input address','matched_address':'Map matched address'},'pet':{'name':'Miso','species':'dog'},'services':['dog_walking','pet_sitting']}
    committed=prepare(q)
    reply=call(committed)
    place=reply['place_id']; pet=reply['pet_id']
    assert call(committed)==reply
    changed=dict(committed,name='Changed request')
    assert 'candidate' in call(changed,False)['error']
    r=call({'operation':'places.get','place_id':place})['places'][0]
    assert len(r['pets'])==1 and len(r['services'])==2
    assert call({'operation':'places.search','query':'unmatched input'})['places'][0]['id']==place
    assert set(r['collections'])=={'general','dog_walking','pet_sitting'}
    stale=prepare(dict(q,request_id='stale',name='Stale'))
    second=copy.deepcopy(q);second['request_id']='second-unit';second['address']+=' UNIT 2'
    second_result=call(prepare(second));assert second_result['place_id']!=place and second_result['pet_id']!=pet
    assert 'stale' in call(stale,False)['error'].lower()
    third=dict(q,request_id='second-service',pet={'id':pet,'name':'Miso'},services=['drop_in'])
    assert call(prepare(third))['place_id']==place
    r=call({'operation':'places.get','place_id':place})['places'][0]
    assert len(r['pets'])==1 and len(r['services'])==3
    assert len(call({'operation':'places.search','query':'Miso'})['places'])==2
    bad=dict(q,request_id='invalid-coordinate',location={'lat':999,'lon':0,'quality':'manual'})
    call(dict(bad,operation='intake.preview'),False)
    update={'request_id':'correction','operation':'places.update','place_id':place,'expected_revision':r['revision'],'location':{'lat':47.6602,'lon':-122.31,'source':'manual','quality':'manual'}}
    preview=call(dict(update,operation='intake.preview'))
    update.update(expected_store_token=preview['store_token'],candidate_token=preview['candidate_token'])
    call(update)
    call(dict(update,request_id='stale-edit'),False)
    # Saved intents reopen on the current graph, detect corrections, and refuse changed previews.
    region=root/'regions/fixture'
    (region/'graph').mkdir(parents=True)
    (region/'meta.json').write_text(json.dumps({'bounds':{'min_lat':47.65,'max_lat':47.67,'min_lon':-122.32,'max_lon':-122.30}}))
    radius=6378137.0
    xy=[(radius*math.radians(-122.31),radius*math.log(math.tan(math.pi/4+math.radians(lat)/2))) for lat in [47.66,47.661]]
    length=radius*math.radians(.001)
    data=b'MFG1'+struct.pack('<III',4,2,2)
    for fmt,values in [('2d',[v[0] for v in xy]),('2d',[v[1] for v in xy]),('3I',[0,1,2]),('2I',[1,0]),('2f',[length,length]),('2f',[10,10]),('2B',[5,5]),('2f',[10,10]),('2f',[0,0]),('2f',[0,0]),('2B',[3,3])]:
        data+=struct.pack('<'+fmt,*values)
    (region/'graph/graph.bin').write_bytes(data)
    os.environ['MAPFORGE_REGIONS_DIR']=str(root/'regions')
    route={'operation':'routes.preview','request_id':'route-one','name':'Synthetic walk','region':'fixture','mode':'walk','objective':'distance','from_place_id':place,'to_place_id':second_result['place_id']}
    preview=call(route);assert 20 < preview['distance_m'] < 25
    save=dict(route,operation='routes.save',preview_token=preview['preview_token'])
    saved=call(save);assert call(save)==saved
    assert not call({'operation':'routes.get','route_id':saved['id']})['stale']
    # Offline intent reads preserve snapshots and never depend on graph availability.
    conn=sqlite3.connect(db)
    before_list=conn.execute('select payload from document').fetchone()[0]
    offline=root/'unavailable-regions'
    os.environ['MAPFORGE_REGIONS_DIR']=str(offline)
    listing=call({'operation':'routes.list','limit':1})
    assert listing['total']==1 and listing['next_offset'] is None
    assert not listing['recomputed'] and listing['graph_status']=='not_checked'
    row=listing['routes'][0]
    assert row['saved_intent']==saved and row['from_place_name']=='Synthetic home'
    assert not row['coordinates_changed'] and not row['from_place_missing']
    assert not call({'operation':'routes.list','offset':1})['routes']
    for field,value in [('limit',0),('limit',101),('limit',True),('limit',1.5),('offset',-1),('offset','0')]:
        call({'operation':'routes.list',field:value},False)
    call({'operation':'routes.get','route_id':saved['id']},False)
    assert conn.execute('select payload from document').fetchone()[0]==before_list
    conn.close()
    os.environ['MAPFORGE_REGIONS_DIR']=str(root/'regions')
    second_route=dict(route,request_id='route-two',name='Second synthetic walk')
    second_preview=call(second_route)
    second_saved=call(dict(second_route,operation='routes.save',preview_token=second_preview['preview_token']))
    first_page=call({'operation':'routes.list','limit':1})
    assert first_page['total']==2 and first_page['next_offset']==1
    second_page=call({'operation':'routes.list','limit':1,'offset':first_page['next_offset']})
    assert second_page['routes'][0]['saved_intent']['id']==second_saved['id'] and second_page['next_offset'] is None
    call(dict(save,name='conflicting'),False)
    r=call({'operation':'places.get','place_id':place})['places'][0]
    correction=dict(update,request_id='route-correction',expected_revision=r['revision'],location={'lat':47.6604,'lon':-122.31,'source':'synthetic','quality':'manual'})
    pv=call(dict(correction,operation='intake.preview'))
    correction.update(expected_store_token=pv['store_token'],candidate_token=pv['candidate_token'])
    call(correction)
    changed=call({'operation':'routes.get','route_id':saved['id']})
    assert changed['stale'] and changed['distance_m'] > preview['distance_m']
    listed=call({'operation':'routes.list'})['routes'][0]
    assert listed['coordinates_changed'] and listed['saved_intent']==saved
    # A legacy/imported missing reference is inspectable without changing its identity.
    missing_runtime=root/'missing-reference'
    (missing_runtime/'pins').mkdir(parents=True)
    (missing_runtime/'pins/places.local.json').write_text(json.dumps({'version':2,'pins':[], 'route_plans':[saved]}))
    os.environ['MAPFORGE_RUNTIME_DIR']=str(missing_runtime)
    missing=call({'operation':'routes.list'})['routes'][0]
    assert missing['from_place_missing'] and missing['to_place_missing']
    assert missing['from_place_name'] is None and missing['saved_intent']==saved
    # Invalid extension payloads are refused instead of coerced or partially listed.
    for invalid_plans in ({'unexpected':'object'}, [None]):
        (missing_runtime/'pins/places.local.json').write_text(json.dumps({'version':2,'pins':[], 'route_plans':invalid_plans}))
        # A separate namespace makes each legacy payload its own independent source.
        invalid_runtime=root/('invalid-object' if isinstance(invalid_plans,dict) else 'invalid-row')
        (invalid_runtime/'pins').mkdir(parents=True)
        (invalid_runtime/'pins/places.local.json').write_text(json.dumps({'version':2,'pins':[], 'route_plans':invalid_plans}))
        os.environ['MAPFORGE_RUNTIME_DIR']=str(invalid_runtime)
        call({'operation':'routes.list'},False)
    os.environ['MAPFORGE_RUNTIME_DIR']=temp
    call(dict(save,request_id='stale-preview'),False)
    assert call(dict(route,to_place_id=place))['distance_m']==0
    assert call({'operation':'intake.receipt','request_id':'synthetic-one'})['receipt']['place_id']==place
    # The helper must replay a stored request without repeating network lookup.
    helper=dict(q,request_id='helper-retry')
    (root/'helper.json').write_text(json.dumps(helper))
    args=[sys.executable,str(Path(intake.__file__)), '--input',str(root/'helper.json'),'--binary',str(binary),'--commit']
    first=subprocess.run(args,capture_output=True,text=True,check=True)
    second=subprocess.run(args+['--resolve','seattle_address_points'],capture_output=True,text=True,check=True)
    assert json.loads(first.stdout)==json.loads(second.stdout)
    helper['name']='Different input';(root/'helper.json').write_text(json.dumps(helper))
    assert subprocess.run(args,capture_output=True,text=True).returncode!=0
    # Native route management preserves intent offline, refuses stale revisions, and is reversible.
    os.environ['MAPFORGE_REGIONS_DIR']=str(offline)
    rename={'operation':'routes.rename','route_id':saved['id'],'request_id':'rename-one','expected_revision':0,'name':'Renamed walk'}
    renamed=call(rename);assert call(rename)==renamed
    assert renamed['from_place_id']==saved['from_place_id'] and renamed['graph_token']==saved['graph_token']
    call(dict(rename,request_id='stale-rename'),False)
    call(dict(rename,name='conflict'),False)
    archive={'operation':'routes.archive','route_id':saved['id'],'request_id':'archive-one','expected_revision':1,'archived':True}
    archived=call(archive);assert call(archive)==archived and archived['archived']
    assert any(x['saved_intent']['archived'] for x in call({'operation':'routes.list'})['routes'] if x['saved_intent']['id']==saved['id'])
    call({'operation':'routes.get','route_id':saved['id']},False)
    call(dict(archive,request_id='archive-wrong-type',expected_revision=2,archived='false'),False)
    restored=call(dict(archive,request_id='unarchive-one',expected_revision=2,archived=False));assert not restored['archived']
    os.environ['MAPFORGE_REGIONS_DIR']=str(root/'regions')
    assert call({'operation':'routes.get','route_id':saved['id']})['stale']
    # Replace one home's profile with stable pets and per-pet roles; detach preserves global records.
    before_second=call({'operation':'places.get','place_id':second_result['place_id']})['places'][0]
    r=call({'operation':'places.get','place_id':place})['places'][0]
    profile={'pets':[{'id':pet,'name':'Miso corrected','species':'dog','services':['dog_walking']},
                     {'id':'native-pet-synthetic-new','name':'Second pet','species':'','services':['pet_sitting']}], 'home_services':[]}
    req={'request_id':'profile-one','place_id':place,'expected_revision':r['revision'],'address':'Corrected display address','notes':'Keep the home point','profile':profile}
    pv=call(dict(req,operation='intake.preview'))
    commit=dict(req,operation='places.update',expected_store_token=pv['store_token'],candidate_token=pv['candidate_token'])
    answer=call(commit);assert call(commit)==answer
    updated=call({'operation':'places.get','place_id':place})['places'][0]
    assert updated['lat']==r['lat'] and updated['lon']==r['lon'] and updated['location_evidence']==r['location_evidence']
    assert len(updated['pets'])==2 and len(updated['services'])==2
    assert {'dog_walking','pet_sitting'} <= set(updated['collections'])
    assert call({'operation':'places.get','place_id':second_result['place_id']})['places'][0]==before_second
    invalid=copy.deepcopy(req);invalid['request_id']='bad-profile';invalid['expected_revision']=updated['revision'];invalid['profile']['pets'].append(copy.deepcopy(invalid['profile']['pets'][0]))
    call(dict(invalid,operation='intake.preview'),False)
    invalid=copy.deepcopy(req);invalid['request_id']='bad-service';invalid['expected_revision']=updated['revision'];invalid['profile']['pets'][0]['services']=['unsupported']
    call(dict(invalid,operation='intake.preview'),False)
    detach=dict(req,request_id='detach-profile',expected_revision=updated['revision'],profile={'pets':[],'home_services':['pet_sitting']})
    pv=call(dict(detach,operation='intake.preview'))
    call(dict(detach,operation='places.update',expected_store_token=pv['store_token'],candidate_token=pv['candidate_token']))
    detached=call({'operation':'places.get','place_id':place})['places'][0]
    assert not detached['pets'] and len(detached['services'])==1 and 'dog_walking' not in detached['collections']
    conn=sqlite3.connect(db);doc=json.loads(conn.execute('select payload from document').fetchone()[0]);conn.close()
    assert any(x['id']==pet for x in doc['pets']) and any(x['id']=='native-pet-synthetic-new' for x in doc['pets'])
    assert doc['place_history'][-1]['previous_pets'] and doc['route_history']
    # Consistent backups/export and preview-bound restore; malformed/future/stale sources are refused.
    backup=root/'snapshot.sqlite';export=root/'export.json'
    backup_reply=call({'operation':'repository.backup','destination':str(backup)})
    assert oct(backup.stat().st_mode & 0o777)=='0o600'
    call({'operation':'repository.backup','destination':str(backup)},False)
    call({'operation':'repository.export','destination':str(export)})
    assert json.loads(export.read_text())==doc and oct(export.stat().st_mode & 0o777)=='0o600'
    call({'operation':'repository.export','destination':str(export)},False)
    restore_preview=call({'operation':'repository.restore.preview','source':str(backup)})
    assert restore_preview['places']==len(doc['pins']) and restore_preview['source_token']==backup_reply['store_token']
    restore_req={'operation':'repository.restore','source':str(backup),'request_id':'restore-one','expected_store_token':restore_preview['store_token'],'source_token':restore_preview['source_token']}
    # A intervening supported edit invalidates a restore preview.
    rename2=dict(rename,request_id='rename-two',expected_revision=3,name='Newer name');call(rename2)
    call(restore_req,False)
    restore_preview=call({'operation':'repository.restore.preview','source':str(backup)})
    restore_req['expected_store_token']=restore_preview['store_token']
    call(dict(restore_req,source_token='bad'),False)
    result=call(restore_req);assert call(restore_req)==result
    assert Path(result['recovery_backup']).exists()
    conn=sqlite3.connect(db);now=json.loads(conn.execute('select payload from document').fetchone()[0]);conn.close()
    assert {k:v for k,v in now.items() if k!='last_restore_receipt'}==doc
    assert json.loads(export.read_text())==doc
    bad_backup=root/'future.sqlite';bad_backup.write_bytes(backup.read_bytes());conn=sqlite3.connect(bad_backup);conn.execute('pragma user_version=999');conn.close()
    call({'operation':'repository.restore.preview','source':str(bad_backup)},False)
    malformed=root/'malformed.sqlite';malformed.write_bytes(b'not sqlite')
    call({'operation':'repository.restore.preview','source':str(malformed)},False)
    symlink=root/'alias.sqlite';symlink.symlink_to(backup)
    call({'operation':'repository.restore.preview','source':str(symlink)},False)
    call({'operation':'repository.backup','destination':str(symlink)},False)
    # Constraint failure must roll back both the authoritative document and relational indexes.
    conn=sqlite3.connect(db)
    before=conn.execute('select payload from document').fetchone()[0]
    payload=json.loads(before)
    assert payload['future_root']=={'keep':True}
    conn.execute("create trigger reject_service before insert on services begin select raise(abort,'synthetic write failure'); end")
    conn.commit()
    rejected=dict(q,request_id='rollback-test',address='999 Test Ave NE, Seattle, WA 98105')
    call(prepare(rejected),False)
    assert conn.execute('select payload from document').fetchone()[0]==before
    assert conn.execute('pragma foreign_key_check').fetchall()==[]
    assert conn.execute('pragma integrity_check').fetchone()[0]=='ok'
    conn.execute('drop trigger reject_service');conn.commit();conn.close()
    # A held native-owner lock must never be bypassed by the CLI.
    import fcntl
    with open(str(db)+'.lock','a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        p=subprocess.run([str(binary),'--request',str(root/'request.json')],capture_output=True,text=True)
        assert p.returncode and 'Owner unavailable' in p.stderr

point={'address':'100 TEST AVE NE, SEATTLE, WA, 98105','score':100,'location':{'x':-122.3,'y':47.66},'attributes':{'Addr_type':'PointAddress','Status':'M','City':'SEATTLE'}}
assert intake.seattle_candidates('100 Test Avenue Northeast Seattle Washington 98105',{'candidates':[point]})[0]['quality']=='address_point'
assert intake.seattle_candidates('101 Test Ave NE Seattle WA 98105',{'candidates':[point]})[0]['quality']=='approximate'
assert intake.seattle_candidates('100 Test Ave NE UNIT 2 Seattle WA 98105',{'candidates':[point]})[0]['quality']=='approximate'
assert intake.seattle_candidates('anything',{'candidates':[]})==[]
try: intake.seattle_candidates('anything',{'error':{'code':500}})
except ValueError: pass
else: raise AssertionError('Provider failure accepted')
print('place intake: migration, metadata, retries, units, pets/services, stale writes, rollback, writer lock, saved-route staleness, helper replay and geocoder fixtures passed')
