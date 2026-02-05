#ifndef MAPFORGE_MAP_MERCATOR_H
#define MAPFORGE_MAP_MERCATOR_H

// Stores latitude/longitude in degrees.
typedef struct LatLon {
    double lat;
    double lon;
} LatLon;

// Stores Web Mercator meters (EPSG:3857).
typedef struct MercatorMeters {
    double x;
    double y;
} MercatorMeters;

// Converts lat/lon degrees to Web Mercator meters.
MercatorMeters mercator_from_latlon(LatLon value);

// Converts Web Mercator meters to lat/lon degrees.
LatLon mercator_to_latlon(MercatorMeters value);

// Returns the Web Mercator world size in meters.
double mercator_world_size_meters(void);

#endif
