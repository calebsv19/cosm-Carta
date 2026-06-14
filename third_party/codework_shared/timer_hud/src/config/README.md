# TimerHUD Configuration

Loads and saves HUD settings from JSON so behaviour can be tweaked at runtime.

| File | Responsibility |
| --- | --- |
| `settings_loader.h/c` | Parses `settings.json`, populates the runtime configuration struct, and writes updates back to disk when settings change. |

Current boundary:
- `timer_hud_config.h` is the preferred host-facing settings contract
- `settings_loader.h` remains the persistence/compatibility header

Uses the JSON helper bundled in `TimerHUD/external/`.
