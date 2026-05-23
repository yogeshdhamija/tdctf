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
    game_buy_creep_upgrade(0);              /* RED +1 retriever, 1 turn research */
    game_lock_in();                         /* → PLAN_BLUE */
    game_lock_in();                         /* → PRE_SIM   */
    game_choose_sim_view(PLAYER_RED);       /* → SIMULATE turn 1 (upgrade still researching) */
    /* Drain the quiet sim. */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) break;
    }
    game_lock_in(); game_lock_in();
    game_choose_sim_view(PLAYER_RED);       /* → SIMULATE turn 2: retriever spawns */
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

/* "Pathing wobble" per docs/game-design.md §10: when multiple shortest paths
 * exist the creep picks uniformly at random among them. Driven by
 * gs->rng_state — same RNG state means the same pick, so this is observable
 * from outside. We construct a spawn/flag pair with two tied first steps
 * (horizontal and vertical) and walk the RNG forward across many calls; both
 * choices must appear, otherwise the random tiebreak is broken. */
static void test_wobble_picks_among_ties(void) {
    g_test = "wobble_picks_among_ties";
    GameState gs;
    /* Spawn (0,0), flag (5,5). Manhattan distance 10; from (0,0) the two
     * shortest-path neighbours are (1,0) and (0,1). */
    minimal_state(&gs, 10, 10, 0, 0, 5, 5, 9, 9);
    /* xorshift32 is a fixed point at 0 — without a real seed every "random"
     * pick would be index 0 and the test would silently pass on broken code. */
    gs.rng_state = 0x12345678u;

    int saw_h = 0, saw_v = 0;
    for (int i = 0; i < 64 && !(saw_h && saw_v); i++) {
        int nx, ny;
        CHECK(game_pathing_next_step(&gs, 0, 0, PLAYER_RED, 0, &nx, &ny));
        if (nx == 1 && ny == 0) saw_h = 1;
        if (nx == 0 && ny == 1) saw_v = 1;
        /* No other first step can be shortest — guard against drift. */
        CHECK((nx == 1 && ny == 0) || (nx == 0 && ny == 1));
    }
    CHECK(saw_h);
    CHECK(saw_v);
}

/* Wobble is RNG-driven, so the same RNG state must always produce the same
 * pick. This is what makes snapshot resumption reproduce the exact same
 * timeline (the snapshot round-trips rng_state). */
static void test_wobble_is_deterministic_per_rng_state(void) {
    g_test = "wobble_is_deterministic_per_rng_state";
    GameState a, b;
    minimal_state(&a, 10, 10, 0, 0, 5, 5, 9, 9);
    minimal_state(&b, 10, 10, 0, 0, 5, 5, 9, 9);
    a.rng_state = b.rng_state = 0xC0FFEEu;

    for (int i = 0; i < 16; i++) {
        int ax, ay, bx, by;
        CHECK(game_pathing_next_step(&a, 0, 0, PLAYER_RED, 0, &ax, &ay));
        CHECK(game_pathing_next_step(&b, 0, 0, PLAYER_RED, 0, &bx, &by));
        CHECK(ax == bx && ay == by);
    }
}

/* When only one neighbour is on a shortest path, the result is deterministic
 * and no RNG is consumed — important so that paths_valid (which doesn't pass
 * a step buffer) and trivially-routed creeps don't burn entropy that the
 * crit rolls depend on for their timeline. */
static void test_single_shortest_step_does_not_consume_rng(void) {
    g_test = "single_shortest_step_does_not_consume_rng";
    GameState gs;
    /* Spawn (0,0), flag (5,0) on the same row: only (1,0) is one step
     * closer to the goal. */
    minimal_state(&gs, 10, 10, 0, 0, 5, 0, 9, 0);
    gs.rng_state = 0xDEADBEEFu;
    uint32_t before = gs.rng_state;
    int nx, ny;
    CHECK(game_pathing_next_step(&gs, 0, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK(nx == 1 && ny == 0);
    CHECK(gs.rng_state == before);
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
    /* Creep at (1,0), visited_flag = 0. Flag is carried, so the goal
     * degenerates to receptacle (9,9). From (1,0), wobble picks one of the
     * two shortest first steps — (2,0) east, (1,1) south — uniformly. */
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK((nx == 2 && ny == 0) || (nx == 1 && ny == 1));
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
    /* Creep at (1,0). Goal moved to (0,2). Two tied shortest first steps:
     * (0,0) [via (1,0)→(0,0)→(0,1)→(0,2)] and (1,1) [via (1,0)→(1,1)→(0,1)
     * →(0,2)]. Either is a valid wobble pick. */
    CHECK(game_pathing_next_step(&gs, 1, 0, PLAYER_RED, 0, &nx, &ny));
    CHECK((nx == 0 && ny == 0) || (nx == 1 && ny == 1));
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void) {
    test_retriever_walks_full_loop_and_wins();
    test_goal_switches_on_visited_flag();
    test_wobble_picks_among_ties();
    test_wobble_is_deterministic_per_rng_state();
    test_single_shortest_step_does_not_consume_rng();
    test_detour_around_obstacle();
    test_carried_flag_is_not_a_goal();
    test_dropped_flag_is_a_goal();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
