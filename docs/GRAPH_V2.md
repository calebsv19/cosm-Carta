# Graph v2 Format (Routing)

Binary file: `data/regions/<region>/graph/graph.bin`

All values are little-endian.

## Header
- `char[4] magic` = "MFG1"
- `uint32 version` = 2
- `uint32 node_count`
- `uint32 edge_count`

## Nodes
- `double node_x[node_count]` (Web Mercator meters)
- `double node_y[node_count]`

## Adjacency
- `uint32 edge_start[node_count + 1]` (CSR offsets)

## Core Edge Fields
- `uint32 edge_to[edge_count]`
- `float edge_length_m[edge_count]`
- `float edge_speed_mps[edge_count]`
- `uint8 edge_class[edge_count]`

## Extended Edge Fields (v2)
- `float edge_speed_limit_mps[edge_count]`
  - Parsed from OSM `maxspeed` when available, else `0`.
- `float edge_grade[edge_count]`
  - Parsed from OSM `incline` when parseable.
  - Forward/reverse directed edges use opposite sign.
- `float edge_penalty[edge_count]`
  - Generic objective penalty bucket from way tags (currently surface-derived heuristic).

## Compatibility Policy
- Runtime loader supports both v1 and v2 graphs.
- v1 behavior on load:
  - `edge_speed_limit_mps` is synthesized from `edge_speed_mps`.
  - `edge_grade` and `edge_penalty` default to `0`.
- Graph builder now emits v2 by default.

## Notes
- Edges are directed.
- `edge_start[i]..edge_start[i+1]` gives outgoing edges for node i.
- Shortest route uses `edge_length_m` cost.
- Fastest route uses `edge_length_m / edge_speed_mps` cost.
- v2 fields are objective inputs for future multi-objective routing modes.
