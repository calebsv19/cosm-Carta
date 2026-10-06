#include "app/place_repository.h"
#include "app/history.h"
#include "core_io.h"
#include <fcntl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

// Reports storage failure while leaving the existing document intact.
static bool repo_error(char *error, size_t capacity, const char *message) {
    if (error && capacity)
        snprintf(error, capacity, "%s", message);
    return false;
}
// Computes the same exact-byte signature used by legacy pin files.
static uint64_t repo_signature(const char *text) {
    uint64_t value = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        value ^= *p;
        value *= UINT64_C(1099511628211);
    }
    return value;
}
// Reads an optional text field without interpreting other JSON types.
static const char *repo_text(struct json_object *object, const char *key) {
    struct json_object *v = NULL;
    return json_object_object_get_ex(object, key, &v) && json_object_is_type(v, json_type_string)
               ? json_object_get_string(v)
               : "";
}
// Reads a scalar pragma without creating or altering tables.
static int repo_pragma(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt = NULL;
    int value = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
        value = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}
// Requires Carta's schema rather than opening another application's SQLite file.
static bool repo_schema(sqlite3 *db) {
    return repo_pragma(db, "PRAGMA application_id") == 1296453714 &&
           repo_pragma(db, "PRAGMA user_version") == 1;
}
// Recognizes the app-owned SQLite workspace extension.
bool map_forge_places_is_database(const char *path) {
    size_t n = path ? strlen(path) : 0u;
    return n > 7u && strcmp(path + n - 7u, ".sqlite") == 0;
}
// Acquires one persistent file lock shared by native and command owners.
int map_forge_places_lock(const char *path) {
    char directory[MAPFORGE_PIN_PATH_CAPACITY], lock[MAPFORGE_PIN_PATH_CAPACITY + 8u];
    if (!path || strlen(path) >= sizeof(directory))
        return -1;
    snprintf(directory, sizeof(directory), "%s", path);
    char *slash = strrchr(directory, '/');
    if (slash) {
        *slash = 0;
        for (char *p = directory + 1; *p; ++p)
            if (*p == '/') {
                *p = 0;
                if (mkdir(directory, 0700) != 0 && access(directory, F_OK) != 0)
                    return -1;
                *p = '/';
            }
        if (mkdir(directory, 0700) != 0 && access(directory, F_OK) != 0)
            return -1;
    }
    snprintf(lock, sizeof(lock), "%s.lock", path);
    int fd = open(lock, O_CREAT | O_RDWR, 0600);
    if (fd < 0)
        return -1;
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}
// Loads exactly one authoritative document; relational indexes are transactionally derived.
bool map_forge_places_load(const char *path, MapForgePinsFile *out, char *error, size_t capacity) {
    if (error && capacity)
        error[0] = 0;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto done;
    if (!repo_schema(db)) {
        repo_error(error, capacity, "Unsupported personal repository identity/schema; preserve it.");
        goto done;
    }
    if (sqlite3_prepare_v2(db, "SELECT payload FROM document WHERE singleton=1", -1, &stmt, NULL) !=
            SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW)
        goto done;
    const char *text = (const char *)sqlite3_column_text(stmt, 0);
    int length = sqlite3_column_bytes(stmt, 0);
    ok = text && length > 0 && map_forge_pins_decode_json(text, (size_t)length, out, error, capacity);
done:
    if (!ok && error && capacity && !error[0])
        repo_error(error, capacity, db ? sqlite3_errmsg(db) : "Cannot open personal repository");
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}
// Executes a prepared insertion with only bound application data.
static bool repo_insert(sqlite3 *db, const char *sql, const char **values, size_t count) {
    sqlite3_stmt *stmt = NULL;
    bool ok = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK;
    for (size_t i = 0u; ok && i < count; ++i)
        ok = (values[i] ? sqlite3_bind_text(stmt, (int)i + 1, values[i], -1, SQLITE_TRANSIENT)
                        : sqlite3_bind_null(stmt, (int)i + 1)) == SQLITE_OK;
    if (ok)
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}
// Builds relational place/pet/service indexes with foreign-key integrity.
static bool repo_index(sqlite3 *db, struct json_object *root) {
    struct json_object *array = NULL;
    if (sqlite3_exec(db,
                     "DELETE FROM services; DELETE FROM pet_places; DELETE FROM pets; DELETE FROM places;",
                     NULL, NULL, NULL) != SQLITE_OK)
        return false;
    json_object_object_get_ex(root, "pins", &array);
    for (size_t i = 0u; i < json_object_array_length(array); ++i) {
        struct json_object *v = json_object_array_get_idx(array, i);
        const char *args[] = {repo_text(v, "id"), repo_text(v, "name"), repo_text(v, "address"),
                              json_object_to_json_string_ext(v, JSON_C_TO_STRING_PLAIN)};
        if (!args[0][0] || !repo_insert(db, "INSERT INTO places VALUES(?1,?2,?3,?4)", args, 4u))
            return false;
    }
    array = NULL;
    json_object_object_get_ex(root, "pets", &array);
    if (array && !json_object_is_type(array, json_type_array))
        return false;
    for (size_t i = 0u; array && i < json_object_array_length(array); ++i) {
        struct json_object *v = json_object_array_get_idx(array, i);
        const char *args[] = {repo_text(v, "id"), repo_text(v, "name"),
                              json_object_to_json_string_ext(v, JSON_C_TO_STRING_PLAIN)};
        if (!args[0][0] || !args[1][0] || !repo_insert(db, "INSERT INTO pets VALUES(?1,?2,?3)", args, 3u))
            return false;
    }
    array = NULL;
    json_object_object_get_ex(root, "pet_places", &array);
    if (array && !json_object_is_type(array, json_type_array))
        return false;
    for (size_t i = 0u; array && i < json_object_array_length(array); ++i) {
        struct json_object *v = json_object_array_get_idx(array, i);
        const char *args[] = {repo_text(v, "pet_id"), repo_text(v, "place_id"), repo_text(v, "role")};
        if (!args[0][0] || !args[1][0] ||
            !repo_insert(db, "INSERT INTO pet_places VALUES(?1,?2,?3)", args, 3u))
            return false;
    }
    array = NULL;
    json_object_object_get_ex(root, "services", &array);
    if (array && !json_object_is_type(array, json_type_array))
        return false;
    for (size_t i = 0u; array && i < json_object_array_length(array); ++i) {
        struct json_object *v = json_object_array_get_idx(array, i);
        const char *pet = repo_text(v, "pet_id");
        const char *args[] = {repo_text(v, "id"), repo_text(v, "place_id"), pet[0] ? pet : NULL,
                              repo_text(v, "kind"),
                              json_object_to_json_string_ext(v, JSON_C_TO_STRING_PLAIN)};
        if (!args[0][0] || !args[1][0] || !args[3][0] ||
            !repo_insert(db, "INSERT INTO services VALUES(?1,?2,?3,?4,?5)", args, 5u))
            return false;
    }
    return true;
}
// Saves canonical JSON and relational indexes in one compare-and-swap transaction.
bool map_forge_places_save_asset(const char *path, struct json_object *root, uint64_t expected,
                                 const HistoryTrack *asset, const char *restore_source, char *error,
                                 size_t capacity) {
    if (error && capacity)
        error[0] = 0;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    bool ok = false, transaction = false;
    bool existed = access(path, F_OK) == 0;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
        goto done;
    sqlite3_busy_timeout(db, 1500);
    if (existed && !repo_schema(db)) {
        repo_error(error, capacity, "Unsupported personal repository schema; refusing replacement.");
        goto done;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL; BEGIN IMMEDIATE", NULL, NULL,
                     NULL) != SQLITE_OK)
        goto done;
    transaction = true;
    if (!existed &&
        sqlite3_exec(
            db,
            "PRAGMA application_id=1296453714; PRAGMA user_version=1;"
            "CREATE TABLE document(singleton INTEGER PRIMARY KEY CHECK(singleton=1),payload TEXT NOT NULL);"
            "CREATE TABLE places(id TEXT PRIMARY KEY,name TEXT NOT NULL,address TEXT NOT NULL,payload TEXT "
            "NOT NULL);"
            "CREATE INDEX places_address ON places(address);"
            "CREATE TABLE pets(id TEXT PRIMARY KEY,name TEXT NOT NULL,payload TEXT NOT NULL);"
            "CREATE TABLE pet_places(pet_id TEXT REFERENCES pets(id),place_id TEXT REFERENCES "
            "places(id),role TEXT NOT NULL,PRIMARY KEY(pet_id,place_id,role));"
            "CREATE TABLE services(id TEXT PRIMARY KEY,place_id TEXT NOT NULL REFERENCES places(id),pet_id "
            "TEXT REFERENCES pets(id),kind TEXT NOT NULL,payload TEXT NOT NULL);",
            NULL, NULL, NULL) != SQLITE_OK)
        goto done;
    if (existed) {
        if (sqlite3_prepare_v2(db, "SELECT payload FROM document WHERE singleton=1", -1, &stmt, NULL) !=
                SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_ROW)
            goto done;
        const char *old = (const char *)sqlite3_column_text(stmt, 0);
        if (!old || !expected || expected != repo_signature(old)) {
            repo_error(error, capacity, "Personal records changed; reload before saving.");
            goto done;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if (sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS hiking_assets(hash TEXT PRIMARY KEY "
                     "CHECK(length(hash)=64),original BLOB NOT NULL CHECK(length(original)<=16777216));",
                     NULL, NULL, NULL) != SQLITE_OK)
        goto done;
    if (restore_source) {
        if (sqlite3_prepare_v2(db, "ATTACH DATABASE ?1 AS history_restore", -1, &stmt, NULL) != SQLITE_OK)
            goto done;
        sqlite3_bind_text(stmt, 1, restore_source, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE)
            goto done;
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (sqlite3_prepare_v2(
                db, "SELECT count(*) FROM history_restore.sqlite_master WHERE name='hiking_assets'", -1,
                &stmt, NULL) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_ROW)
            goto done;
        bool has_assets = sqlite3_column_int(stmt, 0) > 0;
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (has_assets &&
            sqlite3_exec(
                db,
                "INSERT OR IGNORE INTO hiking_assets SELECT hash,original FROM history_restore.hiking_assets",
                NULL, NULL, NULL) != SQLITE_OK)
            goto done;
    }
    if (asset) {
        if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO hiking_assets VALUES(?1,?2)", -1, &stmt, NULL) !=
            SQLITE_OK)
            goto done;
        sqlite3_bind_text(stmt, 1, asset->hash, -1, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 2, asset->bytes, (int)asset->byte_count, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE)
            goto done;
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if (sqlite3_prepare_v2(db, "SELECT coalesce(sum(length(original)),0) FROM hiking_assets", -1, &stmt,
                           NULL) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW || sqlite3_column_int64(stmt, 0) > 128 * 1024 * 1024) {
        repo_error(error, capacity, "History original storage limit reached (128 MiB)");
        goto done;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    struct json_object *hiking = NULL;
    json_object_object_get_ex(root, "hiking", &hiking);
    if (hiking && !json_object_is_type(hiking, json_type_array)) {
        repo_error(error, capacity, "Unsupported hiking catalog; preserve existing records");
        goto done;
    }
    for (size_t i = 0; hiking && i < json_object_array_length(hiking); ++i) {
        const char *hash = repo_text(json_object_array_get_idx(hiking, i), "asset_hash");
        if (!*hash)
            continue;
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM hiking_assets WHERE hash=?1", -1, &stmt, NULL) != SQLITE_OK)
            goto done;
        sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            repo_error(error, capacity, "Referenced hiking source missing; preserve existing records");
            goto done;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if (!repo_index(db, root))
        goto done;
    const char *args[] = {
        json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED)};
    if (!repo_insert(db, "INSERT OR REPLACE INTO document VALUES(1,?1)", args, 1u))
        goto done;
    if (sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK)
        goto done;
    transaction = false;
    ok = true;
    chmod(path, 0600);
done:
    if (!ok && error && capacity && !error[0])
        repo_error(error, capacity, db ? sqlite3_errmsg(db) : "Cannot open personal repository");
    sqlite3_finalize(stmt);
    if (transaction)
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
    sqlite3_close(db);
    if (!ok && !existed)
        unlink(path);
    return ok;
}

// Saves personal metadata while retaining immutable hiking assets.
bool map_forge_places_save_document(const char *path, struct json_object *root, uint64_t expected,
                                    char *error, size_t capacity) {
    return map_forge_places_save_asset(path, root, expected, NULL, NULL, error, capacity);
}

// Commits native pin edits with the same document guard, revision history and readback boundary.
bool map_forge_places_commit_pin(const char *path, MapForgePinsFile *file, const MapForgePin *pin,
                                 bool remove, char *error, size_t capacity) {
    if (!file || !file->source_json || !pin || file->write_blocked)
        return repo_error(error, capacity, "Personal records unavailable or read-only");
    MapForgePinsFile next;
    map_forge_pins_file_init(&next);
    const char *text =
        json_object_to_json_string_ext(file->source_json, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    if (!map_forge_pins_decode_json(text, strlen(text), &next, error, capacity))
        return false;
    next.source_signature = file->source_signature;
    struct json_object *records = NULL, *record = NULL, *history = NULL;
    json_object_object_get_ex(next.source_json, "pins", &records);
    for (size_t i = 0; i < json_object_array_length(records); ++i) {
        struct json_object *item = json_object_array_get_idx(records, i);
        if (!strcmp(repo_text(item, "id"), pin->id)) {
            record = item;
            break;
        }
    }
    if (remove && !record) {
        map_forge_pins_file_free(&next);
        return repo_error(error, capacity, "Unknown place identity");
    }
    if (record) {
        if (!json_object_object_get_ex(next.source_json, "place_history", &history)) {
            history = json_object_new_array();
            json_object_object_add(next.source_json, "place_history", history);
        }
        if (!json_object_is_type(history, json_type_array) || json_object_array_length(history) >= 2000u) {
            map_forge_pins_file_free(&next);
            return repo_error(error, capacity, "Place revision history capacity reached");
        }
        struct json_object *event = json_object_new_object();
        json_object_object_add(event, "place_id", json_object_new_string(pin->id));
        json_object_object_add(
            event, "previous",
            json_tokener_parse(json_object_to_json_string_ext(record, JSON_C_TO_STRING_PLAIN)));
        json_object_object_add(event, "source",
                               json_object_new_string(remove ? "native delete" : "native edit"));
        json_object_array_add(history, event);
    } else if (!remove) {
        record = json_object_new_object();
        json_object_object_add(record, "id", json_object_new_string(pin->id));
        json_object_array_add(records, record);
    }
    if (!remove && record) {
        struct json_object *revision = NULL;
        json_object_object_get_ex(record, "revision", &revision);
        json_object_object_add(record, "revision",
                               json_object_new_int64((revision ? json_object_get_int64(revision) : 0) + 1));
    }
    MapForgePin value = *pin;
    const MapForgePin *previous = map_forge_pins_find_by_id_const(file, pin->id);
    bool moved = !previous || previous->lat != pin->lat || previous->lon != pin->lon;
    if (!remove && moved) {
        snprintf(value.location_source, sizeof(value.location_source), "Manual Carta placement");
        snprintf(value.location_quality, sizeof(value.location_quality), "manual");
        struct json_object *location = json_object_new_object();
        json_object_object_add(location, "lat", json_object_new_double(value.lat));
        json_object_object_add(location, "lon", json_object_new_double(value.lon));
        json_object_object_add(location, "source", json_object_new_string(value.location_source));
        json_object_object_add(location, "quality", json_object_new_string(value.location_quality));
        json_object_object_add(record, "location_evidence", location);
    }
    bool ok = remove ? map_forge_pins_remove_by_id(&next, pin->id)
                     : map_forge_pins_upsert(&next, &value, error, capacity);
    if (ok)
        ok = map_forge_pins_save(path, &next, error, capacity);
    if (ok)
        ok = map_forge_pins_load(path, &next, error, capacity);
    if (ok) {
        map_forge_pins_file_free(file);
        *file = next;
        memset(&next, 0, sizeof(next));
    }
    map_forge_pins_file_free(&next);
    return ok;
}
