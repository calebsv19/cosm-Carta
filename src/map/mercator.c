#include "map/mercator.h"

#include <math.h>

static const double kEarthRadiusMeters = 6378137.0;
static const double kMaxLatitude = 85.05112878;

MercatorMeters mercator_from_latlon(LatLon value) {
    double clamped_lat = value.lat;
    if (clamped_lat > kMaxLatitude) {
        clamped_lat = kMaxLatitude;
    } else if (clamped_lat < -kMaxLatitude) {
        clamped_lat = -kMaxLatitude;
    }

    double x = kEarthRadiusMeters * value.lon * (M_PI / 180.0);
    double sin_lat = sin(clamped_lat * (M_PI / 180.0));
    double y = kEarthRadiusMeters * log((1.0 + sin_lat) / (1.0 - sin_lat)) / 2.0;

    MercatorMeters meters = {x, y};
    return meters;
}

LatLon mercator_to_latlon(MercatorMeters value) {
    double lon = (value.x / kEarthRadiusMeters) * (180.0 / M_PI);
    double lat = (2.0 * atan(exp(value.y / kEarthRadiusMeters)) - (M_PI / 2.0)) * (180.0 / M_PI);

    LatLon latlon = {lat, lon};
    return latlon;
}

double mercator_world_size_meters(void) {
    return 2.0 * M_PI * kEarthRadiusMeters;
}
