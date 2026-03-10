#ifndef CORE_DATA_H
#define CORE_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_base.h"

typedef enum CoreDataKind {
    CORE_DATA_SCALAR_F64 = 1,
    CORE_DATA_ARRAY_F32 = 2,
    CORE_DATA_FIELD2D_F32 = 3,
    CORE_DATA_FIELD2D_VEC2F32 = 4,
    CORE_DATA_TABLE_F32 = 5,
    CORE_DATA_TABLE_TYPED = 6
} CoreDataKind;

typedef enum CoreMetaType {
    CORE_META_STRING = 1,
    CORE_META_F64 = 2,
    CORE_META_I64 = 3,
    CORE_META_BOOL = 4
} CoreMetaType;

typedef struct CoreField2DDesc {
    uint32_t width;
    uint32_t height;
    float origin_x;
    float origin_y;
    float cell_size;
} CoreField2DDesc;

typedef struct CoreMetadataItem {
    char *key;
    CoreMetaType type;
    union {
        char *string_value;
        double f64_value;
        int64_t i64_value;
        bool bool_value;
    } as;
} CoreMetadataItem;

typedef struct CoreTableColumnF32 {
    char *name;
    float *values;
} CoreTableColumnF32;

typedef enum CoreTableColumnType {
    CORE_TABLE_COL_F32 = 1,
    CORE_TABLE_COL_F64 = 2,
    CORE_TABLE_COL_I64 = 3,
    CORE_TABLE_COL_U32 = 4,
    CORE_TABLE_COL_BOOL = 5
} CoreTableColumnType;

typedef struct CoreTableColumnTyped {
    char *name;
    CoreTableColumnType type;
    union {
        float *f32_values;
        double *f64_values;
        int64_t *i64_values;
        uint32_t *u32_values;
        bool *bool_values;
        void *raw;
    } as;
} CoreTableColumnTyped;

typedef struct CoreDataItem {
    char *name;
    CoreDataKind kind;
    union {
        double scalar_f64;
        struct {
            float *values;
            uint32_t count;
        } array_f32;
        struct {
            CoreField2DDesc desc;
            float *values;
        } field2d_f32;
        struct {
            CoreField2DDesc desc;
            float *x;
            float *y;
        } field2d_vec2f32;
        struct {
            CoreTableColumnF32 *columns;
            uint32_t column_count;
            uint32_t row_count;
        } table_f32;
        struct {
            CoreTableColumnTyped *columns;
            uint32_t column_count;
            uint32_t row_count;
        } table_typed;
    } as;
} CoreDataItem;

typedef struct CoreDataset {
    CoreDataItem *items;
    size_t item_count;
    size_t item_capacity;

    CoreMetadataItem *metadata;
    size_t metadata_count;
    size_t metadata_capacity;
} CoreDataset;

void core_dataset_init(CoreDataset *dataset);
void core_dataset_free(CoreDataset *dataset);

CoreResult core_dataset_add_metadata(CoreDataset *dataset, const char *key, const char *value);
CoreResult core_dataset_add_metadata_string(CoreDataset *dataset, const char *key, const char *value);
CoreResult core_dataset_add_metadata_f64(CoreDataset *dataset, const char *key, double value);
CoreResult core_dataset_add_metadata_i64(CoreDataset *dataset, const char *key, int64_t value);
CoreResult core_dataset_add_metadata_bool(CoreDataset *dataset, const char *key, bool value);

CoreResult core_dataset_add_scalar_f64(CoreDataset *dataset, const char *name, double value);
CoreResult core_dataset_add_array_f32(CoreDataset *dataset, const char *name, const float *values, uint32_t count);
CoreResult core_dataset_add_field2d_f32(CoreDataset *dataset, const char *name, CoreField2DDesc desc, const float *values);
CoreResult core_dataset_add_field2d_vec2f32(CoreDataset *dataset, const char *name, CoreField2DDesc desc, const float *x, const float *y);
CoreResult core_dataset_add_table_f32(CoreDataset *dataset,
                                      const char *name,
                                      const char *const *column_names,
                                      uint32_t column_count,
                                      uint32_t row_count,
                                      const float *values_row_major);
CoreResult core_dataset_add_table_typed(CoreDataset *dataset,
                                        const char *name,
                                        const char *const *column_names,
                                        const CoreTableColumnType *column_types,
                                        uint32_t column_count,
                                        uint32_t row_count,
                                        const void *const *column_data);

const CoreDataItem *core_dataset_find(const CoreDataset *dataset, const char *name);
const CoreMetadataItem *core_dataset_find_metadata(const CoreDataset *dataset, const char *key);

#endif
