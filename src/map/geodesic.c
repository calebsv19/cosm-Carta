#include "map/geodesic.h"

#include <math.h>

double ground_distance_meters(LatLon a, LatLon b) {
    const double radians = 0.017453292519943295;
    const double radius_m = 6378137.0;
    double lat_a = a.lat * radians;
    double lat_b = b.lat * radians;
    double sin_lat = sin((lat_b - lat_a) * 0.5);
    double sin_lon = sin((b.lon - a.lon) * radians * 0.5);
    double h = sin_lat * sin_lat + cos(lat_a) * cos(lat_b) * sin_lon * sin_lon;
    if (h < 0.0) h = 0.0;
    if (h > 1.0) h = 1.0;
    return 2.0 * radius_m * atan2(sqrt(h), sqrt(1.0 - h));
}

double mercator_ground_distance_meters(MercatorMeters a, MercatorMeters b) {
    return ground_distance_meters(mercator_to_latlon(a), mercator_to_latlon(b));
}
