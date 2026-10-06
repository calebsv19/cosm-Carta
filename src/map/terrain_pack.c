#include "map/terrain_pack.h"
#include <json-c/json.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
// Gets an uncoerced field from manifest metadata.
static struct json_object *field(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, key, &v);
    return v;
}
// Accepts only a positive, bounded integer dimension.
static int dimension(struct json_object *o, const char *key) {
    struct json_object *v = field(o, key);
    if (!json_object_is_type(v, json_type_int))
        return 0;
    int64_t n = json_object_get_int64(v);
    return n > 0 && n <= 8192 ? (int)n : 0;
}
// Reads one scalar pragma using the existing read-only connection.
static int scalar(sqlite3 *db, const char *sql) {
    sqlite3_stmt *q = NULL;
    int n = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &q, NULL) == SQLITE_OK && sqlite3_step(q) == SQLITE_ROW)
        n = sqlite3_column_int(q, 0);
    sqlite3_finalize(q);
    return n;
}
void terrain_pack_close(TerrainPack *p) {
    if (!p)
        return;
    sqlite3_finalize(p->tile);
    sqlite3_close(p->db);
    memset(p, 0, sizeof(*p));
}
bool terrain_pack_open(TerrainPack *p, const char *path, char *error, size_t size) {
    TerrainPack next = {0};
    sqlite3_stmt *q = NULL;
    struct json_object *m = NULL;
    struct stat st;
    bool ok = false;
    int expected = 0;
    if (!p || !path)
        return false;
    if (stat(path, &st) || !S_ISREG(st.st_mode) || st.st_size < 4096 || st.st_size > 256LL * 1024 * 1024)
        goto done;
    if (sqlite3_open_v2(path, &next.db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK)
        goto done;
    sqlite3_limit(next.db, SQLITE_LIMIT_LENGTH, 512 * 1024);
    sqlite3_exec(next.db, "pragma query_only=ON; pragma cache_size=-2048; pragma mmap_size=0;", NULL, NULL,
                 NULL);
    if (scalar(next.db, "pragma application_id") != 0x4d465450 || scalar(next.db, "pragma user_version") != 1)
        goto done;
    if (sqlite3_prepare_v2(next.db, "select value from metadata where key='manifest'", -1, &q, NULL) !=
            SQLITE_OK ||
        sqlite3_step(q) != SQLITE_ROW)
        goto done;
    int n = sqlite3_column_bytes(q, 0);
    if (n <= 0 || n > 128 * 1024)
        goto done;
    struct json_tokener *tok = json_tokener_new();
    if (!tok)
        goto done;
    m = json_tokener_parse_ex(tok, (const char *)sqlite3_column_text(q, 0), n);
    bool parsed = json_tokener_get_error(tok) == json_tokener_success;
    json_tokener_free(tok);
    if (!parsed || !json_object_is_type(m, json_type_object))
        goto done;
    if (dimension(m, "schema") != 1 || dimension(m, "tile_size") != 256 || dimension(m, "gutter") != 1 ||
        strcmp(json_object_get_string(field(m, "crs")) ? json_object_get_string(field(m, "crs")) : "",
               "EPSG:3857"))
        goto done;
    struct json_object *styles = field(m, "styles");
    if (!json_object_is_type(styles, json_type_array) || json_object_array_length(styles) < 2 ||
        json_object_array_length(styles) > 10)
        goto done;
    next.style_count = (int)json_object_array_length(styles);
    for (int s = 0; s < next.style_count; s++) {
        struct json_object *style = json_object_array_get_idx(styles, s), *e = field(style, "extent"),
                           *levels = field(style, "levels");
        if (!json_object_is_type(e, json_type_array) || json_object_array_length(e) != 4 ||
            !json_object_is_type(levels, json_type_array) || json_object_array_length(levels) != 5)
            goto done;
        struct json_object *mode = field(style, "mode"), *palette = field(style, "palette"),
                           *area = field(style, "area");
        next.styles[s].mode = s < 2 ? s : -1;
        if (mode) {
            if (!json_object_is_type(mode, json_type_int) || json_object_get_int(mode) < 0 ||
                json_object_get_int(mode) > 1)
                goto done;
            next.styles[s].mode = json_object_get_int(mode);
        }
        if (next.styles[s].mode < 0 || (s < 2 && next.styles[s].mode != s))
            goto done;
        if (area && json_object_is_type(area, json_type_string))
            snprintf(next.styles[s].area, sizeof(next.styles[s].area), "%s", json_object_get_string(area));
        else
            snprintf(next.styles[s].area, sizeof(next.styles[s].area), "Overview");
        if (palette) {
            const char *encoding = json_object_get_string(field(style, "encoding"));
            if (!encoding || strcmp(encoding, "indexed8_zlib") ||
                !json_object_is_type(palette, json_type_array) || json_object_array_length(palette) != 256)
                goto done;
            next.styles[s].indexed = true;
            for (int i = 0; i < 256; i++) {
                struct json_object *color = json_object_array_get_idx(palette, i);
                if (!json_object_is_type(color, json_type_array) || json_object_array_length(color) != 4)
                    goto done;
                for (int c = 0; c < 4; c++) {
                    struct json_object *value = json_object_array_get_idx(color, c);
                    if (!json_object_is_type(value, json_type_int) || json_object_get_int64(value) < 0 ||
                        json_object_get_int64(value) > 255)
                        goto done;
                    next.styles[s].palette[i][c] = (unsigned char)json_object_get_int(value);
                }
            }
        }
        for (int i = 0; i < 4; i++) {
            struct json_object *v = json_object_array_get_idx(e, i);
            if (!json_object_is_type(v, json_type_double) && !json_object_is_type(v, json_type_int))
                goto done;
            double x = json_object_get_double(v);
            if (!isfinite(x) || fabs(x) > 20037509)
                goto done;
            next.styles[s].extent[i] = x;
        }
        if (next.styles[s].extent[0] >= next.styles[s].extent[2] ||
            next.styles[s].extent[1] >= next.styles[s].extent[3])
            goto done;
        for (int l = 0; l < 5; l++) {
            struct json_object *v = json_object_array_get_idx(levels, l);
            TerrainLevel *d = &next.styles[s].levels[l];
            d->width = dimension(v, "width");
            d->height = dimension(v, "height");
            d->cols = dimension(v, "cols");
            d->rows = dimension(v, "rows");
            if (!d->width || !d->height || d->cols != (d->width + 255) / 256 ||
                d->rows != (d->height + 255) / 256)
                goto done;
            if (l && (d->width != (next.styles[s].levels[0].width + (1 << l) - 1) / (1 << l) ||
                      d->height != (next.styles[s].levels[0].height + (1 << l) - 1) / (1 << l)))
                goto done;
            expected += d->cols * d->rows;
        }
    }
    if (scalar(next.db, "select count(*) from tiles") != expected)
        goto done;
    if (sqlite3_prepare_v2(next.db, "select crc,rgba from tiles where style=? and lod=? and col=? and row=?",
                           -1, &next.tile, NULL) != SQLITE_OK)
        goto done;
    next.bytes = (size_t)st.st_size;
    ok = true;
done:
    sqlite3_finalize(q);
    json_object_put(m);
    if (!ok) {
        terrain_pack_close(&next);
        if (error && size)
            snprintf(error, size, "Terrain unavailable: missing, corrupt or unsupported overview pack");
        return false;
    }
    terrain_pack_close(p);
    *p = next;
    if (error && size)
        *error = 0;
    return true;
}
bool terrain_pack_tile(TerrainPack *p, int style, int lod, int col, int row, unsigned char *rgba) {
    if (!p || !p->tile || !rgba || style < 0 || style >= p->style_count || lod < 0 || lod > 4)
        return false;
    TerrainLevel *d = &p->styles[style].levels[lod];
    if (col < 0 || row < 0 || col >= d->cols || row >= d->rows)
        return false;
    sqlite3_stmt *q = p->tile;
    sqlite3_reset(q);
    sqlite3_clear_bindings(q);
    sqlite3_bind_int(q, 1, style);
    sqlite3_bind_int(q, 2, lod);
    sqlite3_bind_int(q, 3, col);
    sqlite3_bind_int(q, 4, row);
    bool ok = false;
    if (sqlite3_step(q) == SQLITE_ROW) {
        int bytes = sqlite3_column_bytes(q, 1);
        if (bytes > 0 && bytes <= 300000) {
            unsigned char indices[TERRAIN_PIXELS * TERRAIN_PIXELS];
            bool indexed = p->styles[style].indexed;
            uLongf length = indexed ? sizeof(indices) : TERRAIN_RGBA_BYTES;
            ok = uncompress(indexed ? indices : rgba, &length, sqlite3_column_blob(q, 1), (uLong)bytes) ==
                     Z_OK &&
                 length == (indexed ? sizeof(indices) : TERRAIN_RGBA_BYTES);
            if (ok && indexed)
                for (size_t i = 0; i < sizeof(indices); i++)
                    memcpy(rgba + i * 4, p->styles[style].palette[indices[i]], 4);
            ok = ok && (uint32_t)crc32(0, rgba, TERRAIN_RGBA_BYTES) == (uint32_t)sqlite3_column_int64(q, 0);
        }
    }
    sqlite3_reset(q);
    return ok;
}

// Uses exact source footprints rather than guessing detail coverage from route identity.
int terrain_pack_select(const TerrainPack *p, int mode, double xmin, double ymin, double xmax, double ymax) {
    if (!p || mode < 0 || mode > 1)
        return -1;
    int best = mode;
    for (int i = 2; i < p->style_count; i++) {
        const TerrainStyle *s = &p->styles[i], *current = &p->styles[best];
        if (s->mode == mode && xmin >= s->extent[0] && ymin >= s->extent[1] && xmax <= s->extent[2] &&
            ymax <= s->extent[3] &&
            (s->extent[2] - s->extent[0]) / s->levels[0].width <
                (current->extent[2] - current->extent[0]) / current->levels[0].width)
            best = i;
    }
    return best;
}
