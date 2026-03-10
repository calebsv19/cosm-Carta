# TimerHUD Logging

Persists timing data to disk for post-run analysis.

| File | Responsibility |
| --- | --- |
| `logger.h/c` | Provides CSV/JSON logging back ends and coordinates flush intervals. |

Configure logging behaviour through the settings loader in `config/`.
