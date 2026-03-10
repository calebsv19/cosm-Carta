#include "core_data.h"

#include <assert.h>

int main(void) {
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

    assert(core_dataset_add_scalar_f64(&ds, "time_seconds", 1.25).code == CORE_OK);

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
    return 0;
}
