/*
 * core_data.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_data.h"

#include <string.h>

static char *core_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = (char *)core_alloc(n);
    if (!out) return NULL;
    memcpy(out, s, n);
    return out;
}

static CoreResult ensure_item_capacity(CoreDataset *dataset, size_t extra) {
    size_t needed = dataset->item_count + extra;
    if (needed <= dataset->item_capacity) return core_result_ok();

    size_t cap = dataset->item_capacity ? dataset->item_capacity * 2 : 8;
    while (cap < needed) cap *= 2;

    CoreDataItem *next = (CoreDataItem *)core_realloc(dataset->items, cap * sizeof(CoreDataItem));
    if (!next) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    dataset->items = next;
    dataset->item_capacity = cap;
    return core_result_ok();
}

static CoreResult ensure_meta_capacity(CoreDataset *dataset, size_t extra) {
    size_t needed = dataset->metadata_count + extra;
    if (needed <= dataset->metadata_capacity) return core_result_ok();

    size_t cap = dataset->metadata_capacity ? dataset->metadata_capacity * 2 : 8;
    while (cap < needed) cap *= 2;

    CoreMetadataItem *next = (CoreMetadataItem *)core_realloc(dataset->metadata, cap * sizeof(CoreMetadataItem));
    if (!next) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    dataset->metadata = next;
    dataset->metadata_capacity = cap;
    return core_result_ok();
}

static size_t column_type_size(CoreTableColumnType type) {
    switch (type) {
        case CORE_TABLE_COL_F32: return sizeof(float);
        case CORE_TABLE_COL_F64: return sizeof(double);
        case CORE_TABLE_COL_I64: return sizeof(int64_t);
        case CORE_TABLE_COL_U32: return sizeof(uint32_t);
        case CORE_TABLE_COL_BOOL: return sizeof(bool);
        default: return 0;
    }
}

static void free_item(CoreDataItem *item) {
    if (!item) return;
    core_free(item->name);
    if (item->kind == CORE_DATA_ARRAY_F32) {
        core_free(item->as.array_f32.values);
    } else if (item->kind == CORE_DATA_FIELD2D_F32) {
        core_free(item->as.field2d_f32.values);
    } else if (item->kind == CORE_DATA_FIELD2D_VEC2F32) {
        core_free(item->as.field2d_vec2f32.x);
        core_free(item->as.field2d_vec2f32.y);
    } else if (item->kind == CORE_DATA_TABLE_F32) {
        for (uint32_t c = 0; c < item->as.table_f32.column_count; ++c) {
            core_free(item->as.table_f32.columns[c].name);
            core_free(item->as.table_f32.columns[c].values);
        }
        core_free(item->as.table_f32.columns);
    } else if (item->kind == CORE_DATA_TABLE_TYPED) {
        for (uint32_t c = 0; c < item->as.table_typed.column_count; ++c) {
            core_free(item->as.table_typed.columns[c].name);
            core_free(item->as.table_typed.columns[c].as.raw);
        }
        core_free(item->as.table_typed.columns);
    }
    memset(item, 0, sizeof(*item));
}

void core_dataset_init(CoreDataset *dataset) {
    if (!dataset) return;
    memset(dataset, 0, sizeof(*dataset));
}

void core_dataset_free(CoreDataset *dataset) {
    if (!dataset) return;

    for (size_t i = 0; i < dataset->item_count; ++i) {
        free_item(&dataset->items[i]);
    }
    core_free(dataset->items);

    for (size_t i = 0; i < dataset->metadata_count; ++i) {
        core_free(dataset->metadata[i].key);
        if (dataset->metadata[i].type == CORE_META_STRING) {
            core_free(dataset->metadata[i].as.string_value);
        }
    }
    core_free(dataset->metadata);

    memset(dataset, 0, sizeof(*dataset));
}

static CoreResult add_metadata_item(CoreDataset *dataset, const char *key, CoreMetaType type, const void *value) {
    if (!dataset || !key || !value) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_meta_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    CoreMetadataItem *item = &dataset->metadata[dataset->metadata_count++];
    memset(item, 0, sizeof(*item));
    item->key = core_strdup(key);
    item->type = type;
    if (!item->key) {
        dataset->metadata_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    if (type == CORE_META_STRING) {
        item->as.string_value = core_strdup((const char *)value);
        if (!item->as.string_value) {
            core_free(item->key);
            dataset->metadata_count--;
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
    } else if (type == CORE_META_F64) {
        item->as.f64_value = *(const double *)value;
    } else if (type == CORE_META_I64) {
        item->as.i64_value = *(const int64_t *)value;
    } else if (type == CORE_META_BOOL) {
        item->as.bool_value = *(const bool *)value;
    }

    return core_result_ok();
}

CoreResult core_dataset_add_metadata(CoreDataset *dataset, const char *key, const char *value) {
    return core_dataset_add_metadata_string(dataset, key, value);
}

CoreResult core_dataset_add_metadata_string(CoreDataset *dataset, const char *key, const char *value) {
    if (!value) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    return add_metadata_item(dataset, key, CORE_META_STRING, value);
}

CoreResult core_dataset_add_metadata_f64(CoreDataset *dataset, const char *key, double value) {
    return add_metadata_item(dataset, key, CORE_META_F64, &value);
}

CoreResult core_dataset_add_metadata_i64(CoreDataset *dataset, const char *key, int64_t value) {
    return add_metadata_item(dataset, key, CORE_META_I64, &value);
}

CoreResult core_dataset_add_metadata_bool(CoreDataset *dataset, const char *key, bool value) {
    return add_metadata_item(dataset, key, CORE_META_BOOL, &value);
}

CoreResult core_dataset_add_scalar_f64(CoreDataset *dataset, const char *name, double value) {
    if (!dataset || !name) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_item_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    CoreDataItem *item = &dataset->items[dataset->item_count++];
    memset(item, 0, sizeof(*item));
    item->name = core_strdup(name);
    item->kind = CORE_DATA_SCALAR_F64;
    item->as.scalar_f64 = value;
    if (!item->name) {
        dataset->item_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }
    return core_result_ok();
}

CoreResult core_dataset_add_array_f32(CoreDataset *dataset, const char *name, const float *values, uint32_t count) {
    if (!dataset || !name || (!values && count > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_item_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    CoreDataItem *item = &dataset->items[dataset->item_count++];
    memset(item, 0, sizeof(*item));
    item->name = core_strdup(name);
    item->kind = CORE_DATA_ARRAY_F32;
    item->as.array_f32.count = count;

    if (count > 0) {
        item->as.array_f32.values = (float *)core_alloc(sizeof(float) * count);
        if (!item->as.array_f32.values || !item->name) {
            free_item(item);
            dataset->item_count--;
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        memcpy(item->as.array_f32.values, values, sizeof(float) * count);
    } else if (!item->name) {
        dataset->item_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    return core_result_ok();
}

CoreResult core_dataset_add_field2d_f32(CoreDataset *dataset, const char *name, CoreField2DDesc desc, const float *values) {
    if (!dataset || !name || !values || desc.width == 0 || desc.height == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_item_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    size_t count = (size_t)desc.width * (size_t)desc.height;
    CoreDataItem *item = &dataset->items[dataset->item_count++];
    memset(item, 0, sizeof(*item));
    item->name = core_strdup(name);
    item->kind = CORE_DATA_FIELD2D_F32;
    item->as.field2d_f32.desc = desc;
    item->as.field2d_f32.values = (float *)core_alloc(sizeof(float) * count);

    if (!item->name || !item->as.field2d_f32.values) {
        free_item(item);
        dataset->item_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    memcpy(item->as.field2d_f32.values, values, sizeof(float) * count);
    return core_result_ok();
}

CoreResult core_dataset_add_field2d_vec2f32(CoreDataset *dataset, const char *name, CoreField2DDesc desc, const float *x, const float *y) {
    if (!dataset || !name || !x || !y || desc.width == 0 || desc.height == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_item_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    size_t count = (size_t)desc.width * (size_t)desc.height;
    CoreDataItem *item = &dataset->items[dataset->item_count++];
    memset(item, 0, sizeof(*item));
    item->name = core_strdup(name);
    item->kind = CORE_DATA_FIELD2D_VEC2F32;
    item->as.field2d_vec2f32.desc = desc;
    item->as.field2d_vec2f32.x = (float *)core_alloc(sizeof(float) * count);
    item->as.field2d_vec2f32.y = (float *)core_alloc(sizeof(float) * count);

    if (!item->name || !item->as.field2d_vec2f32.x || !item->as.field2d_vec2f32.y) {
        free_item(item);
        dataset->item_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    memcpy(item->as.field2d_vec2f32.x, x, sizeof(float) * count);
    memcpy(item->as.field2d_vec2f32.y, y, sizeof(float) * count);
    return core_result_ok();
}

CoreResult core_dataset_add_table_f32(CoreDataset *dataset,
                                      const char *name,
                                      const char *const *column_names,
                                      uint32_t column_count,
                                      uint32_t row_count,
                                      const float *values_row_major) {
    if (!dataset || !name || !column_names || !values_row_major || column_count == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_item_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    CoreDataItem *item = &dataset->items[dataset->item_count++];
    memset(item, 0, sizeof(*item));
    item->name = core_strdup(name);
    item->kind = CORE_DATA_TABLE_F32;
    item->as.table_f32.column_count = column_count;
    item->as.table_f32.row_count = row_count;
    item->as.table_f32.columns = (CoreTableColumnF32 *)core_calloc(column_count, sizeof(CoreTableColumnF32));

    if (!item->name || !item->as.table_f32.columns) {
        free_item(item);
        dataset->item_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    for (uint32_t c = 0; c < column_count; ++c) {
        item->as.table_f32.columns[c].name = core_strdup(column_names[c]);
        item->as.table_f32.columns[c].values = (float *)core_alloc(sizeof(float) * row_count);
        if (!item->as.table_f32.columns[c].name || !item->as.table_f32.columns[c].values) {
            free_item(item);
            dataset->item_count--;
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }

        for (uint32_t row = 0; row < row_count; ++row) {
            item->as.table_f32.columns[c].values[row] = values_row_major[(size_t)row * column_count + c];
        }
    }

    return core_result_ok();
}

CoreResult core_dataset_add_table_typed(CoreDataset *dataset,
                                        const char *name,
                                        const char *const *column_names,
                                        const CoreTableColumnType *column_types,
                                        uint32_t column_count,
                                        uint32_t row_count,
                                        const void *const *column_data) {
    if (!dataset || !name || !column_names || !column_types || !column_data || column_count == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    CoreResult cap = ensure_item_capacity(dataset, 1);
    if (cap.code != CORE_OK) return cap;

    CoreDataItem *item = &dataset->items[dataset->item_count++];
    memset(item, 0, sizeof(*item));
    item->name = core_strdup(name);
    item->kind = CORE_DATA_TABLE_TYPED;
    item->as.table_typed.column_count = column_count;
    item->as.table_typed.row_count = row_count;
    item->as.table_typed.columns = (CoreTableColumnTyped *)core_calloc(column_count, sizeof(CoreTableColumnTyped));

    if (!item->name || !item->as.table_typed.columns) {
        free_item(item);
        dataset->item_count--;
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    for (uint32_t c = 0; c < column_count; ++c) {
        size_t elem = column_type_size(column_types[c]);
        if (elem == 0 || !column_data[c] || !column_names[c]) {
            free_item(item);
            dataset->item_count--;
            CoreResult r = { CORE_ERR_INVALID_ARG, "invalid column type or data" };
            return r;
        }

        item->as.table_typed.columns[c].name = core_strdup(column_names[c]);
        item->as.table_typed.columns[c].type = column_types[c];
        item->as.table_typed.columns[c].as.raw = core_alloc(elem * row_count);

        if (!item->as.table_typed.columns[c].name || !item->as.table_typed.columns[c].as.raw) {
            free_item(item);
            dataset->item_count--;
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }

        memcpy(item->as.table_typed.columns[c].as.raw, column_data[c], elem * row_count);
    }

    return core_result_ok();
}

const CoreDataItem *core_dataset_find(const CoreDataset *dataset, const char *name) {
    if (!dataset || !name) return NULL;
    for (size_t i = 0; i < dataset->item_count; ++i) {
        if (dataset->items[i].name && strcmp(dataset->items[i].name, name) == 0) {
            return &dataset->items[i];
        }
    }
    return NULL;
}

const CoreMetadataItem *core_dataset_find_metadata(const CoreDataset *dataset, const char *key) {
    if (!dataset || !key) return NULL;
    for (size_t i = 0; i < dataset->metadata_count; ++i) {
        if (dataset->metadata[i].key && strcmp(dataset->metadata[i].key, key) == 0) {
            return &dataset->metadata[i];
        }
    }
    return NULL;
}
