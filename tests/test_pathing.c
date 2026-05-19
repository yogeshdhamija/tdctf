#include "game/game.h"
#include <stdio.h>

/* Consumer-driven tests for creep pathing (docs/game-design.md §10). Tests drive the
 * public game_* API and inspect Thing state via game_get_state(). No internal
 * BFS / path-progress helpers are touched directly — they're covered
 * transitively.
 *
 * NB: base retriever spawn count is currently 0 — to get a creep on the
 * field the test buys a +1 retriever upgrade (research_turns=1) in turn 1
 * and lets that turn's quiet sim phase elapse so the upgrade completes;
 * turn 2's sim is then the one under test. */

static int         g_assertions;
static int         g_fail;
static const char *g_test;

#define CHECK(cond) do {                                                    \
    g_assertions++;                                                         \
    if (!(cond)) {                                                          \
        fprintf(stderr, "  [%s] FAIL %s:%d: %s\n",                          \
                g_test, __FILE__, __LINE__, #cond);                         \
        g_fail++;                                                           \
    }                                                                       \
} while (0)

static void step_ticks(int n) {
    for (int i = 0; i < n * SIM_FRAMES_PER_TICK; i++) game_frame();
}

static const Thing *first_creep(PlayerID owner) {
    const GameState *s = game_get_state();
    for (int i = 0; i < s->thing_count; i++) {
        const Thing *t = &s->things[i];
        if (t->tag == THING_CREEP && t->alive && t->owner == owner) return t;
    }
    return NULL;
}

static void enter_sim(void) {
    game_lock_in();   /* PHASE_PLAN_RED  → PHASE_PLAN_BLUE */
    game_lock_in();   /* PHASE_PLAN_BLUE → PHASE_SIMULATE  */
}

static void run_sim_to_completion(void) {
    /* Step until the sim phase ends and we're back in PHASE_PLAN_RED.
     * Bounded loop so a regression in phase advancement can't hang the test. */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) return;
    }
}

/* Run turn 1 quietly so the +1 retriever upgrade RED purchased completes,
 * then enter the turn 2 sim where a retriever is guaranteed to spawn. */
static void setup_turn2_with_red_retriever(void) {
    game_buy_creep_upgrade(0); /* RED +1 Retriever, research_turns=1 */
    enter_sim();
    run_sim_to_completion();
    /* now in PHASE_PLAN_RED, turn 2; upgrade is completed → 1 retriever per spawn */
    enter_sim();
}

/* On the default map, RED's demarcated line runs straight from receptacle
 * (4,4) to enemy flag (25,4) along y=4. With no obstructions the retriever
 * should walk one cell per tick, and path_progress should tick up in lockstep
 * — proof that the goal set is "forward path cells", not just the flag. */
static void test_line_following_unobstructed(void) {
    g_test = "line_following_unobstructed";
    game_init();
    setup_turn2_with_red_retriever();
    CHECK(game_get_state()->phase == PHASE_SIMULATE);

    for (int tick = 1; tick <= 5; tick++) {
        step_ticks(1);
        const Thing *t = first_creep(PLAYER_RED);
        CHECK(t != NULL);
        if (!t) return;
        CHECK(t->x == 4 + tick);
        CHECK(t->y == 4);
        CHECK(t->creep.path_progress == tick);
    }
}

/* Tower placed on path index 6 forces a detour. BLOCKER is used so it neither
 * shoots the creep nor generates noise. Invariants:
 *   1. The placement is accepted (paths_valid sees an alternate route).
 *   2. The creep never occupies the blocked cell.
 *   3. path_progress eventually passes index 6 — the creep rejoins the line
 *      at a forward cell, not just any cell. */
static void test_detour_around_blocking_tower(void) {
    g_test = "detour_around_blocking_tower";
    game_init();
    game_set_placement(TOWER_BLOCKER);
    game_grid_click(10, 4);
    const GameState *s = game_get_state();
    CHECK(s->grid[10][4].thing_id != -1);

    setup_turn2_with_red_retriever();
    CHECK(s->phase == PHASE_SIMULATE);

    int max_progress = 0;
    for (int tick = 1; tick <= 15; tick++) {
        step_ticks(1);
        const Thing *t = first_creep(PLAYER_RED);
        CHECK(t != NULL);
        if (!t) return;
        CHECK(!(t->x == 10 && t->y == 4));
        if (t->creep.path_progress > max_progress)
            max_progress = t->creep.path_progress;
    }
    CHECK(max_progress > 6);
}

/* While outbound, path_progress must be monotonically non-decreasing — even
 * across a detour where the creep is off-line for several ticks. */
static void test_path_progress_monotonic(void) {
    g_test = "path_progress_monotonic";
    game_init();
    game_set_placement(TOWER_BLOCKER);
    game_grid_click(10, 4);
    setup_turn2_with_red_retriever();

    int last = 0;
    for (int tick = 1; tick <= 15; tick++) {
        step_ticks(1);
        const Thing *t = first_creep(PLAYER_RED);
        CHECK(t != NULL);
        if (!t || t->creep.has_flag) break;
        CHECK(t->creep.path_progress >= last);
        last = t->creep.path_progress;
    }
}

/* Sanity-check that placement_valid still rejects the obvious illegal cells.
 * This exercises the paths_valid path in placement_valid indirectly — if
 * paths_valid were broken (e.g., always returning true), placing on the
 * receptacle would still be rejected by an earlier check, so we additionally
 * verify a normal valid placement DOES succeed. */
static void test_placement_validity(void) {
    g_test = "placement_validity";
    game_init();
    const GameState *s = game_get_state();

    /* Receptacle cell rejected. */
    game_set_placement(TOWER_GUNNER);
    game_grid_click(s->receptacle_x[PLAYER_RED], s->receptacle_y[PLAYER_RED]);
    CHECK(s->grid[s->receptacle_x[PLAYER_RED]][s->receptacle_y[PLAYER_RED]].thing_id == -1);

    /* Flag cell (RED's own flag is at_home) rejected. */
    game_set_placement(TOWER_GUNNER);
    game_grid_click(s->flags[PLAYER_RED].x, s->flags[PLAYER_RED].y);
    CHECK(s->grid[s->flags[PLAYER_RED].x][s->flags[PLAYER_RED].y].thing_id == -1);

    /* Normal placement on the line succeeds — alternate routes exist. */
    game_set_placement(TOWER_GUNNER);
    game_grid_click(10, 4);
    CHECK(s->grid[10][4].thing_id != -1);
}

int main(void) {
    test_line_following_unobstructed();
    test_detour_around_blocking_tower();
    test_path_progress_monotonic();
    test_placement_validity();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
