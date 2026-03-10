# sys_shims Overlay

This folder provides optional header overlays used for `SHIM_MODE=shadow` tests.

Each standard header name maps to a local shim wrapper under `sys_shims/`.
This allows existing `#include <...>` code paths to be tested against shim behavior
without changing application source files.
