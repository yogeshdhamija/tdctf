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
 * walks the shortest path to the enemy flag, picks it up, and walks the
 * shortest path back to its own receptacle — triggering the win condition.
 * Exercises one path through every layer (spawn → BFS pathing → flag
 * pickup → visited_flag flip → BFS pathing to receptacle → win check). */
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

/* Construct a minimal walkable state with the given endpoints. RED is the
 * acting player; BLUE owns the enemy flag at (fx, fy). The caller can
 * post-hoc block grid cells (gs.grid[x][y].thing_id = nonzero) or move the
 * flag to construct each scenario. */
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
}

/* Pre-flag (visited_flag == 0): the goal is the enemy flag's current cell.
 * Post-flag (visited_flag == 1): the goal is the creep's own receptacle. */
static void test_goal_switches_on_visited_flag(void) {
    g_test = "goal_switches_on_visited_flag";
    GameState gs;
    /* Spawn (0,0), flag (5,0), receptacle (9,0). All on the same row so
     * the BFS step is deterministic regardless of tiebreak. */
    minimal_state(&gs, 10, 10, 0, 0, 5, 0, 9, 0);

    int nx, ny;
    /* visited_flag == 0: BFS heads east toward the flag. */
    CHECK(game_pathing_next_step(&gs, 0, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 1 && ny == 0);

    /* visited_flag == 1, sitting at the flag: BFS heads further east
     * toward the receptacle. */
    CHECK(game_pathing_next_step(&gs, 5, 0, PLAYER_RED, 1, &nx, &ny));
    CHECK(nx == 6 && ny == 0);
}

/* When two cells are tied for the shortest distance to the goal, BFS
 * prefers the horizontal one as the first step. */
static void test_horizontal_tiebreak(void) {
    g_test = "horizontal_tiebreak";
    GameState gs;
    /* Spawn (0,0), flag (5,5). Manhattan distance 10; many shortest paths.
     * Horizontal-first means the first step is (1,0), not (0,1). */
    minimal_state(&gs, 10, 10, 0, 0, 5, 5, 9, 9);

    int nx, ny;
    CHECK(game_pathing_next_step(&gs, 0, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 1 && ny == 0);

    /* From (0,5) heading to flag (5,0): horizontal step toward x=5 is
     * also tied with the vertical step toward y=0. Horizontal wins. */
    CHECK(game_pathing_next_step(&gs, 0, 5, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 1 && ny == 5);
}

/* A blocked cell forces BFS to reroute through walkable cells. The first
 * step must be off the blocked direction, but still a shortest path. */
static void test_detour_around_obstacle(void) {
    g_test = "detour_around_obstacle";
    GameState gs;
    minimal_state(&gs, 10, 10, 0, 0, 5, 0, 9, 9);
    gs.grid[2][0].thing_id = 99;   /* block (2,0) */

    int nx, ny;
    /* From (1,0) heading to flag at (5,0) with (2,0) blocked, the only
     * length-5 detour exits via (1,1). */
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 1 && ny == 1);
}

/* Carried-flag exemption: a non-carrier in phase 1 does NOT chase a
 * teammate carrier. Same geometry as the dropped-flag scenario, but the
 * flag is marked as carried, so its location is excluded from the goal
 * set and the creep falls back to heading toward its own receptacle. */
static void test_carried_flag_is_not_a_goal(void) {
    g_test = "carried_flag_is_not_a_goal";
    GameState gs;
    minimal_state(&gs, 10, 10, 0, 0, 5, 0, 9, 9);
    gs.flags[PLAYER_BLUE].x = 0;
    gs.flags[PLAYER_BLUE].y = 2;
    gs.flags[PLAYER_BLUE].at_home    = 0;
    gs.flags[PLAYER_BLUE].carried_by = 7;   /* in-flight on some other creep */

    int nx, ny;
    /* Creep at (1,0), visited_flag = 0. Flag is carried, so goal degenerates
     * to receptacle (9,9). From (1,0), horizontal-first step is (2,0). */
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 2 && ny == 0);
}

/* The flag's current cell is the goal regardless of whether it's at home or
 * has been dropped. Moving the flag changes where the creep heads. */
static void test_dropped_flag_is_a_goal(void) {
    g_test = "dropped_flag_is_a_goal";
    GameState gs;
    minimal_state(&gs, 10, 10, 0, 0, 5, 0, 9, 9);
    gs.flags[PLAYER_BLUE].x = 0;
    gs.flags[PLAYER_BLUE].y = 2;
    gs.flags[PLAYER_BLUE].at_home    = 0;
    gs.flags[PLAYER_BLUE].carried_by = -1;

    int nx, ny;
    /* Creep at (1,0). Goal moved to (0,2). Manhattan 3. BFS picks shortest:
     * horizontal-first means (0,0) is the first step (one of the two tied
     * 3-step paths goes (1,0)→(0,0)→(0,1)→(0,2)). */
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 0 && ny == 0);
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void) {
    test_retriever_walks_full_loop_and_wins();
    test_goal_switches_on_visited_flag();
    test_horizontal_tiebreak();
    test_detour_around_obstacle();
    test_carried_flag_is_not_a_goal();
    test_dropped_flag_is_a_goal();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
