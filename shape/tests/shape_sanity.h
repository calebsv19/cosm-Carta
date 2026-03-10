#pragma once

#include <stdio.h>

// Runs a minimal load/save/rasterize cycle against the first asset in a
// directory. Returns 0 on success, non-zero on failure. When out_log is
// non-null, a short summary is written there.
int shape_sanity_check(const char *asset_dir, FILE *out_log);
