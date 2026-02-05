# OSM Road Classification (Phase 2)

This document maps OSM `highway=*` tags to MapForge `RoadClass` values.

## Mapping
- motorway, motorway_link -> ROAD_CLASS_MOTORWAY
- trunk, trunk_link -> ROAD_CLASS_TRUNK
- primary, primary_link -> ROAD_CLASS_PRIMARY
- secondary, secondary_link -> ROAD_CLASS_SECONDARY
- tertiary, tertiary_link -> ROAD_CLASS_TERTIARY
- residential -> ROAD_CLASS_RESIDENTIAL
- service -> ROAD_CLASS_SERVICE
- footway, cycleway, pedestrian, steps -> ROAD_CLASS_FOOTWAY
- path, track -> ROAD_CLASS_PATH

## Defaults
- Any other `highway` value maps to `ROAD_CLASS_RESIDENTIAL` for now.

## Notes
- This is intentionally minimal for Phase 2.
- Later phases can refine handling for unclassified, track, path, footway, etc.
