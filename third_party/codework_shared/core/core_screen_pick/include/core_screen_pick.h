#ifndef CORE_SCREEN_PICK_H
#define CORE_SCREEN_PICK_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreScreenPickCandidate {
    uint64_t stable_key;
    int64_t payload;
    double screen_x;
    double screen_y;
    double view_depth;
} CoreScreenPickCandidate;

typedef struct CoreScreenPickConfig {
    double capture_radius_px;
    double cell_size_px;
    double distance_tie_epsilon_sq;
} CoreScreenPickConfig;

typedef struct CoreScreenPickResult {
    bool found;
    uint64_t stable_key;
    int64_t payload;
    double distance_sq;
    double view_depth;
    size_t candidate_count;
} CoreScreenPickResult;

typedef struct CoreScreenPickCell CoreScreenPickCell;

typedef struct CoreScreenPickIndex {
    CoreScreenPickCandidate *candidates;
    int32_t *next_indices;
    CoreScreenPickCell *cells;
    size_t candidate_count;
    size_t cell_capacity;
    CoreScreenPickConfig config;
    uint64_t revision;
    bool ready;
} CoreScreenPickIndex;

CoreScreenPickConfig core_screen_pick_config_default(void);
CoreResult core_screen_pick_config_validate(const CoreScreenPickConfig *config);
CoreResult core_screen_pick_index_init(CoreScreenPickIndex *out_index,
                                       CoreScreenPickConfig config);
void core_screen_pick_index_destroy(CoreScreenPickIndex *index);
CoreResult core_screen_pick_index_rebuild(CoreScreenPickIndex *index,
                                          const CoreScreenPickCandidate *candidates,
                                          size_t candidate_count,
                                          uint64_t revision);
CoreResult core_screen_pick_query_nearest(const CoreScreenPickIndex *index,
                                          double screen_x,
                                          double screen_y,
                                          CoreScreenPickResult *out_result);
CoreResult core_screen_pick_query_ranked(const CoreScreenPickIndex *index,
                                         double screen_x,
                                         double screen_y,
                                         CoreScreenPickResult *out_results,
                                         size_t result_capacity,
                                         size_t *out_result_count);

#ifdef __cplusplus
}
#endif

#endif
