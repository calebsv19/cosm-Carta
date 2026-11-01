"""Exercise continuity isolation and hard refusals without rendering or region writes."""
from pathlib import Path
import json,os,shutil,subprocess,tempfile,unittest

class ContinuityIsolationTest(unittest.TestCase):
    # Runs the real harness against a synthetic process that persists its final camera.
    def test_profiles_preserve_caller_and_start_identically(self):
        with tempfile.TemporaryDirectory(prefix='carta-continuity-isolation-') as temp:
            root=Path(temp);repo=root/'repo'
            for directory in ['tests','config','build/targets/macOS-arm64/tools']:
                (repo/directory).mkdir(parents=True,exist_ok=True)
            shutil.copy2(Path(__file__).with_name('test_phase_b_continuity_stress.sh'),repo/'tests/stress.sh')
            seed={'map_view':{'zoom':12.981203079223633}}
            (repo/'config/app.config.json').write_text(json.dumps(seed))
            validator=repo/'build/targets/macOS-arm64/tools/mapforge_region_validate'
            validator.write_text('#!/bin/sh\nexit "${VALIDATOR_EXIT:-0}"\n');validator.chmod(0o755)
            helper=root/'target';helper.write_text('#!/bin/sh\necho macOS-arm64\n');helper.chmod(0o755)
            fake=root/'app'
            fake.write_text('''#!/usr/bin/env python3
import os,json
from pathlib import Path
r=Path(os.environ['MAPFORGE_RUNTIME_DIR']);h=Path(os.environ['HOME'])
assert r != Path(os.environ['CALLER_RUNTIME'])
assert h != Path(os.environ['CALLER_HOME'])
assert os.environ['MAPFORGE_RUNTIME_CONFIG_PATH']==str(r/'app_state.json')
state=json.loads((r/'app_state.json').read_text())
with open(os.environ['OBSERVATIONS'],'a') as f:f.write(json.dumps({'state':state,'runtime':str(r)})+'\\n')
(r/'app_state.json').write_text('{"map_view":{"zoom":99}}')
(r/'pins').mkdir();(r/'pins/synthetic.json').write_text('{}')
t=r/'traces';t.mkdir();(t/('mapforge_trace_'+r.parent.name+'.pack')).write_bytes(b'trace')
for i in range(6):
 print('perf_phase_a cov(global=1.000) l0(lat_ms=0.00) cache(evict=0) churn(frame_band=0 frame_rebuild=0)')
 print('perf region=synthetic draw_path(vk=0 fallback=1) band_fallback='+os.environ.get('SYNTHETIC_FALLBACK','0'))
 if not os.environ.get('OMIT_CONTINUITY'):print('perf_continuity band_fallback='+os.environ.get('SYNTHETIC_FRAME_FALLBACK','0'))
''');fake.chmod(0o755)
            caller=root/'caller';caller.mkdir();home=root/'home';home.mkdir()
            (caller/'app_state.json').write_text('{"map_view":{"zoom":17}}')
            (caller/'pins.json').write_text('personal sentinel')
            before={p.name:p.read_bytes() for p in caller.iterdir()}
            observations=root/'observations'
            env=os.environ|{'TARGET_CONTRACT_HELPER':str(helper),'MAPFORGE_BINARY':str(fake),'MAPFORGE_RUNTIME_DIR':str(caller),'MAPFORGE_RUNTIME_CONFIG_PATH':str(caller/'app_state.json'),'HOME':str(home),'CALLER_RUNTIME':str(caller),'CALLER_HOME':str(home),'OBSERVATIONS':str(observations),'TMPDIR':str(root),'MAPFORGE_PHASE_B_MAX_ATTEMPTS':'1'}
            def run(extra=None):
                return subprocess.run(['bash',str(repo/'tests/stress.sh')],env=env|(extra or {}),capture_output=True,text=True)
            for _ in range(2):
                result=run();self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            rows=[json.loads(line) for line in observations.read_text().splitlines()]
            self.assertEqual([row['state'] for row in rows],[seed,seed])
            self.assertNotEqual(rows[0]['runtime'],rows[1]['runtime'])
            self.assertTrue(all(not Path(row['runtime']).exists() for row in rows))
            self.assertEqual(len(list((repo/'build/traces').glob('*.pack'))),2)
            # Missing regions and an actual continuity breach still fail closed.
            invalid=run({'VALIDATOR_EXIT':'1'})
            self.assertNotEqual(invalid.returncode,0)
            self.assertIn('precondition failed',invalid.stderr)
            self.assertEqual(len(observations.read_text().splitlines()),2)
            breach=run({'SYNTHETIC_FALLBACK':'65'})
            self.assertNotEqual(breach.returncode,0)
            self.assertIn('band_fallback_violations',breach.stderr)
            self.assertEqual({p.name:p.read_bytes() for p in caller.iterdir()},before)
            self.assertIn('retained isolated scenario',breach.stderr)
            # A peak missed by periodic draw logs still blocks the gate.
            transient=run({'SYNTHETIC_FRAME_FALLBACK':'65'})
            self.assertNotEqual(transient.returncode,0)
            self.assertIn('band_fallback_violations',transient.stderr)
            missing=run({'OMIT_CONTINUITY':'1'})
            self.assertNotEqual(missing.returncode,0)
            self.assertIn('missing per-frame fallback telemetry',missing.stderr)
            self.assertEqual({p.name:p.read_bytes() for p in caller.iterdir()},before)

if __name__=='__main__':unittest.main()
