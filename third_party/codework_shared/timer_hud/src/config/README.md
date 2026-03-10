# TimerHUD Configuration

Loads and saves HUD settings from JSON so behaviour can be tweaked at runtime.

| File | Responsibility |
| --- | --- |
| `settings_loader.h/c` | Parses `settings.json`, populates the runtime configuration struct, and writes updates back to disk when settings change. |

Uses the JSON helper bundled in `TimerHUD/external/`.
