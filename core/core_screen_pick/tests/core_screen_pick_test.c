#include "core_screen_pick.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static CoreScreenPickIndex make_index(void) {
    CoreScreenPickIndex index;
    assert(core_screen_pick_index_init(&index, core_screen_pick_config_default()).code == CORE_OK);
    return index;
}

static void test_radius_and_nearest(void) {
    CoreScreenPickIndex index = make_index();
    CoreScreenPickCandidate candidates[] = {
        {1u, 10, 10.0, 0.0, 1.0},
        {2u, 20, 27.9, 0.0, 2.0},
        {3u, 30, 28.1, 0.0, 3.0}
    };
    CoreScreenPickResult result = {0};
    assert(core_screen_pick_index_rebuild(&index, candidates, 3u, 11u).code == CORE_OK);
    assert(core_screen_pick_query_nearest(&index, 0.0, 0.0, &result).code == CORE_OK);
    assert(result.found && result.stable_key == 1u && result.payload == 10);
    assert(result.candidate_count == 2u);
    assert(core_screen_pick_query_nearest(&index, -40.0, 0.0, &result).code == CORE_OK);
    assert(!result.found);
    core_screen_pick_index_destroy(&index);
}

static void test_depth_and_stable_ties(void) {
    CoreScreenPickIndex index = make_index();
    CoreScreenPickCandidate candidates[] = {
        {9u, 90, -4.0, -4.0, 1.0},
        {7u, 70, 4.0, 4.0, 5.0},
        {3u, 30, 4.0, 4.0, 5.0}
    };
    CoreScreenPickResult ranked[3] = {{0}};
    size_t count = 0u;
    assert(core_screen_pick_index_rebuild(&index, candidates, 3u, 12u).code == CORE_OK);
    assert(core_screen_pick_query_ranked(&index, 0.0, 0.0, ranked, 3u, &count).code == CORE_OK);
    assert(count == 3u);
    assert(ranked[0].stable_key == 3u);
    assert(ranked[1].stable_key == 7u);
    assert(ranked[2].stable_key == 9u);
    core_screen_pick_index_destroy(&index);
}

static void test_invalid_rebuild_preserves_index(void) {
    CoreScreenPickIndex index = make_index();
    CoreScreenPickCandidate good = {4u, 40, -10.0, -10.0, 2.0};
    CoreScreenPickCandidate bad = {5u, 50, NAN, 0.0, 0.0};
    CoreScreenPickResult result = {0};
    assert(core_screen_pick_index_rebuild(&index, &good, 1u, 13u).code == CORE_OK);
    assert(core_screen_pick_index_rebuild(&index, &bad, 1u, 14u).code == CORE_ERR_INVALID_ARG);
    assert(index.revision == 13u);
    assert(core_screen_pick_query_nearest(&index, -10.0, -10.0, &result).code == CORE_OK);
    assert(result.found && result.stable_key == 4u);
    core_screen_pick_index_destroy(&index);
}

static bool brute_before(const CoreScreenPickCandidate *a,
                         double a_dist,
                         const CoreScreenPickCandidate *b,
                         double b_dist) {
    if (a_dist + 0.25 < b_dist) return true;
    if (b_dist + 0.25 < a_dist) return false;
    if (a->view_depth != b->view_depth) return a->view_depth > b->view_depth;
    return a->stable_key < b->stable_key;
}

static void test_large_grid_matches_bruteforce(void) {
    const size_t count = 100000u;
    CoreScreenPickCandidate *candidates = malloc(count * sizeof(*candidates));
    CoreScreenPickIndex index = make_index();
    CoreScreenPickResult result = {0};
    const double query_x = 417.25;
    const double query_y = -233.75;
    size_t brute_index = SIZE_MAX;
    double brute_dist = 0.0;
    size_t i = 0u;
    assert(candidates != NULL);
    for (i = 0u; i < count; ++i) {
        candidates[i].stable_key = (uint64_t)i + 1u;
        candidates[i].payload = (int64_t)i;
        candidates[i].screen_x = (double)((int)(i % 1000u) - 500) * 3.25;
        candidates[i].screen_y = (double)((int)(i / 1000u) - 50) * 6.5;
        candidates[i].view_depth = (double)(i % 17u);
        {
            double dx = candidates[i].screen_x - query_x;
            double dy = candidates[i].screen_y - query_y;
            double dist = dx * dx + dy * dy;
            if (dist <= 28.0 * 28.0 &&
                (brute_index == SIZE_MAX ||
                 brute_before(&candidates[i], dist, &candidates[brute_index], brute_dist))) {
                brute_index = i;
                brute_dist = dist;
            }
        }
    }
    assert(brute_index != SIZE_MAX);
    assert(core_screen_pick_index_rebuild(&index, candidates, count, 15u).code == CORE_OK);
    assert(core_screen_pick_query_nearest(&index, query_x, query_y, &result).code == CORE_OK);
    assert(result.found);
    assert(result.stable_key == candidates[brute_index].stable_key);
    assert(fabs(result.distance_sq - brute_dist) < 1e-9);
    core_screen_pick_index_destroy(&index);
    free(candidates);
}

int main(void) {
    test_radius_and_nearest();
    test_depth_and_stable_ties();
    test_invalid_rebuild_preserves_index();
    test_large_grid_matches_bruteforce();
    puts("core_screen_pick_test: ok");
    return 0;
}
