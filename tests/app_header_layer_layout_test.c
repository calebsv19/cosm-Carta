#include "app/app_ui_internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_closef(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.001f);
}

static void test_fits_right_aligned(void) {
    assert_closef(app_header_layer_strip_start_x(100.0f, 500.0f, 300.0f, 0.0f), 300.0f);
}

static void test_overflow_starts_with_right_edge_pinned(void) {
    float start_x = app_header_layer_strip_start_x(100.0f, 250.0f, 600.0f, 0.0f);
    assert_closef(start_x, -250.0f);
    assert_closef(start_x + 600.0f, 350.0f);
}

static void test_scroll_reveals_left_priority_layers(void) {
    assert_closef(app_header_layer_strip_start_x(100.0f, 250.0f, 600.0f, 350.0f), 100.0f);
    assert_closef(app_header_layer_strip_start_x(100.0f, 250.0f, 600.0f, 999.0f), 100.0f);
    assert_closef(app_header_layer_strip_start_x(100.0f, 250.0f, 600.0f, -20.0f), -250.0f);
}

int main(void) {
    test_fits_right_aligned();
    test_overflow_starts_with_right_edge_pinned();
    test_scroll_reveals_left_priority_layers();
    printf("app_header_layer_layout_test: success\n");
    return 0;
}
