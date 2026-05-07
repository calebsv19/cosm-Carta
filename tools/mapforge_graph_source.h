#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum OSMSourceKind {
    OSM_SOURCE_KIND_XML = 0,
    OSM_SOURCE_KIND_PBF = 1,
    OSM_SOURCE_KIND_UNKNOWN = 2
} OSMSourceKind;

OSMSourceKind detect_osm_source_kind(const char *path);
bool convert_pbf_to_xml(const char *pbf_path, char *xml_path, size_t xml_path_cap);
