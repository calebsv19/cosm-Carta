#include "core_screen_pick.h"

#include <limits.h>
#include <math.h>
#include <string.h>

struct CoreScreenPickCell {
    int32_t cell_x;
    int32_t cell_y;
    int32_t head_index;
    bool occupied;
};

static CoreResult screen_pick_error(CoreError code, const char *message) {
    CoreResult result = {code, message};
    return result;
}

static bool screen_pick_finite(double value) {
    return isfinite(value) != 0;
}

static bool screen_pick_candidate_valid(const CoreScreenPickCandidate *candidate) {
    return candidate && screen_pick_finite(candidate->screen_x) &&
           screen_pick_finite(candidate->screen_y) &&
           screen_pick_finite(candidate->view_depth);
}

static uint64_t screen_pick_hash_cell(int32_t x, int32_t y) {
    uint64_t ux = (uint64_t)(uint32_t)x;
    uint64_t uy = (uint64_t)(uint32_t)y;
    uint64_t value = (ux << 32u) | uy;
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

static size_t screen_pick_next_pow2(size_t value) {
    size_t result = 1u;
    while (result < value && result <= SIZE_MAX / 2u) result <<= 1u;
    return result;
}

static bool screen_pick_cell_coord(double value, double cell_size, int32_t *out_coord) {
    double cell = 0.0;
    if (!out_coord || !screen_pick_finite(value) || !screen_pick_finite(cell_size) ||
        cell_size <= 0.0) {
        return false;
    }
    cell = floor(value / cell_size);
    if (cell < (double)INT32_MIN || cell > (double)INT32_MAX) return false;
    *out_coord = (int32_t)cell;
    return true;
}

static CoreScreenPickCell *screen_pick_find_cell(CoreScreenPickCell *cells,
                                                  size_t capacity,
                                                  int32_t cell_x,
                                                  int32_t cell_y,
                                                  bool create) {
    size_t slot = 0u;
    size_t probe = 0u;
    if (!cells || capacity == 0u) return NULL;
    slot = (size_t)(screen_pick_hash_cell(cell_x, cell_y) & (uint64_t)(capacity - 1u));
    for (probe = 0u; probe < capacity; ++probe) {
        CoreScreenPickCell *cell = &cells[(slot + probe) & (capacity - 1u)];
        if (!cell->occupied) {
            if (!create) return NULL;
            cell->occupied = true;
            cell->cell_x = cell_x;
            cell->cell_y = cell_y;
            cell->head_index = -1;
            return cell;
        }
        if (cell->cell_x == cell_x && cell->cell_y == cell_y) return cell;
    }
    return NULL;
}

static const CoreScreenPickCell *screen_pick_find_cell_const(
    const CoreScreenPickCell *cells,
    size_t capacity,
    int32_t cell_x,
    int32_t cell_y) {
    return screen_pick_find_cell((CoreScreenPickCell *)cells,
                                 capacity,
                                 cell_x,
                                 cell_y,
                                 false);
}

static bool screen_pick_result_before(const CoreScreenPickResult *a,
                                      const CoreScreenPickResult *b,
                                      double epsilon_sq) {
    if (a->distance_sq + epsilon_sq < b->distance_sq) return true;
    if (b->distance_sq + epsilon_sq < a->distance_sq) return false;
    if (a->view_depth > b->view_depth) return true;
    if (a->view_depth < b->view_depth) return false;
    return a->stable_key < b->stable_key;
}

CoreScreenPickConfig core_screen_pick_config_default(void) {
    CoreScreenPickConfig config = {28.0, 32.0, 0.25};
    return config;
}

CoreResult core_screen_pick_config_validate(const CoreScreenPickConfig *config) {
    if (!config || !screen_pick_finite(config->capture_radius_px) ||
        !screen_pick_finite(config->cell_size_px) ||
        !screen_pick_finite(config->distance_tie_epsilon_sq) ||
        config->capture_radius_px <= 0.0 || config->cell_size_px <= 0.0 ||
        config->distance_tie_epsilon_sq < 0.0) {
        return screen_pick_error(CORE_ERR_INVALID_ARG, "invalid screen-pick configuration");
    }
    return core_result_ok();
}

CoreResult core_screen_pick_index_init(CoreScreenPickIndex *out_index,
                                       CoreScreenPickConfig config) {
    CoreResult validation = core_screen_pick_config_validate(&config);
    if (validation.code != CORE_OK || !out_index) {
        return screen_pick_error(CORE_ERR_INVALID_ARG, "invalid screen-pick index init");
    }
    memset(out_index, 0, sizeof(*out_index));
    out_index->config = config;
    out_index->ready = true;
    return core_result_ok();
}

void core_screen_pick_index_destroy(CoreScreenPickIndex *index) {
    if (!index) return;
    core_free(index->candidates);
    core_free(index->next_indices);
    core_free(index->cells);
    memset(index, 0, sizeof(*index));
}

CoreResult core_screen_pick_index_rebuild(CoreScreenPickIndex *index,
                                          const CoreScreenPickCandidate *candidates,
                                          size_t candidate_count,
                                          uint64_t revision) {
    CoreScreenPickCandidate *new_candidates = NULL;
    int32_t *new_next = NULL;
    CoreScreenPickCell *new_cells = NULL;
    size_t cell_capacity = 0u;
    size_t i = 0u;
    if (!index || !index->ready || (candidate_count > 0u && !candidates) ||
        candidate_count > (size_t)INT32_MAX) {
        return screen_pick_error(CORE_ERR_INVALID_ARG, "invalid screen-pick rebuild");
    }
    for (i = 0u; i < candidate_count; ++i) {
        int32_t ignored_x = 0;
        int32_t ignored_y = 0;
        if (!screen_pick_candidate_valid(&candidates[i]) ||
            !screen_pick_cell_coord(candidates[i].screen_x, index->config.cell_size_px, &ignored_x) ||
            !screen_pick_cell_coord(candidates[i].screen_y, index->config.cell_size_px, &ignored_y)) {
            return screen_pick_error(CORE_ERR_INVALID_ARG, "invalid screen-pick candidate");
        }
    }
    if (candidate_count > 0u) {
        if (candidate_count > SIZE_MAX / sizeof(*new_candidates) ||
            candidate_count > SIZE_MAX / sizeof(*new_next) ||
            candidate_count > SIZE_MAX / 2u) {
            return screen_pick_error(CORE_ERR_OUT_OF_MEMORY, "screen-pick capacity overflow");
        }
        cell_capacity = screen_pick_next_pow2(candidate_count * 2u);
        if (cell_capacity < 8u) cell_capacity = 8u;
        new_candidates = core_alloc(candidate_count * sizeof(*new_candidates));
        new_next = core_alloc(candidate_count * sizeof(*new_next));
        new_cells = core_calloc(cell_capacity, sizeof(*new_cells));
        if (!new_candidates || !new_next || !new_cells) {
            core_free(new_candidates);
            core_free(new_next);
            core_free(new_cells);
            return screen_pick_error(CORE_ERR_OUT_OF_MEMORY, "screen-pick allocation failed");
        }
        memcpy(new_candidates, candidates, candidate_count * sizeof(*new_candidates));
        for (i = 0u; i < candidate_count; ++i) {
            int32_t cell_x = 0;
            int32_t cell_y = 0;
            CoreScreenPickCell *cell = NULL;
            (void)screen_pick_cell_coord(new_candidates[i].screen_x,
                                         index->config.cell_size_px,
                                         &cell_x);
            (void)screen_pick_cell_coord(new_candidates[i].screen_y,
                                         index->config.cell_size_px,
                                         &cell_y);
            cell = screen_pick_find_cell(new_cells, cell_capacity, cell_x, cell_y, true);
            if (!cell) {
                core_free(new_candidates);
                core_free(new_next);
                core_free(new_cells);
                return screen_pick_error(CORE_ERR_OUT_OF_MEMORY, "screen-pick hash table full");
            }
            new_next[i] = cell->head_index;
            cell->head_index = (int32_t)i;
        }
    }
    core_free(index->candidates);
    core_free(index->next_indices);
    core_free(index->cells);
    index->candidates = new_candidates;
    index->next_indices = new_next;
    index->cells = new_cells;
    index->candidate_count = candidate_count;
    index->cell_capacity = cell_capacity;
    index->revision = revision;
    return core_result_ok();
}

static CoreResult screen_pick_query_internal(const CoreScreenPickIndex *index,
                                             double screen_x,
                                             double screen_y,
                                             CoreScreenPickResult *out_results,
                                             size_t result_capacity,
                                             size_t *out_result_count) {
    int32_t min_cell_x = 0;
    int32_t max_cell_x = 0;
    int32_t min_cell_y = 0;
    int32_t max_cell_y = 0;
    int64_t cell_y = 0;
    size_t total = 0u;
    const double radius = index ? index->config.capture_radius_px : 0.0;
    const double radius_sq = radius * radius;
    if (!index || !index->ready || !out_result_count ||
        (result_capacity > 0u && !out_results) || !screen_pick_finite(screen_x) ||
        !screen_pick_finite(screen_y)) {
        return screen_pick_error(CORE_ERR_INVALID_ARG, "invalid screen-pick query");
    }
    *out_result_count = 0u;
    if (index->candidate_count == 0u) return core_result_ok();
    if (!screen_pick_cell_coord(screen_x - radius, index->config.cell_size_px, &min_cell_x) ||
        !screen_pick_cell_coord(screen_x + radius, index->config.cell_size_px, &max_cell_x) ||
        !screen_pick_cell_coord(screen_y - radius, index->config.cell_size_px, &min_cell_y) ||
        !screen_pick_cell_coord(screen_y + radius, index->config.cell_size_px, &max_cell_y)) {
        return screen_pick_error(CORE_ERR_INVALID_ARG, "screen-pick query out of range");
    }
    for (cell_y = min_cell_y; cell_y <= (int64_t)max_cell_y; ++cell_y) {
        int64_t cell_x = 0;
        for (cell_x = min_cell_x; cell_x <= (int64_t)max_cell_x; ++cell_x) {
            const CoreScreenPickCell *cell = screen_pick_find_cell_const(
                index->cells, index->cell_capacity, (int32_t)cell_x, (int32_t)cell_y);
            int32_t candidate_index = cell ? cell->head_index : -1;
            while (candidate_index >= 0) {
                const CoreScreenPickCandidate *candidate = &index->candidates[candidate_index];
                double dx = candidate->screen_x - screen_x;
                double dy = candidate->screen_y - screen_y;
                double distance_sq = dx * dx + dy * dy;
                if (distance_sq <= radius_sq) {
                    CoreScreenPickResult result = {
                        true,
                        candidate->stable_key,
                        candidate->payload,
                        distance_sq,
                        candidate->view_depth,
                        0u
                    };
                    size_t stored = total < result_capacity ? total : result_capacity;
                    size_t insert_at = stored;
                    if (result_capacity > 0u) {
                        if (stored == result_capacity) {
                            if (!screen_pick_result_before(
                                    &result,
                                    &out_results[result_capacity - 1u],
                                    index->config.distance_tie_epsilon_sq)) {
                                total += 1u;
                                candidate_index = index->next_indices[candidate_index];
                                continue;
                            }
                            insert_at = result_capacity - 1u;
                        }
                        while (insert_at > 0u &&
                               screen_pick_result_before(&result,
                                                         &out_results[insert_at - 1u],
                                                         index->config.distance_tie_epsilon_sq)) {
                            if (insert_at < result_capacity) {
                                out_results[insert_at] = out_results[insert_at - 1u];
                            }
                            insert_at -= 1u;
                        }
                        if (insert_at < result_capacity) out_results[insert_at] = result;
                    }
                    total += 1u;
                }
                candidate_index = index->next_indices[candidate_index];
            }
        }
    }
    *out_result_count = total;
    return core_result_ok();
}

CoreResult core_screen_pick_query_nearest(const CoreScreenPickIndex *index,
                                          double screen_x,
                                          double screen_y,
                                          CoreScreenPickResult *out_result) {
    CoreScreenPickResult nearest = {0};
    size_t count = 0u;
    CoreResult result = {0};
    if (!out_result) return screen_pick_error(CORE_ERR_INVALID_ARG, "missing screen-pick result");
    result = screen_pick_query_internal(index, screen_x, screen_y, &nearest, 1u, &count);
    if (result.code != CORE_OK) return result;
    nearest.candidate_count = count;
    *out_result = nearest;
    return core_result_ok();
}

CoreResult core_screen_pick_query_ranked(const CoreScreenPickIndex *index,
                                         double screen_x,
                                         double screen_y,
                                         CoreScreenPickResult *out_results,
                                         size_t result_capacity,
                                         size_t *out_result_count) {
    CoreResult result = screen_pick_query_internal(index,
                                                   screen_x,
                                                   screen_y,
                                                   out_results,
                                                   result_capacity,
                                                   out_result_count);
    size_t stored_count = 0u;
    size_t i = 0u;
    if (result.code != CORE_OK) return result;
    stored_count = *out_result_count < result_capacity ? *out_result_count : result_capacity;
    for (i = 0u; i < stored_count; ++i) out_results[i].candidate_count = *out_result_count;
    return core_result_ok();
}
