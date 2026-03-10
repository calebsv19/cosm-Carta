# Core Pack v1 Fixtures

This folder contains golden `.pack` fixtures used by Phase 5 stabilization tests.

Files:
- `physics_v1_sample.pack`: Physics profile sample (`VFHD`, `DENS`, `VELX`, `VELY`, `JSON`)
- `daw_v1_sample.pack`: DAW profile sample (`DAWH`, `WMIN`, `WMAX`, `MRKS`, `JSON`)
- `fixtures.sha256`: fixture SHA-256 checksums
- `*_inspect.txt`: reference `pack_cli inspect` output captured at freeze time

Regeneration policy:
- Do not replace fixtures casually.
- Any fixture replacement must accompany a spec update and parser test updates.

Regeneration command:
- `./regenerate_fixtures.sh`

Optional source override:
- `PHYSICS_PACK_SRC=/abs/path/to/physics.pack DAW_PACK_SRC=/abs/path/to/daw.pack ./regenerate_fixtures.sh`
