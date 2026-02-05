# Graph v1 Format (Routing)

Binary file: `data/regions/<region>/graph/graph.bin`

All values are little-endian.

## Header
- `char[4] magic` = "MFG1"
- `uint32 version` = 1
- `uint32 node_count`
- `uint32 edge_count`

## Nodes
- `double node_x[node_count]` (Web Mercator meters)
- `double node_y[node_count]`

## Adjacency
- `uint32 edge_start[node_count + 1]` (CSR offsets)

## Edges
- `uint32 edge_to[edge_count]`
- `float edge_length_m[edge_count]`
- `float edge_speed_mps[edge_count]`
- `uint8 edge_class[edge_count]`

## Notes
- Edges are directed.
- `edge_start[i]..edge_start[i+1]` gives outgoing edges for node i.
- Shortest route uses edge_length_m as cost.
- Fastest route uses edge_length_m / edge_speed_mps as cost.
