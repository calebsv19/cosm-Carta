#include "app/history.h"
#include "map/geodesic.h"
#include <libxml/xmlreader.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GPX_MAX_BYTES (16u * 1024u * 1024u)
#define GPX_MAX_POINTS 200000u
// Reports a bounded refusal without weakening an existing personal record.
static bool fail(char *error, size_t cap, const char *message) {
    if (cap)
        snprintf(error, cap, "%s", message);
    return false;
}
// Hashes exact source bytes using the host's maintained cryptographic implementation.
static void digest(const void *bytes, size_t n, char out[65]) {
    unsigned char hash[32];
#ifdef __APPLE__
    CC_SHA256(bytes, (CC_LONG)n, hash);
#else
    SHA256(bytes, n, hash);
#endif
    for (unsigned i = 0; i < 32; i++)
        snprintf(out + i * 2, 3, "%02x", hash[i]);
}
// Releases owned source bytes and normalized points.
void history_track_free(HistoryTrack *t) {
    if (t) {
        free(t->bytes);
        free(t->points);
        memset(t, 0, sizeof(*t));
    }
}
// Parses one finite scalar with no unconsumed non-whitespace suffix.
static bool number(const char *s, double *out) {
    if (!s || !*s)
        return false;
    char *end = NULL;
    errno = 0;
    *out = strtod(s, &end);
    while (end && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
        ++end;
    return !errno && end != s && end && !*end && isfinite(*out);
}
// Converts explicit-zone ISO timestamps to UTC seconds; unavailable values stay NaN.
static double timestamp(const char *s) {
    int y, m, d, h, mi, used = 0;
    double sec = 0;
    if (!s || strlen(s) < 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':')
        return NAN;
    for (int i = 0; i < 19; i++)
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && (s[i] < '0' || s[i] > '9'))
            return NAN;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%lf%n", &y, &m, &d, &h, &mi, &sec, &used) != 6 || y < 1900 ||
        y > 9999 || m < 1 || m > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 ||
        sec >= 60)
        return NAN;
    const char *z = s + used;
    int oh = 0, om = 0, offset = 0;
    if (!strcmp(z, "Z"))
        offset = 0;
    else {
        int n = 0;
        if (strlen(z) != 6 || z[3] != ':' || z[1] < '0' || z[1] > '9' || z[2] < '0' || z[2] > '9' ||
            z[4] < '0' || z[4] > '9' || z[5] < '0' || z[5] > '9' || (*z != '+' && *z != '-') ||
            sscanf(z + 1, "%2d:%2d%n", &oh, &om, &n) != 2 || z[1 + n] || oh < 0 || oh > 23 || om < 0 ||
            om > 59)
            return NAN;
        offset = (oh * 60 + om) * 60 * (*z == '+' ? 1 : -1);
    }
    struct tm t = {0};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = (int)sec;
    time_t epoch = timegm(&t);
    if (t.tm_year != y - 1900 || t.tm_mon != m - 1 || t.tm_mday != d)
        return NAN;
    return (double)epoch + sec - floor(sec) - offset;
}
// Matches a GPX element only in its official namespace.
static bool named(xmlTextReaderPtr r, const char *name) {
    const xmlChar *local = xmlTextReaderConstLocalName(r), *ns = xmlTextReaderConstNamespaceUri(r);
    return local && ns && !strcmp((const char *)local, name) &&
           !strcmp((const char *)ns, "http://www.topografix.com/GPX/1/1");
}
// Validates and normalizes bounded segmented GPX track geometry.
bool history_parse(const void *bytes, size_t size, HistoryTrack *out, char *error, size_t cap) {
    HistoryTrack t = {0};
    if (cap)
        error[0] = 0;
    if (!bytes || !size || size > GPX_MAX_BYTES || memchr(bytes, 0, size))
        return fail(error, cap, "GPX byte limit, encoding or empty input refused");
    t.bytes = malloc(size + 1);
    if (!t.bytes)
        return fail(error, cap, "GPX allocation failed");
    memcpy(t.bytes, bytes, size);
    t.bytes[size] = 0;
    t.byte_count = size;
    if (strstr((const char *)t.bytes, "<!DOCTYPE") || strstr((const char *)t.bytes, "<!ENTITY")) {
        history_track_free(&t);
        return fail(error, cap, "GPX DTD/entities refused");
    }
    digest(bytes, size, t.hash);
    xmlTextReaderPtr r = xmlReaderForMemory((const char *)bytes, (int)size, NULL, "UTF-8",
                                            XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!r) {
        history_track_free(&t);
        return fail(error, cap, "Cannot create GPX reader");
    }
    t.min_lat = t.min_lon = INFINITY;
    t.max_lat = t.max_lon = -INFINITY;
    int status = 0;
    size_t nodes = 0, all_points = 0, tracks = 0, point_capacity = 0;
    bool root = false, in_track = false, in_seg = false, in_point = false, have_ele = false,
         have_time = false;
    int field = 0, field_depth = -1;
    char value[4097] = {0};
    size_t value_n = 0;
    HistoryPoint p = {0};
    while ((status = xmlTextReaderRead(r)) == 1) {
        int depth = xmlTextReaderDepth(r), type = xmlTextReaderNodeType(r);
        const xmlChar *node_value = xmlTextReaderConstValue(r);
        if (++nodes > 1000000 || depth > 32 || xmlTextReaderAttributeCount(r) > 64 ||
            (node_value && xmlStrlen(node_value) > 4096))
            goto refused;
        if (type == XML_READER_TYPE_DOCUMENT_TYPE || type == XML_READER_TYPE_ENTITY_REFERENCE)
            goto refused;
        if (type == XML_READER_TYPE_ELEMENT) {
            if (xmlTextReaderMoveToFirstAttribute(r) == 1) {
                do {
                    const xmlChar *v = xmlTextReaderConstValue(r);
                    if (v && xmlStrlen(v) > 4096)
                        goto refused;
                } while (xmlTextReaderMoveToNextAttribute(r) == 1);
                xmlTextReaderMoveToElement(r);
            }
            if (depth == 0) {
                xmlChar *version = xmlTextReaderGetAttribute(r, (const xmlChar *)"version");
                bool valid = !root && named(r, "gpx") && version && !strcmp((char *)version, "1.1");
                xmlFree(version);
                if (!valid)
                    goto refused;
                root = true;
            } else if (depth == 1 && named(r, "trk")) {
                if (++tracks > 128)
                    goto refused;
                in_track = true;
            } else if (depth == 2 && in_track && named(r, "trkseg")) {
                if (++t.segment_count > 1024)
                    goto refused;
                in_seg = true;
            } else if (named(r, "trkpt") || named(r, "rtept") || named(r, "wpt")) {
                if (++all_points > GPX_MAX_POINTS)
                    goto refused;
                if (named(r, "rtept") || named(r, "wpt")) {
                    xmlChar *la = xmlTextReaderGetAttribute(r, (const xmlChar *)"lat"),
                            *lo = xmlTextReaderGetAttribute(r, (const xmlChar *)"lon");
                    double a, b;
                    bool valid =
                        number((char *)la, &a) && number((char *)lo, &b) && fabs(a) <= 90 && fabs(b) <= 180;
                    xmlFree(la);
                    xmlFree(lo);
                    if (!valid)
                        goto refused;
                    if (named(r, "rtept"))
                        ++t.route_point_count;
                    else
                        ++t.waypoint_count;
                }
                if (depth == 3 && in_track && in_seg && named(r, "trkpt")) {
                    if (in_point)
                        goto refused;
                    xmlChar *lat = xmlTextReaderGetAttribute(r, (const xmlChar *)"lat"),
                            *lon = xmlTextReaderGetAttribute(r, (const xmlChar *)"lon");
                    p = (HistoryPoint){
                        .elevation = NAN, .time_s = NAN, .segment = (unsigned)t.segment_count - 1};
                    bool valid = number((char *)lat, &p.lat) && number((char *)lon, &p.lon) &&
                                 fabs(p.lat) <= 85.05112878 && fabs(p.lon) <= 180;
                    xmlFree(lat);
                    xmlFree(lon);
                    if (!valid)
                        goto refused;
                    in_point = true;
                    have_ele = have_time = false;
                }
            }
            if (depth == 2 && in_track && named(r, "name")) {
                field = 1;
                field_depth = depth;
                value_n = 0;
                value[0] = 0;
            }
            if (depth == 4 && in_point && (named(r, "ele") || named(r, "time"))) {
                field = named(r, "ele") ? 2 : 3;
                if ((field == 2 && have_ele) || (field == 3 && have_time))
                    goto refused;
                if (field == 2)
                    have_ele = true;
                else
                    have_time = true;
                field_depth = depth;
                value_n = 0;
                value[0] = 0;
            }
        }
        if ((type == XML_READER_TYPE_TEXT || type == XML_READER_TYPE_CDATA ||
             type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) &&
            field && depth == field_depth + 1) {
            const xmlChar *v = xmlTextReaderConstValue(r);
            size_t n = v ? strlen((const char *)v) : 0;
            if (value_n + n > 4096)
                goto refused;
            memcpy(value + value_n, v, n);
            value_n += n;
            value[value_n] = 0;
        }
        bool end = type == XML_READER_TYPE_END_ELEMENT ||
                   (type == XML_READER_TYPE_ELEMENT && xmlTextReaderIsEmptyElement(r));
        if (end) {
            if (field && depth == field_depth) {
                if (field == 1 && !t.name[0])
                    snprintf(t.name, sizeof(t.name), "%s", value);
                if (field == 2 && !number(value, &p.elevation))
                    p.elevation = NAN;
                if (field == 3)
                    p.time_s = timestamp(value);
                field = 0;
            }
            if (depth == 3 && named(r, "trkpt") && in_point) {
                if (t.point_count == GPX_MAX_POINTS)
                    goto refused;
                if (t.point_count == point_capacity) {
                    size_t next_capacity = point_capacity ? point_capacity * 2 : 256;
                    if (next_capacity > GPX_MAX_POINTS)
                        next_capacity = GPX_MAX_POINTS;
                    HistoryPoint *next = realloc(t.points, next_capacity * sizeof(*next));
                    if (!next)
                        goto refused;
                    t.points = next;
                    point_capacity = next_capacity;
                }
                if (t.point_count && t.points[t.point_count - 1].segment == p.segment) {
                    HistoryPoint prev = t.points[t.point_count - 1];
                    double step =
                        ground_distance_meters((LatLon){prev.lat, prev.lon}, (LatLon){p.lat, p.lon});
                    t.distance_m += step;
                    t.large_steps += step > 1000;
                    t.time_reversals += isfinite(prev.time_s) && isfinite(p.time_s) && p.time_s < prev.time_s;
                }
                p.distance_m = t.distance_m;
                t.points[t.point_count++] = p;
                t.min_lat = fmin(t.min_lat, p.lat);
                t.max_lat = fmax(t.max_lat, p.lat);
                t.min_lon = fmin(t.min_lon, p.lon);
                t.max_lon = fmax(t.max_lon, p.lon);
                t.elevation_count += isfinite(p.elevation);
                t.time_count += isfinite(p.time_s);
                t.invalid_optional_count +=
                    (have_ele && !isfinite(p.elevation)) + (have_time && !isfinite(p.time_s));
                in_point = false;
            }
            if (depth == 2 && named(r, "trkseg"))
                in_seg = false;
            if (depth == 1 && named(r, "trk"))
                in_track = false;
        }
    }
    xmlFreeTextReader(r);
    if (status < 0 || !root || !t.point_count) {
        history_track_free(&t);
        return fail(error, cap, "Invalid GPX or no recorded/reference track geometry");
    }
    HistoryPoint *exact = realloc(t.points, t.point_count * sizeof(*exact));
    if (!exact) {
        history_track_free(&t);
        return fail(error, cap, "GPX normalization allocation failed");
    }
    t.points = exact;
    t.track_count = tracks;
    *out = t;
    return true;
refused:
    xmlFreeTextReader(r);
    history_track_free(&t);
    return fail(error, cap, "GPX structure, coordinates or resource limits refused");
}
// Reads a complete bounded regular file before parsing it.
bool history_read(const char *path, HistoryTrack *out, char *error, size_t cap) {
    if (!path || path[0] != '/')
        return fail(error, cap, "Select an absolute GPX path");
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    struct stat st;
    if (fd < 0)
        return fail(error, cap, "Cannot read GPX source");
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > GPX_MAX_BYTES) {
        close(fd);
        return fail(error, cap, "GPX source type/size refused");
    }
    size_t n = (size_t)st.st_size, done = 0;
    unsigned char *data = malloc(n);
    bool ok = data != NULL;
    while (ok && done < n) {
        ssize_t count = read(fd, data + done, n - done);
        if (count <= 0)
            ok = false;
        else
            done += (size_t)count;
    }
    unsigned char extra;
    if (ok && read(fd, &extra, 1) != 0)
        ok = false;
    close(fd);
    if (ok)
        ok = history_parse(data, n, out, error, cap);
    else
        fail(error, cap, "GPX changed or could not be read");
    free(data);
    return ok;
}
