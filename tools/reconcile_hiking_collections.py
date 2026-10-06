#!/usr/bin/env python3
"""Import inspected list manifests through Carta's guarded commands without inferring summit attainment."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
# Sends one bounded structured request through the native owner or standalone repository lock.
def command(binary,runtime,request,work):
    p=work/'request.json';p.write_text(json.dumps(request));run=subprocess.run([str(binary),'--request',str(p)],env=dict(os.environ,MAPFORGE_RUNTIME_DIR=str(runtime)),capture_output=True,text=True)
    if not run.stdout:raise RuntimeError(run.stderr)
    result=json.loads(run.stdout)
    if not result.get('ok'):raise RuntimeError(result.get('error',result))
    return result
# Performs a preview-bound mutation and preserves immutable request identity on retries.
def commit(binary,runtime,request,work,prefix='history'):
    preview=command(binary,runtime,dict(request,operation=prefix+'.preview'),work)
    if preview.get('duplicate'):return preview
    return command(binary,runtime,dict(request,operation=prefix+'.commit',candidate_token=preview['candidate_token'],expected_store_token=preview['store_token']),work)
# Matches source identities rather than approximate route names and retains metadata-only far-away goals.
def reconcile(binary,runtime,manifest,work):
    rows=[];offset=0
    while True:
        page=command(binary,runtime,{'operation':'history.list','include_archived':True,'offset':offset,'limit':100},work);rows+=page['records'];offset+=len(page['records'])
        if offset>=page['total']:break
    prepared=[]
    for collection in manifest['collections']:
        c={k:v for k,v in collection.items() if k!='members'};members=[]
        for member in collection['members']:
            if isinstance(member,str):members.append(member);continue
            kind=member.get('kind','recommended')
            record=next((v for v in rows if v.get('kind')==kind and v.get('external_id')==member['external_id']),None)
            if record is None:
                key=hashlib.sha256((kind+member['external_id']).encode()).hexdigest()[:20]
                allowed={'external_id','name','source_url','source_path','lat','lon','kind','evidence'}
                q={k:v for k,v in member.items() if k in allowed};q.update(request_id='list-reference-'+key,kind=kind)
                record=commit(binary,runtime,q,work)['record'];rows.append(record)
            members.append(record['id'])
        c['members']=members;prepared.append(c)
    key=hashlib.sha256(json.dumps(prepared,sort_keys=True).encode()).hexdigest()[:20]
    return commit(binary,runtime,{'request_id':'list-snapshot-'+key,'collections':prepared},work,'history.collections')
# Runs an explicit manifest import and writes only a local receipt.
def main():
    p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True)
    manifest=json.loads(a.manifest.read_text());result=reconcile(a.binary.resolve(),a.runtime.resolve(),manifest,a.work);(a.work/'reconciliation.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if __name__=='__main__':main()
