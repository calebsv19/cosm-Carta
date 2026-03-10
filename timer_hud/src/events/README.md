# Timer Events

Tracks discrete events that can be correlated with timer data (e.g. tagged
log entries).

| File | Responsibility |
| --- | --- |
| `event_tracker.h/c` | Manages a ring buffer of timestamped events that can be displayed alongside timing data. |

Hook the event tracker up to HUD widgets or logging sinks to surface context
for captured timings.
