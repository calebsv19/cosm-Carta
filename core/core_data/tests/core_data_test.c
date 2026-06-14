#include "core_data.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

static void test_happy_path(void) {
    CoreDataset ds;
    core_dataset_init(&ds);

    assert(core_dataset_add_metadata_string(&ds, "source", "physics_sim").code == CORE_OK);
    assert(core_dataset_add_metadata_f64(&ds, "time_seconds", 1.25).code == CORE_OK);
    assert(core_dataset_add_metadata_i64(&ds, "frame_index", 7).code == CORE_OK);
    assert(core_dataset_add_metadata_bool(&ds, "paused", false).code == CORE_OK);

    const CoreMetadataItem *meta = core_dataset_find_metadata(&ds, "frame_index");
    assert(meta != NULL);
    assert(meta->type == CORE_META_I64);
    assert(meta->as.i64_value == 7);

    assert(core_dataset_add_scalar_f64(&ds, "time_seconds_scalar", 1.25).code == CORE_OK);

    float arr[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    assert(core_dataset_add_array_f32(&ds, "samples", arr, 4).code == CORE_OK);

    CoreField2DDesc d = {2, 2, 0.0f, 0.0f, 1.0f};
    float density[4] = {0.2f, 0.4f, 0.6f, 0.8f};
    float velx[4] = {1.0f, 0.0f, -1.0f, 0.5f};
    float vely[4] = {0.5f, -0.5f, 0.25f, -0.25f};

    assert(core_dataset_add_field2d_f32(&ds, "density", d, density).code == CORE_OK);
    assert(core_dataset_add_field2d_vec2f32(&ds, "velocity", d, velx, vely).code == CORE_OK);

    const char *cols[3] = {"time", "density", "energy"};
    float rows[6] = {
        0.0f, 0.1f, 1.5f,
        1.0f, 0.2f, 1.7f
    };
    assert(core_dataset_add_table_f32(&ds, "metrics", cols, 3, 2, rows).code == CORE_OK);

    const CoreDataItem *item = core_dataset_find(&ds, "velocity");
    assert(item != NULL);
    assert(item->kind == CORE_DATA_FIELD2D_VEC2F32);

    const CoreDataItem *table = core_dataset_find(&ds, "metrics");
    assert(table != NULL);
    assert(table->kind == CORE_DATA_TABLE_F32);
    assert(table->as.table_f32.column_count == 3);
    assert(table->as.table_f32.row_count == 2);
    assert(table->as.table_f32.columns[1].values[0] == 0.1f);
    assert(table->as.table_f32.columns[2].values[1] == 1.7f);

    const char *typed_cols[4] = {"step", "energy64", "active", "bucket"};
    CoreTableColumnType typed_types[4] = {
        CORE_TABLE_COL_I64, CORE_TABLE_COL_F64, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_U32
    };
    int64_t step_vals[2] = {100, 101};
    double energy_vals[2] = {9.5, 9.75};
    bool active_vals[2] = {true, false};
    uint32_t bucket_vals[2] = {3u, 4u};
    const void *typed_data[4] = {step_vals, energy_vals, active_vals, bucket_vals};

    assert(core_dataset_add_table_typed(&ds,
                                        "metrics_typed",
                                        typed_cols,
                                        typed_types,
                                        4,
                                        2,
                                        typed_data).code == CORE_OK);

    const CoreDataItem *typed = core_dataset_find(&ds, "metrics_typed");
    assert(typed != NULL);
    assert(typed->kind == CORE_DATA_TABLE_TYPED);
    assert(typed->as.table_typed.column_count == 4);
    assert(typed->as.table_typed.row_count == 2);
    assert(typed->as.table_typed.columns[0].as.i64_values[1] == 101);
    assert(typed->as.table_typed.columns[1].as.f64_values[0] == 9.5);
    assert(typed->as.table_typed.columns[2].as.bool_values[0] == true);
    assert(typed->as.table_typed.columns[3].as.u32_values[1] == 4u);

    core_dataset_free(&ds);
}

static void test_duplicate_policy(void) {
    CoreDataset ds;
    CoreResult r;
    float values[2] = {1.0f, 2.0f};

    core_dataset_init(&ds);

    r = core_dataset_add_metadata_string(&ds, "source", "first");
    assert(r.code == CORE_OK);
    r = core_dataset_add_metadata_string(&ds, "source", "second");
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_scalar_f64(&ds, "series", 1.0);
    assert(r.code == CORE_OK);
    r = core_dataset_add_array_f32(&ds, "series", values, 2);
    assert(r.code == CORE_ERR_INVALID_ARG);

    core_dataset_free(&ds);
}

static void test_invalid_inputs(void) {
    CoreDataset ds;
    CoreResult r;
    float table_rows[2] = {1.0f, 2.0f};
    const char *table_cols_ok[2] = {"a", "b"};
    const char *table_cols_bad[2] = {"a", ""};
    const char *typed_cols_ok[2] = {"a", "b"};
    const char *typed_cols_bad[2] = {"a", NULL};
    CoreTableColumnType typed_types_ok[2] = {CORE_TABLE_COL_I64, CORE_TABLE_COL_BOOL};
    CoreTableColumnType typed_types_bad[2] = {CORE_TABLE_COL_I64, (CoreTableColumnType)999};
    int64_t typed_steps[1] = {1};
    bool typed_flags[1] = {true};
    const void *typed_data_ok[2] = {typed_steps, typed_flags};
    const void *typed_data_missing[2] = {typed_steps, NULL};

    core_dataset_init(&ds);

    r = core_dataset_add_metadata_string(&ds, "", "bad");
    assert(r.code == CORE_ERR_INVALID_ARG);
    r = core_dataset_add_scalar_f64(&ds, "", 1.0);
    assert(r.code == CORE_ERR_INVALID_ARG);
    r = core_dataset_add_array_f32(&ds, "", table_rows, 2);
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_table_f32(&ds, "table_bad_name", table_cols_bad, 2, 1, table_rows);
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_table_typed(&ds, "typed_bad_col_name", typed_cols_bad, typed_types_ok, 2, 1, typed_data_ok);
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_table_typed(&ds, "typed_bad_type", typed_cols_ok, typed_types_bad, 2, 1, typed_data_ok);
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_table_typed(&ds, "typed_missing_data", typed_cols_ok, typed_types_ok, 2, 1, typed_data_missing);
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_table_f32(&ds, "table_ok", table_cols_ok, 2, 1, table_rows);
    assert(r.code == CORE_OK);

    core_dataset_free(&ds);
}

static void test_zero_size_policy(void) {
    CoreDataset ds;
    CoreResult r;
    const char *cols_f32[2] = {"x", "y"};
    const char *cols_typed[2] = {"count", "enabled"};
    CoreTableColumnType typed_types[2] = {CORE_TABLE_COL_U32, CORE_TABLE_COL_BOOL};
    const void *typed_data[2] = {NULL, NULL};

    core_dataset_init(&ds);

    r = core_dataset_add_array_f32(&ds, "empty_array", NULL, 0);
    assert(r.code == CORE_OK);

    r = core_dataset_add_table_f32(&ds, "empty_table", cols_f32, 2, 0, NULL);
    assert(r.code == CORE_OK);

    r = core_dataset_add_table_typed(&ds, "empty_typed_table", cols_typed, typed_types, 2, 0, typed_data);
    assert(r.code == CORE_OK);

    assert(ds.item_count == 3);
    assert(ds.items[0].as.array_f32.values == NULL);
    assert(ds.items[1].as.table_f32.columns[0].values == NULL);
    assert(ds.items[2].as.table_typed.columns[0].as.raw == NULL);

    core_dataset_free(&ds);
}

static void test_capacity_growth_and_overflow_guards(void) {
    CoreDataset ds;
    CoreResult r;
    float sample = 1.0f;
    CoreField2DDesc huge_field = {UINT32_MAX, UINT32_MAX, 0.0f, 0.0f, 1.0f};

    core_dataset_init(&ds);

    for (int i = 0; i < 20; ++i) {
        char key[32];
        char name[32];

        (void)snprintf(key, sizeof(key), "meta_%d", i);
        (void)snprintf(name, sizeof(name), "scalar_%d", i);

        r = core_dataset_add_metadata_i64(&ds, key, i);
        assert(r.code == CORE_OK);
        r = core_dataset_add_scalar_f64(&ds, name, (double)i);
        assert(r.code == CORE_OK);
    }

    assert(ds.metadata_count == 20);
    assert(ds.item_count == 20);
    assert(ds.metadata_capacity >= ds.metadata_count);
    assert(ds.item_capacity >= ds.item_count);

    r = core_dataset_add_field2d_f32(&ds, "huge_field", huge_field, &sample);
    assert(r.code == CORE_ERR_INVALID_ARG);

    r = core_dataset_add_field2d_vec2f32(&ds, "huge_vec_field", huge_field, &sample, &sample);
    assert(r.code == CORE_ERR_INVALID_ARG);

    core_dataset_free(&ds);
}

int main(void) {
    test_happy_path();
    test_duplicate_policy();
    test_invalid_inputs();
    test_zero_size_policy();
    test_capacity_growth_and_overflow_guards();
    return 0;
}
