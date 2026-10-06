"""Exercise native coverage, exact-file removal, stale edits, path escapes and total byte budgets."""
import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

binary=Path(sys.argv[1]).resolve()
names=('terrain.sqlite','terrain.json','terrain_vectors.sqlite','terrain_vectors.json','source.json')


def fixture(root):
    """Create isolated background-only inputs with no personal repository or user files."""
    base=root/'regions/hiking_terrain_overview';base.mkdir(parents=True)
    (base/'terrain.json').write_text(json.dumps({'styles':[{'extent':[-1000,-1000,1000,1000]},{}]}))
    for name in ('terrain.sqlite','terrain_vectors.sqlite'):(base/name).write_bytes(b'baseline')
    extra=root/'terrain';patch=extra/'glacier-peak';patch.mkdir(parents=True)
    for name in names:(patch/name).write_bytes(b'fixture')
    entry=dict(id='glacier-peak',name='Fixture Glacier',source='USGS fixture',extent=[2000,2000,4000,4000],resolution_m=20,
               bytes=35,files={n:7 for n in names},raster_path='glacier-peak/terrain.sqlite',vector_path='glacier-peak/terrain_vectors.sqlite')
    catalog=dict(schema=1,additional_budget_bytes=128*1024*1024,areas=[entry])
    (extra/'catalog.json').write_text(json.dumps(catalog));return extra,catalog


def run(mode,root):
    """Call production C policy rather than reproduce its acceptance rules in Python."""
    subprocess.run([str(binary),mode,str(root/'regions'),str(root/'terrain')],check=True,capture_output=True,text=True)


for mode in ('accept','remove','stale','base'):
    with tempfile.TemporaryDirectory(prefix='carta-s13-catalog-') as folder:
        root=Path(folder);extra,catalog=fixture(root)
        if mode=='base':(extra/'catalog.json').unlink()
        elif mode=='remove':(extra/'glacier-peak/.DS_Store').write_bytes(b'Finder metadata')
        run(mode,root)
        if mode=='remove':assert not (extra/'glacier-peak').exists()

for case in ('escape','directory-symlink','file-symlink','bytes','bounds','duplicate','extra-file','area-budget','total-budget','type','trailing'):
    with tempfile.TemporaryDirectory(prefix='carta-s13-reject-') as folder:
        root=Path(folder);extra,catalog=fixture(root);entry=catalog['areas'][0]
        if case=='escape':entry['raster_path']='../personal/places.sqlite'
        elif case=='directory-symlink':
            (extra/'glacier-peak').rename(root/'elsewhere');(extra/'glacier-peak').symlink_to(root/'elsewhere',target_is_directory=True)
        elif case=='file-symlink':
            (extra/'glacier-peak/source.json').unlink();(root/'personal.json').write_bytes(b'fixture');(extra/'glacier-peak/source.json').symlink_to(root/'personal.json')
        elif case=='bytes':entry['bytes']=36
        elif case=='bounds':entry['extent']=[4000,2000,2000,4000]
        elif case=='duplicate':catalog['areas'].append(copy.deepcopy(entry))
        elif case=='extra-file':(extra/'glacier-peak/user-note.txt').write_text('preserve')
        elif case=='area-budget':
            size=33*1024*1024
            with (extra/'glacier-peak/terrain.sqlite').open('wb') as f:f.truncate(size)
            entry['files']['terrain.sqlite']=size;entry['bytes']=size+28
        elif case=='total-budget':
            catalog['areas']=[]
            import shutil
            for i in range(5):
                e=copy.deepcopy(entry);e['id']=f'patch-{i}';e['raster_path']=f'patch-{i}/terrain.sqlite';e['vector_path']=f'patch-{i}/terrain_vectors.sqlite'
                shutil.copytree(extra/'glacier-peak',extra/e['id'])
                size=27*1024*1024
                with (extra/e['id']/'terrain.sqlite').open('wb') as f:f.truncate(size)
                e['files']['terrain.sqlite']=size;e['bytes']=size+28;catalog['areas'].append(e)
            shutil.rmtree(extra/'glacier-peak')
        elif case=='type':catalog['schema']='1'
        (extra/'catalog.json').write_text(json.dumps(catalog)+('garbage' if case=='trailing' else ''))
        run('reject',root)
print('catalog: coverage/no coverage, protected base, removal, stale token, malformed/path/symlink and whole-output budgets passed')
