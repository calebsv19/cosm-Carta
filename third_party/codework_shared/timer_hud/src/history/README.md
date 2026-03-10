# Timer History

Aggregates timing samples across frames for later inspection.

| File | Responsibility |
| --- | --- |
| `history_compression.h/c` | Work-in-progress helpers that compress raw timer samples into rolling history buckets (min/max/avg). |

Integrate this module with logging/UI layers to present historical charts.
