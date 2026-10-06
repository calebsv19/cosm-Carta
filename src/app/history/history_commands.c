#include "app/history.h"
#include "app/history_status.h"
#include "map/geodesic.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Reads typed bounded metadata without coercing numbers to text.
static const char *text(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    return o && json_object_object_get_ex(o, k, &v) && json_object_is_type(v, json_type_string)
               ? json_object_get_string(v)
               : "";
}
// Creates an independent metadata snapshot.
static struct json_object *clone(struct json_object *o) {
    return json_tokener_parse(json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
}
// Builds an explicit command outcome shared by native and agent callers.
static struct json_object *reply(bool ok, const char *s) {
    struct json_object *r = json_object_new_object();
    json_object_object_add(r, "ok", json_object_new_boolean(ok));
    json_object_object_add(r, ok ? "status" : "error", json_object_new_string(s));
    return r;
}
// Returns a catalog without mutating the loaded authoritative document.
static struct json_object *array(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, k, &v);
    return v && json_object_is_type(v, json_type_array) ? v : NULL;
}
// Locates a stable history identity in an app-owned catalog.
static struct json_object *find(struct json_object *a, const char *id) {
    for (size_t i = 0; a && i < json_object_array_length(a); i++) {
        struct json_object *v = json_object_array_get_idx(a, i);
        if (!strcmp(text(v, "id"), id))
            return v;
    }
    return NULL;
}
// Hashes bounded request metadata for preview/retry identity.
static void token(const char *s, char out[32]) {
    uint64_t h = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= UINT64_C(1099511628211);
    }
    snprintf(out, 32, "%016llx", (unsigned long long)h);
}
// Formats the exact personal document comparison token.
static void store_token(const MapForgePinsFile *f, char out[32]) {
    snprintf(out, 32, "%016llx", (unsigned long long)f->source_signature);
}
// Supplies a bounded UTC metadata creation time, independent of visit date.
static void stamp(char out[64]) {
    time_t now = time(NULL);
    struct tm t;
    gmtime_r(&now, &t);
    strftime(out, 64, "%Y-%m-%dT%H:%M:%SZ", &t);
}
// Checks one optional text field's type/size/embedded NUL before retaining it.
static bool valid_text(struct json_object *r, const char *k, size_t limit) {
    struct json_object *v = NULL;
    if (!json_object_object_get_ex(r, k, &v))
        return true;
    return json_object_is_type(v, json_type_string) &&
           strlen(json_object_get_string(v)) == (size_t)json_object_get_string_len(v) &&
           strlen(json_object_get_string(v)) <= limit;
}
// Exposes source coverage and geometry metrics without copying point arrays into metadata.
static struct json_object *metrics(const HistoryTrack *t) {
    struct json_object *r = json_object_new_object();
    json_object_object_add(r, "source_bytes", json_object_new_int64((int64_t)t->byte_count));
    json_object_object_add(r, "point_count", json_object_new_int64((int64_t)t->point_count));
    json_object_object_add(r, "track_count", json_object_new_int64(t->track_count));
    json_object_object_add(r, "segment_count", json_object_new_int64((int64_t)t->segment_count));
    json_object_object_add(r, "distance_m", json_object_new_double(t->distance_m));
    json_object_object_add(r, "elevation_points", json_object_new_int64((int64_t)t->elevation_count));
    json_object_object_add(r, "route_points_ignored", json_object_new_int64(t->route_point_count));
    json_object_object_add(r, "waypoints_preserved_in_original", json_object_new_int64(t->waypoint_count));
    json_object_object_add(r, "time_points", json_object_new_int64((int64_t)t->time_count));
    json_object_object_add(r, "invalid_optional_fields", json_object_new_int64(t->invalid_optional_count));
    json_object_object_add(r, "time_reversals", json_object_new_int64(t->time_reversals));
    json_object_object_add(r, "large_steps", json_object_new_int64(t->large_steps));
    json_object_object_add(r, "elevation_datum", json_object_new_string("unspecified"));
    struct json_object *b = json_object_new_array();
    double v[] = {t->min_lat, t->min_lon, t->max_lat, t->max_lon};
    for (unsigned i = 0; i < 4; i++)
        json_object_array_add(b, json_object_new_double(v[i]));
    json_object_object_add(r, "bounds", b);
    return r;
}
// Writes an original export create-only so an existing file is never overwritten.
static bool export_asset(const char *dest, const HistoryTrack *t) {
    if (dest[0] != '/')
        return false;
    int fd = open(dest, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0)
        return false;
    size_t n = 0;
    bool ok = true;
    while (n < t->byte_count) {
        ssize_t count = write(fd, t->bytes + n, t->byte_count - n);
        if (count <= 0) {
            ok = false;
            break;
        }
        n += (size_t)count;
    }
    if (fsync(fd))
        ok = false;
    if (close(fd))
        ok = false;
    if (!ok)
        unlink(dest);
    return ok;
}
// Dispatches versioned history reads and guarded durable mutations.
struct json_object *history_command(const char *path, MapForgePinsFile *file, struct json_object *request) {
    const char *op = text(request, "operation");
    struct json_object *records = array(file->source_json, "hiking");
    struct json_object *schema = NULL, *raw = NULL;
    if (json_object_object_get_ex(file->source_json, "hiking", &raw) &&
        (!json_object_is_type(raw, json_type_array) ||
         !json_object_object_get_ex(file->source_json, "hiking_version", &schema) ||
         !json_object_is_type(schema, json_type_int) || json_object_get_int(schema) != 1))
        return reply(false, "Unsupported hiking catalog; preserve it");
    if (!strncmp(op, "history.migrate.", 16) || !strncmp(op, "history.collections.", 20))
        return history_collections_command(path, file, request);
    if (!strcmp(op, "history.list")) {
        struct json_object *r = reply(true, "read"), *rows = json_object_new_array();
        const char *kind = text(request, "kind"), *query = text(request, "query");
        struct json_object *v = NULL;
        int64_t offset = 0, limit = 100;
        bool archived = false;
        if (json_object_object_get_ex(request, "offset", &v)) {
            if (!json_object_is_type(v, json_type_int))
                return json_object_put(r), json_object_put(rows), reply(false, "Invalid offset");
            offset = json_object_get_int64(v);
        }
        if (json_object_object_get_ex(request, "limit", &v)) {
            if (!json_object_is_type(v, json_type_int))
                return json_object_put(r), json_object_put(rows), reply(false, "Invalid limit");
            limit = json_object_get_int64(v);
        }
        if (json_object_object_get_ex(request, "include_archived", &v)) {
            if (!json_object_is_type(v, json_type_boolean))
                return json_object_put(r), json_object_put(rows), reply(false, "Invalid archive filter");
            archived = json_object_get_boolean(v);
        }
        if (offset < 0 || offset > 4096 || limit < 1 || limit > 100) {
            json_object_put(r);
            json_object_put(rows);
            return reply(false, "History pagination limit refused");
        }
        int64_t total = 0;
        for (size_t i = 0; records && i < json_object_array_length(records); i++) {
            struct json_object *item = json_object_array_get_idx(records, i), *a = NULL;
            json_object_object_get_ex(item, "archived", &a);
            if ((!archived && json_object_get_boolean(a)) || (*kind && strcmp(kind, text(item, "kind"))) ||
                (*query && !strcasestr(text(item, "name"), query)) ||
                (*text(request,"progress") && strcmp(text(request,"progress"),history_progress(file->source_json,item))))
                continue;
            if (total >= offset && total < offset + limit)
                json_object_array_add(rows, clone(item));
            total++;
        }
        json_object_object_add(r, "records", rows);
        json_object_object_add(r, "total", json_object_new_int64(total));
        json_object_object_add(r, "next_offset",
                               total > offset + limit ? json_object_new_int64(offset + limit) : NULL);
        return r;
    }
    struct json_object *existing = find(records, text(request, "record_id"));
    if (!strcmp(op, "history.get") || !strcmp(op, "history.export") || !strcmp(op, "history.point")) {
        if (!existing)
            return reply(false, "Unknown history identity");
        struct json_object *r = reply(true, "read");
        json_object_object_add(r, "record", clone(existing));
        if (!strcmp(op, "history.get"))
            return r;
        HistoryTrack t = {0};
        char err[256] = {0};
        if (!history_asset_load(path, text(existing, "asset_hash"), &t, err, sizeof(err))) {
            json_object_put(r);
            return reply(false, err);
        }
        bool ok = true;
        if (!strcmp(op, "history.export"))
            ok = export_asset(text(request, "destination"), &t);
        else {
            struct json_object *index = NULL;
            int64_t i = -1;
            if (json_object_object_get_ex(request, "index", &index) &&
                json_object_is_type(index, json_type_int))
                i = json_object_get_int64(index);
            if (i < 0 || (uint64_t)i >= t.point_count)
                ok = false;
            else {
                HistoryPoint p = t.points[i];
                struct json_object *point = json_object_new_object();
                json_object_object_add(point, "lat", json_object_new_double(p.lat));
                json_object_object_add(point, "lon", json_object_new_double(p.lon));
                json_object_object_add(point, "segment", json_object_new_int(p.segment));
                json_object_object_add(point, "distance_m", json_object_new_double(p.distance_m));
                json_object_object_add(point, "elevation_m",
                                       isfinite(p.elevation) ? json_object_new_double(p.elevation) : NULL);
                json_object_object_add(point, "utc_seconds",
                                       isfinite(p.time_s) ? json_object_new_double(p.time_s) : NULL);
                json_object_object_add(r, "point", point);
            }
        }
        history_track_free(&t);
        if (!ok) {
            json_object_put(r);
            return reply(false, "Point index or export destination refused");
        }
        return r;
    }
    if (!strcmp(op, "history.nearby")) {
        if (!existing)
            return reply(false, "Select a track identity");
        struct json_object *radius = NULL;
        double meters = 200;
        if (json_object_object_get_ex(request, "radius_m", &radius)) {
            if (!(json_object_is_type(radius, json_type_int) ||
                  json_object_is_type(radius, json_type_double)))
                return reply(false, "Numeric peak radius required");
            meters = json_object_get_double(radius);
        }
        if (!isfinite(meters) || meters < 10 || meters > 2000)
            return reply(false, "Peak radius must be 10 to 2000 meters");
        HistoryTrack t = {0};
        char err[256] = {0};
        if (!history_asset_load(path, text(existing, "asset_hash"), &t, err, sizeof(err)))
            return reply(false, err);
        struct json_object *r = reply(true, "suggestions_only"), *candidates = json_object_new_array();
        size_t peaks = 0;
        for (size_t i = 0; records && i < json_object_array_length(records); i++) {
            struct json_object *peak = json_object_array_get_idx(records, i), *la = NULL, *lo = NULL,
                               *archived = NULL;
            json_object_object_get_ex(peak, "archived", &archived);
            if (strcmp(text(peak, "kind"), "peak") || json_object_get_boolean(archived))
                continue;
            if (++peaks > 128) {
                json_object_put(r);
                json_object_put(candidates);
                history_track_free(&t);
                return reply(false, "Select a catalog with at most 128 active peaks for proximity review");
            }
            if (!json_object_object_get_ex(peak, "lat", &la) || !json_object_object_get_ex(peak, "lon", &lo))
                continue;
            double nearest = INFINITY;
            size_t index = 0;
            for (size_t j = 0; j < t.point_count; j++) {
                double d =
                    ground_distance_meters((LatLon){json_object_get_double(la), json_object_get_double(lo)},
                                           (LatLon){t.points[j].lat, t.points[j].lon});
                if (d < nearest) {
                    nearest = d;
                    index = j;
                }
            }
            if (nearest <= meters) {
                struct json_object *v = json_object_new_object();
                json_object_object_add(v, "peak_id", json_object_new_string(text(peak, "id")));
                json_object_object_add(v, "name", json_object_new_string(text(peak, "name")));
                json_object_object_add(v, "distance_m", json_object_new_double(nearest));
                json_object_object_add(v, "point_index", json_object_new_int64(index));
                json_object_array_add(candidates, v);
            }
        }
        history_track_free(&t);
        json_object_object_add(r, "candidates", candidates);
        json_object_object_add(r, "confirmation_required", json_object_new_boolean(true));
        return r;
    }
    bool preview = !strcmp(op, "history.preview");
    if (!preview && strcmp(op, "history.commit"))
        return reply(false, "Unknown history operation");
    if (file->write_blocked || !map_forge_places_is_database(path))
        return reply(false, "History requires a writable personal SQLite repository");
    const char *rid = text(request, "request_id");
    if (!*rid || !valid_text(request, "request_id", 128))
        return reply(false, "Bounded request_id required");
    char fingerprint[32];
    token(json_object_to_json_string_ext(request, JSON_C_TO_STRING_PLAIN), fingerprint);
    struct json_object *receipts = array(file->source_json, "hiking_receipts"), *prior = find(receipts, rid),
                       *saved = NULL;
    if (!preview && prior) {
        if (strcmp(text(prior, "fingerprint"), fingerprint))
            return reply(false, "History request_id reused with different input");
        json_object_object_get_ex(prior, "reply", &saved);
        return clone(saved);
    }
    const char *keys[] = {"name",  "kind",     "source_url", "external_id",  "visited_on",
                          "notes", "evidence", "peak_id",    "recording_id", "trail_id",
                          "state", "progress", "status_evidence", "action", "source_path"};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (!valid_text(request, keys[i], !strcmp(keys[i], "notes") ? 2048 : 1024))
            return reply(false, "History field type/length refused");
    const char *kind = text(request, "kind"), *action = text(request, "action");
    const char *state = text(request, "state");
    if (*state && strcmp(state, "unvisited") && strcmp(state, "wishlist") && strcmp(state, "visited") &&
        strcmp(state, "summited"))
        return reply(false, "Unknown peak/visit state");
    if (*action && strcmp(action, "archive") && strcmp(action, "restore") && strcmp(action,"status") && strcmp(action,"status_restore"))
        return reply(false, "Unknown history action");
    if (!*action && strcmp(kind, "recorded") && strcmp(kind, "recommended") && strcmp(kind, "manual") &&
        strcmp(kind, "peak") && strcmp(kind, "visit"))
        return reply(false, "Explicit history kind required");
    if (existing && !*action && strcmp(kind, text(existing, "kind")))
        return reply(false,
                     "Keep the history kind stable; add a separate linked record for another source type");
    if (*text(request,"progress") && !history_progress_valid(text(request,"progress")))
        return reply(false,"Unknown personal progress");
    if (!strcmp(action,"status") && (!existing || !*text(request,"progress")))
        return reply(false,"Status change requires record identity and personal progress");
    struct json_object *snapshot=NULL;
    if(!strcmp(action,"status_restore")) {
        if(!existing || !json_object_object_get_ex(request,"status_snapshot",&snapshot) || !json_object_is_type(snapshot,json_type_object))return reply(false,"Status undo requires a bounded metadata snapshot");
        const char *keys[]={"progress","state","status_origin","status_evidence"};
        for(unsigned i=0;i<4;i++)if(!valid_text(snapshot,keys[i],1024))return reply(false,"Invalid status undo field");
        if(*text(snapshot,"progress")&&!history_progress_valid(text(snapshot,"progress")))return reply(false,"Invalid prior personal progress");
        struct json_object *tracked=NULL;
        if(json_object_object_get_ex(snapshot,"tracked",&tracked)&&!json_object_is_type(tracked,json_type_boolean))return reply(false,"Invalid prior tracking state");
    }
    HistoryTrack t = {0};
    char error[256] = {0};
    bool asset = *text(request, "source_path") != 0;
    if (asset && strcmp(kind, "recorded") && strcmp(kind, "recommended"))
        return reply(false, "Only recorded/recommended records attach GPX geometry");
    if (!*action && (!strcmp(kind, "recorded") || !strcmp(kind, "recommended")) && !asset && !existing &&
        (strcmp(kind,"recommended") || !*text(request,"external_id") || !*text(request,"source_url")))
        return reply(false, "Track source or identifiable reference metadata required");
    if (asset && !history_read(text(request, "source_path"), &t, error, sizeof(error)))
        return reply(false, error);
    if (!*text(request, "name") && !t.name[0] && !*action) {
        history_track_free(&t);
        return reply(false, "History name required");
    }
    struct json_object *record = existing ? clone(existing) : json_object_new_object();
    const char *id = existing ? text(existing, "id") : NULL;
    char new_id[64], meta_token[32];
    token(rid, meta_token);
    snprintf(new_id, sizeof(new_id), "hike-%s", meta_token);
    if (!id)
        id = new_id;
    if (!existing && *text(request, "record_id")) {
        json_object_put(record);
        history_track_free(&t);
        return reply(false, "Unknown record_id");
    }
    struct json_object *rev = NULL, *expected = NULL;
    int64_t revision = 0;
    if (existing) {
        json_object_object_get_ex(existing, "revision", &rev);
        revision = json_object_get_int64(rev);
        if (!json_object_object_get_ex(request, "expected_revision", &expected) ||
            !json_object_is_type(expected, json_type_int) || json_object_get_int64(expected) != revision) {
            json_object_put(record);
            history_track_free(&t);
            return reply(false, "History revision is stale or missing");
        }
    }
    if (!*action) {
        for (size_t i = 0; records && i < json_object_array_length(records); i++) {
            struct json_object *old = json_object_array_get_idx(records, i);
            if (old == existing)
                continue;
            bool same_external = *text(request, "external_id") &&
                                 !strcmp(text(request, "external_id"), text(old, "external_id")) &&
                                 !strcmp(kind, text(old, "kind"));
            bool same_source = asset && !strcmp(kind, "recorded") && !strcmp(kind, text(old, "kind")) &&
                               !strcmp(t.hash, text(old, "asset_hash"));
            if (same_external && asset && strcmp(t.hash, text(old, "asset_hash"))) {
                json_object_put(record);
                history_track_free(&t);
                return reply(false, "Changed source for existing activity/trail; update its record_id with "
                                    "expected_revision");
            }
            if (same_external || same_source) {
                json_object_put(record);
                history_track_free(&t);
                struct json_object *r = reply(true, "duplicate_or_existing_identity");
                json_object_object_add(r, "record", clone(old));
                json_object_object_add(r, "duplicate", json_object_new_boolean(true));
                return r;
            }
        }
        json_object_object_add(record, "id", json_object_new_string(id));
        json_object_object_add(record, "kind", json_object_new_string(kind));
        json_object_object_add(
            record, "name", json_object_new_string(*text(request, "name") ? text(request, "name") : t.name));
        for (unsigned i = 2; i < sizeof(keys) / sizeof(keys[0]) - 2; i++) {
            struct json_object *v = NULL;
            if (json_object_object_get_ex(request, keys[i], &v))
                json_object_object_add(record, keys[i], json_object_get(v));
        }
        if (!strcmp(kind, "peak")) {
            struct json_object *lat = NULL, *lon = NULL;
            if (!json_object_object_get_ex(request, "lat", &lat) ||
                !json_object_object_get_ex(request, "lon", &lon) ||
                !(json_object_is_type(lat, json_type_double) || json_object_is_type(lat, json_type_int)) ||
                !(json_object_is_type(lon, json_type_double) || json_object_is_type(lon, json_type_int)) ||
                !isfinite(json_object_get_double(lat)) || !isfinite(json_object_get_double(lon)) ||
                fabs(json_object_get_double(lat)) > 85.05112878 || fabs(json_object_get_double(lon)) > 180) {
                json_object_put(record);
                history_track_free(&t);
                return reply(false, "Peak coordinates required");
            }
            json_object_object_add(record, "lat", json_object_get(lat));
            json_object_object_add(record, "lon", json_object_get(lon));
        }
        if (*text(request, "peak_id") &&
            (!find(records, text(request, "peak_id")) ||
             strcmp(text(find(records, text(request, "peak_id")), "kind"), "peak"))) {
            json_object_put(record);
            history_track_free(&t);
            return reply(false, "Unknown peak link");
        }
        if (*text(request, "recording_id") &&
            (!find(records, text(request, "recording_id")) ||
             strcmp(text(find(records, text(request, "recording_id")), "kind"), "recorded"))) {
            json_object_put(record);
            history_track_free(&t);
            return reply(false, "Unknown recording link");
        }
        if (*text(request, "trail_id") &&
            (!find(records, text(request, "trail_id")) ||
             strcmp(text(find(records, text(request, "trail_id")), "kind"), "recommended"))) {
            json_object_put(record);
            history_track_free(&t);
            return reply(false, "Unknown reference trail link");
        }
        if (asset) {
            json_object_object_add(record, "asset_hash", json_object_new_string(t.hash));
            json_object_object_add(record, "metrics", metrics(&t));
            json_object_object_add(record, "source_name", json_object_new_string(t.name));
        }
    }
    if (*text(request,"progress")) {
        json_object_object_add(record,"progress",json_object_new_string(text(request,"progress")));
        json_object_object_add(record,"status_origin",json_object_new_string("user"));
        json_object_object_add(record,"status_evidence",json_object_new_string(*text(request,"status_evidence")?text(request,"status_evidence"):"Explicit user status change"));
        if (!strcmp(text(record,"kind"),"peak")) {
            const char *progress=text(request,"progress");
            json_object_object_add(record,"tracked",json_object_new_boolean(strcmp(progress,"not_done") && strcmp(progress,"unreviewed")));
            json_object_object_add(record,"state",json_object_new_string(!strcmp(progress,"completed")?"summited":!strcmp(progress,"partial")?"visited":!strcmp(progress,"planned")?"wishlist":"unvisited"));
        }
    }
    if(snapshot) {
        const char *keys[]={"progress","state","tracked","status_origin","status_evidence"};
        for(unsigned i=0;i<5;i++){struct json_object *value=NULL;if(json_object_object_get_ex(snapshot,keys[i],&value))json_object_object_add(record,keys[i],json_object_get(value));else json_object_object_del(record,keys[i]);}
    }
    if (*action && !existing) {
        json_object_put(record);
        history_track_free(&t);
        return reply(false, "Archive/restore requires record identity");
    }
    struct json_object *a = NULL;
    if (*action && strcmp(action,"status") && strcmp(action,"status_restore"))
        json_object_object_add(record, "archived", json_object_new_boolean(!strcmp(action, "archive")));
    else if (!json_object_object_get_ex(record, "archived", &a))
        json_object_object_add(record, "archived", json_object_new_boolean(false));
    json_object_object_add(record, "revision", json_object_new_int64(revision + 1));
    struct json_object *candidate = clone(request);
    json_object_object_del(candidate, "operation");
    json_object_object_del(candidate, "candidate_token");
    json_object_object_del(candidate, "expected_store_token");
    if (asset)
        json_object_object_add(candidate, "source_sha256", json_object_new_string(t.hash));
    char ct[32], st[32];
    token(json_object_to_json_string_ext(candidate, JSON_C_TO_STRING_PLAIN), ct);
    json_object_put(candidate);
    store_token(file, st);
    if (!existing && !preview) {
        char now[64];
        stamp(now);
        json_object_object_add(record, "imported_at", json_object_new_string(now));
    }
    struct json_object *r = reply(true, preview ? "preview" : "saved");
    json_object_object_add(r, "record", clone(record));
    json_object_object_add(r, "candidate_token", json_object_new_string(ct));
    json_object_object_add(r, "store_token", json_object_new_string(st));
    if (preview) {
        json_object_put(record);
        history_track_free(&t);
        return r;
    }
    if (strcmp(ct, text(request, "candidate_token")) || strcmp(st, text(request, "expected_store_token"))) {
        json_object_put(record);
        json_object_put(r);
        history_track_free(&t);
        return reply(false, "History preview changed or is stale");
    }
    struct json_object *root = clone(file->source_json), *next = array(root, "hiking");
    if (!next) {
        next = json_object_new_array();
        json_object_object_add(root, "hiking", next);
    }
    if ((!existing && json_object_array_length(next) >= 4096) ||
        !json_object_is_type(next, json_type_array)) {
        json_object_put(root);
        json_object_put(record);
        json_object_put(r);
        history_track_free(&t);
        return reply(false, "History capacity reached");
    }
    if (existing) {
        for (size_t i = 0; i < json_object_array_length(next); i++)
            if (!strcmp(text(json_object_array_get_idx(next, i), "id"), id)) {
                json_object_array_put_idx(next, i, record);
                record = NULL;
                break;
            }
    } else {
        json_object_array_add(next, record);
        record = NULL;
    }
    json_object_object_add(root, "hiking_version", json_object_new_int(1));
    struct json_object *nr = array(root, "hiking_receipts");
    if (!nr) {
        nr = json_object_new_array();
        json_object_object_add(root, "hiking_receipts", nr);
    }
    if (json_object_array_length(nr) >= 8192) {
        json_object_put(root);
        json_object_put(r);
        history_track_free(&t);
        return reply(false, "History receipt capacity reached");
    }
    struct json_object *receipt = json_object_new_object();
    json_object_object_add(receipt, "id", json_object_new_string(rid));
    json_object_object_add(receipt, "fingerprint", json_object_new_string(fingerprint));
    json_object_object_add(receipt, "reply", clone(r));
    json_object_array_add(nr, receipt);
    bool ok = map_forge_places_save_asset(path, root, file->source_signature, asset ? &t : NULL, NULL, error,
                                          sizeof(error));
    json_object_put(root);
    history_track_free(&t);
    if (ok)
        ok = map_forge_places_load(path, file, error, sizeof(error));
    if (!ok) {
        json_object_put(r);
        return reply(false, error);
    }
    return r;
}
