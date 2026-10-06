#include "app/app_tile_pipeline_helpers.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

// Checks genuine visible demand separately from empty, consumed and lane-throttled queues.
int main(void) {
    assert(app_tile_submission_frame_scale(17)==1);
    assert(app_tile_submission_frame_scale(34)==2);
    assert(app_tile_submission_frame_scale(51)==3);
    assert(app_tile_submission_frame_scale(120)==4);
    assert(app_tile_submission_frame_scale(NAN)==1);
    assert(app_tile_submission_frame_scale(INFINITY)==1);
    assert(app_tile_submission_frame_scale(-1)==1);
    TileQueueItem items[32] = {0};
    TileQueue queue = {.items=items, .count=32, .capacity=32};
    uint32_t caps[TILE_QUEUE_LANE_COUNT] = {32,32,3,1};
    assert(app_tile_queue_serviceable_count(&queue, 24, caps) == 24);
    assert(app_tile_visible_missing_budget(&queue,2,8)==8);
    queue.index = 30;
    assert(app_tile_queue_serviceable_count(&queue, 24, caps) == 2);
    queue.index = 32;
    assert(app_tile_queue_serviceable_count(&queue, 24, caps) == 0);
    queue.index = 0;
    for (unsigned i=0;i<32;i++) items[i].lane = TILE_QUEUE_LANE_L3_FAR_PREFETCH;
    assert(app_tile_queue_serviceable_count(&queue, 24, caps) == 1);
    assert(app_tile_visible_missing_budget(&queue,2,8)==2);
    caps[3]=0;
    assert(app_tile_queue_serviceable_count(&queue, 24, caps) == 0);
    for (unsigned i=0;i<32;i++) items[i].lane = i<4 ? TILE_QUEUE_LANE_L0_VISIBLE_MISSING : TILE_QUEUE_LANE_L2_NEAR_PREFETCH;
    assert(app_tile_queue_serviceable_count(&queue, 24, caps) == 7);
    assert(app_tile_visible_missing_budget(&queue,2,8)==4);
    assert(app_tile_queue_serviceable_count(&queue, 5, caps) == 5);
    puts("tile submission demand regression passed");
    return 0;
}
