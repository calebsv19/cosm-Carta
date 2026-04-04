#ifndef MAPFORGE_APP_RUNTIME_INPUT_POLICY_H
#define MAPFORGE_APP_RUNTIME_INPUT_POLICY_H

#include "core/input.h"

#include <stdbool.h>
#include <stdint.h>

// Apply text-entry shortcut gating in-place; returns number of blocked shortcuts.
uint32_t app_runtime_apply_text_entry_shortcut_policy(InputState *input,
                                                      bool text_entry_active);

#endif
