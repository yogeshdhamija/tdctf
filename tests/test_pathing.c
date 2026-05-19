#include "game/game.h"
#include <stdio.h>
#include <string.h>

/* Pathing tests per docs/game-design.md §10 and docs/ai-instructions/coding-norms.md
 * "Layered Testing".
 *
 * ONE test exercises a full game board — the happy-path retriever loop. All
 * other branches in the pathing logic are tested one layer down: against
 * game_pathing_next_step with a hand-built GameState. No game_init, no turn
 * machinery, no spawning — just "given this map, where would the next step
 * go?". A regression in a branch points at exactly the one test that covers
 * it. */

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

/* ── full-board test ─────────────────────────────────────────────────── */

/* The single feature-level test: a retriever spawned at the RED spawn cell
 * walks the entire line — outbound to the enemy flag, picks it up at the
 * flag's cell, and continues along the return half to its own receptacle,
 * triggering the win condition. Exercises one path through every layer
 * (spawn → unified pathing → flag pickup → win check). */
static void test_retriever_walks_full_loop_and_wins(void) {
    g_test = "retriever_walks_full_loop_and_wins";
    game_init();
    game_buy_creep_upgrade(0);   /* RED +1 retriever, 1 turn research */
    game_lock_in();              /* → PLAN_BLUE */
    game_lock_in();              /* → SIMULATE turn 1 (upgrade still researching) */
    /* Drain the quiet sim. */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) break;
    }
    game_lock_in(); game_lock_in(); /* → SIMULATE turn 2: retriever spawns */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_GAME_OVER) break;
    }
    const GameState *s = game_get_state();
    CHECK(s->phase == PHASE_GAME_OVER);
    CHECK(s->winner == PLAYER_RED);
}

/* ── lower-level tests: game_pathing_next_step ───────────────────────── */

/* Construct a minimal walkable state with the given line endpoints. RED is
 * the acting player; BLUE owns the enemy flag at (flag_x, flag_y). The
 * caller can post-hoc block grid cells (gs.grid[x][y].thing_id = nonzero)
 * or move/carry the flag to construct each scenario. */
static void minimal_state(GameState *gs, int w, int h,
                          int sx, int sy, int fx, int fy, int rx, int ry) {
    memset(gs, 0, sizeof(*gs));
    gs->grid_w = w; gs->grid_h = h;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++)
            gs->grid[x][y].thing_id = -1;
    for (int k = 0; k < 2; k++) {
        gs->flags[k].carried_by = -1;
        gs->flags[k].at_home    = 1;
    }
    gs->spawn_x[PLAYER_RED]      = sx; gs->spawn_y[PLAYER_RED]      = sy;
    gs->receptacle_x[PLAYER_RED] = rx; gs->receptacle_y[PLAYER_RED] = ry;
    gs->flags[PLAYER_BLUE].x = fx; gs->flags[PLAYER_BLUE].y = fy;
    gs->flags[PLAYER_BLUE].owner = PLAYER_BLUE;
    game_build_path(gs, PLAYER_RED);
}

/* On an unobstructed grid the creep walks the line one cell at a time. Two
 * probes: at spawn moving toward the flag, and at the flag moving onto the
 * return half. */
static void test_next_step_follows_line(void) {
    g_test = "next_step_follows_line";
    GameState gs;
    /* Path: (0,0)..(5,0)[flag]..(5,5)[receptacle]. Length 11. */
    minimal_state(&gs, 10, 10, /*spawn*/0,0, /*flag*/5,0, /*recept*/5,5);

    int nx, ny;
    CHECK(game_pathing_next_step(&gs, 0, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 1 && ny == 0);

    /* At flag cell (5,0) with path_progress = 5: the flag itself is a goal
     * but is excluded as the creep's own cell, so the next step is path[6]. */
    CHECK(game_pathing_next_step(&gs, 5, 0, PLAYER_RED, 5, &nx, &ny));
    CHECK(nx == 5 && ny == 1);
}

/* A blocked line cell forces BFS to reroute through off-line walkable cells.
 * The first step must be off the line, not into the blocked cell. */
static void test_detour_around_obstacle(void) {
    g_test = "detour_around_obstacle";
    GameState gs;
    minimal_state(&gs, 10, 10, 0,0, 5,0, 5,5);
    gs.grid[2][0].thing_id = 99;   /* block path[2] */

    int nx, ny;
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 1, &nx, &ny));
    /* (2,0) blocked → BFS finds (3,0) via (1,0)→(1,1)→(2,1)→(3,1)→(3,0).
     * BFS expansion order at (1,0) is right→left→down→up; right is blocked
     * and left enters a dead corner first, so the shortest goal is reached
     * through (1,1). */
    CHECK(nx == 1 && ny == 1);
}

/* A dropped flag (carried_by == -1, off the static line) becomes an extra
 * goal cell. If it's the closest unvisited goal, BFS steers the creep
 * toward it instead of toward the line. */
static void test_dropped_flag_is_a_goal(void) {
    g_test = "dropped_flag_is_a_goal";
    GameState gs;
    minimal_state(&gs, 10, 10, 0,0, 5,0, 5,5);
    /* Wall off the outbound half so the closest line cell is far. */
    gs.grid[2][0].thing_id = 99;
    gs.grid[3][0].thing_id = 99;
    gs.grid[4][0].thing_id = 99;
    /* Move the flag to (0,2) — dropped, off-line. */
    gs.flags[PLAYER_BLUE].x = 0;
    gs.flags[PLAYER_BLUE].y = 2;
    gs.flags[PLAYER_BLUE].at_home = 0;
    gs.flags[PLAYER_BLUE].carried_by = -1;

    int nx, ny;
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 1, &nx, &ny));
    /* Flag at (0,2) is 3 steps from (1,0); the nearest reachable line cell
     * is (5,1) at 5+ steps. BFS heads for the flag — first step (0,0). */
    CHECK(nx == 0 && ny == 0);
}

/* The same geometry as the previous test, but the flag is being carried
 * (carried_by != -1). The flag's coords are NOT in the goal set, so the
 * creep falls back to the static line and BFS heads for the nearest path
 * cell — a different first step. */
static void test_carried_flag_is_not_a_goal(void) {
    g_test = "carried_flag_is_not_a_goal";
    GameState gs;
    minimal_state(&gs, 10, 10, 0,0, 5,0, 5,5);
    gs.grid[2][0].thing_id = 99;
    gs.grid[3][0].thing_id = 99;
    gs.grid[4][0].thing_id = 99;
    gs.flags[PLAYER_BLUE].x = 0;
    gs.flags[PLAYER_BLUE].y = 2;
    gs.flags[PLAYER_BLUE].at_home = 0;
    gs.flags[PLAYER_BLUE].carried_by = 7;   /* in-flight on some other creep */

    int nx, ny;
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 1, &nx, &ny));
    /* No flag-goal: BFS routes around the wall to reach (5,1) via
     * (1,0)→(1,1)→(2,1)→(3,1)→(4,1)→(5,1). First step (1,1) — distinct
     * from (0,0) in the dropped-flag case, proving the conditional. */
    CHECK(nx == 1 && ny == 1);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void) {
    test_retriever_walks_full_loop_and_wins();
    test_next_step_follows_line();
    test_detour_around_obstacle();
    test_dropped_flag_is_a_goal();
    test_carried_flag_is_not_a_goal();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
