#ifndef MAPFORGE_MAP_GEODESIC_H
#define MAPFORGE_MAP_GEODESIC_H

#include "map/mercator.h"

// Measures the spherical ground distance between WGS84 latitude/longitude points.
double ground_distance_meters(LatLon a, LatLon b);

// Measures ground distance between projected points without treating map units as ground units.
double mercator_ground_distance_meters(MercatorMeters a, MercatorMeters b);

#endif
