#include "mapforge_region_internal.h"

static const char *k_metric_band_names[METRIC_BAND_COUNT] = {
    "default",
    "coarse",
    "mid",
    "fine"
};

static const char *k_metric_layer_names[METRIC_LAYER_COUNT] = {
    "artery",
    "local",
    "water",
    "park",
    "landuse",
    "building",
    "contour"
};

const char *mapforge_region_metric_band_name(int band) {
    if (band < 0 || band >= METRIC_BAND_COUNT) {
        return NULL;
    }
    return k_metric_band_names[band];
}

const char *mapforge_region_metric_layer_name(int layer) {
    if (layer < 0 || layer >= METRIC_LAYER_COUNT) {
        return NULL;
    }
    return k_metric_layer_names[layer];
}

const char *mapforge_region_archive_layer_from_suffix(const char *suffix) {
    if (!suffix || suffix[0] == '\0') {
        return NULL;
    }
    if (strcmp(suffix, "artery.mft") == 0) {
        return "road_artery";
    }
    if (strcmp(suffix, "local.mft") == 0) {
        return "road_local";
    }
    if (strcmp(suffix, "water.mft") == 0) {
        return "water";
    }
    if (strcmp(suffix, "park.mft") == 0) {
        return "park";
    }
    if (strcmp(suffix, "landuse.mft") == 0) {
        return "landuse";
    }
    if (strcmp(suffix, "building.mft") == 0) {
        return "building";
    }
    if (strcmp(suffix, "contour.mft") == 0) {
        return "contour";
    }
    if (strcmp(suffix, "mft") == 0) {
        return "road_artery";
    }
    return NULL;
}

int mapforge_region_archive_metric_band_index(const char *band) {
    if (!band || band[0] == '\0' || strcmp(band, "default") == 0) {
        return METRIC_BAND_DEFAULT;
    }
    if (strcmp(band, "coarse") == 0) {
        return METRIC_BAND_COARSE;
    }
    if (strcmp(band, "mid") == 0) {
        return METRIC_BAND_MID;
    }
    if (strcmp(band, "fine") == 0) {
        return METRIC_BAND_FINE;
    }
    return -1;
}

int mapforge_region_archive_metric_layer_index(const char *layer) {
    if (!layer || layer[0] == '\0') {
        return -1;
    }
    if (strcmp(layer, "road_artery") == 0) {
        return METRIC_LAYER_ARTERY;
    }
    if (strcmp(layer, "road_local") == 0) {
        return METRIC_LAYER_LOCAL;
    }
    if (strcmp(layer, "water") == 0) {
        return METRIC_LAYER_WATER;
    }
    if (strcmp(layer, "park") == 0) {
        return METRIC_LAYER_PARK;
    }
    if (strcmp(layer, "landuse") == 0) {
        return METRIC_LAYER_LANDUSE;
    }
    if (strcmp(layer, "building") == 0) {
        return METRIC_LAYER_BUILDING;
    }
    if (strcmp(layer, "contour") == 0) {
        return METRIC_LAYER_CONTOUR;
    }
    return -1;
}
