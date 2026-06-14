# core_wake Roadmap

## Mission
Decouple kernel wake behavior from UI/event frameworks so runtime orchestration remains portable and headless-safe.

## Immediate Steps
1. Keep SDL or other event-loop adapters outside `core_wake`; only the external callback boundary belongs here.
2. Add any further timeout coverage only if a host exposes a concrete regression beyond the current cond/external lifecycle suite.
3. Decide later whether wake observability belongs in this module or in a higher orchestration layer.
4. Review whether shutdown coordination helpers are needed, without pulling kernel or app policy into the wake primitive.

## Future Steps
1. Add explicit platform backend files (`*_mac.c`, `*_posix.c`, `*_win.c`) as needed.
2. Add wake coalescing policy hooks if event storms require it.
3. Lock stable wake contract for `core_kernel` policy modes.
