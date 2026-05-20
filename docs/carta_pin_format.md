# Carta Pin Format

`Carta` headless jobs resolve named locations from a JSON pin file.

## Version

- current version: `1`

## Shape

```json
{
  "version": 1,
  "map_region": "seattle",
  "pins": [
    {
      "id": "demo_start",
      "name": "Demo Start",
      "type": "demo",
      "color": "blue",
      "lat": 47.6710,
      "lon": -122.3490,
      "notes": "Example pin only",
      "private": false
    }
  ]
}
```

## Required Fields

- root:
  - `version`
  - `pins`
- per pin:
  - `id`
  - `name`
  - `lat`
  - `lon`

## Optional Fields

- root:
  - `map_region`
- per pin:
  - `type`
  - `color`
  - `notes`
  - `private`

## Current Contract

- pins are resolved by exact `id` first and exact `name` second
- coordinates are stored as latitude/longitude and converted into Web Mercator meters at runtime
- the first headless foundation patch resolves each pin to the nearest graph node
- private real-world pins should stay in ignored local files under `data/pins/private/`
