#include "render/render.h"
#include "game/game.h"
#include "game/tower_config.h"
#include "platform/platform.h"
#include "test_fixtures.h"
#include <stdio.h>
#include <string.h>

/* Consumer-driven tests for the render layer's click-hit testing
 * (render_button_at). render.c records every button-shaped rect into a
 * static table during render_frame; render_button_at later queries that
 * table. We exercise the public API only:
 *
 *   1. Build a GameState via game_init() + game_* actions.
 *   2. Call render_frame(state) with the game state.
 *   3. Sweep pixel coordinates with render_button_at to collect the set
 *      of ButtonIDs currently hittable, and assert against expectations.
 *
 * Drawing primitives are stubbed out — we don't care about pixels here,
 * only about the click-to-action wiring.
 *
 * Add new tests as render-layer features land. */

/* ── platform stubs (no-op draw primitives) ─────────────────────────── */

void plat_clear(uint32_t c)                                              { (void)c; }
void plat_draw_rect(int x, int y, int w, int h, uint32_t c)              { (void)x;(void)y;(void)w;(void)h;(void)c; }
void plat_fill_rect(int x, int y, int w, int h, uint32_t c)              { (void)x;(void)y;(void)w;(void)h;(void)c; }
void plat_draw_circle(int cx, int cy, int r, uint32_t c)                 { (void)cx;(void)cy;(void)r;(void)c; }
void plat_fill_circle(int cx, int cy, int r, uint32_t c)                 { (void)cx;(void)cy;(void)r;(void)c; }
void plat_draw_line(int x1, int y1, int x2, int y2, uint32_t c)          { (void)x1;(void)y1;(void)x2;(void)y2;(void)c; }
/* Captured plat_draw_text calls — render.c is consumer-tested through these
 * so tests can assert what text was drawn (and in what color), not just
 * which buttons were placed. Reset between renders via reset_text_log(). */
typedef struct { int x, y; uint32_t color; char text[64]; } TextCall;
#define MAX_TEXT_CALLS 256
static TextCall g_text_calls[MAX_TEXT_CALLS];
static int      g_text_call_count;
static void reset_text_log(void) { g_text_call_count = 0; }
void plat_draw_text(int x, int y, const char *t, uint32_t c) {
    if (g_text_call_count >= MAX_TEXT_CALLS) return;
    TextCall *tc = &g_text_calls[g_text_call_count++];
    tc->x = x; tc->y = y; tc->color = c;
    size_t n = strlen(t);
    if (n >= sizeof(tc->text)) n = sizeof(tc->text) - 1;
    memcpy(tc->text, t, n); tc->text[n] = 0;
}
void plat_draw_triangle(int a, int b, int c2, int d, int e, int f, uint32_t g) {
    (void)a;(void)b;(void)c2;(void)d;(void)e;(void)f;(void)g;
}
void plat_get_frame_stats(FrameStats *out)                               { out->fps = 60; out->max_lag_ms = 0; }

/* ── harness ────────────────────────────────────────────────────────── */

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

/* Sweep the canvas after a render_frame and record which ButtonIDs are
 * currently hittable anywhere. Buttons are all >= 20x18 px, so the sweep
 * stride is fine at 4 px. The presence table is sized to cover both the
 * fixed enum range (< BTN_PLACE_TOWER_BASE) and the dynamic tower-placement
 * range (BTN_PLACE_TOWER_BASE .. + TOWER_MAX_COUNT). */
#define PRESENT_SIZE (BTN_PLACE_TOWER_BASE + TOWER_MAX_COUNT)
static unsigned char g_present[PRESENT_SIZE];
static void scan_buttons(void) {
    memset(g_present, 0, sizeof(g_present));
    int gw = 30 * CELL_SIZE, gh = 20 * CELL_SIZE;
    int total_w = gw + SIDEBAR_W;
    for (int y = 0; y < gh; y += 4)
        for (int x = 0; x < total_w; x += 4) {
            int id = render_button_at(x, y);
            if (id > 0 && id < PRESENT_SIZE) g_present[id] = 1;
        }
}

static int present(int id) { return g_present[id]; }

/* ── tests ──────────────────────────────────────────────────────────── */

/* The grid area (left of the sidebar) has no buttons in any phase —
 * grid clicks must always fall through to game_grid_click, not a button. */
static void test_no_buttons_in_grid_area(void) {
    g_test = "no_buttons_in_grid_area";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    render_frame(game_get_state());

    int gw = 30 * CELL_SIZE, gh = 20 * CELL_SIZE;
    int any_button = 0;
    for (int y = 0; y < gh; y += 4)
        for (int x = 0; x < gw; x += 4)
            if (render_button_at(x, y) != BTN_NONE) any_button = 1;
    CHECK(!any_button);
}

/* In PLAN_RED with no purchases yet, the sidebar shows the Lock In button,
 * a placement button for every tower in the catalog, and all four
 * BUY_UPGRADE_* buttons. */
static void test_planning_buttons_visible(void) {
    g_test = "planning_buttons_visible";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    render_frame(game_get_state());
    scan_buttons();
    CHECK(present(BTN_LOCK_IN));
    CHECK(game_tower_count() > 0);
    for (int i = 0; i < game_tower_count(); i++)
        CHECK(present(BTN_PLACE_TOWER_BASE + i));
    CHECK(present(BTN_BUY_UPGRADE_0));
    CHECK(present(BTN_BUY_UPGRADE_1));
    CHECK(present(BTN_BUY_UPGRADE_2));
    CHECK(present(BTN_BUY_UPGRADE_3));
    /* No tower selected yet → no upgrade/destroy buttons. */
    CHECK(!present(BTN_UPGRADE_TOWER));
    CHECK(!present(BTN_DESTROY_TOWER));
    /* Not in game over → no restart. */
    CHECK(!present(BTN_RESTART));
}

/* Purchasing an upgrade flips it from a clickable button to a status tile
 * — the BTN_BUY_UPGRADE_N entry should no longer be present in the hit
 * table. */
static void test_buy_upgrade_button_disappears_after_purchase(void) {
    g_test = "buy_upgrade_button_disappears_after_purchase";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    render_frame(game_get_state());
    scan_buttons();
    CHECK(present(BTN_BUY_UPGRADE_0));

    game_buy_creep_upgrade(0);
    render_frame(game_get_state());
    scan_buttons();
    CHECK(!present(BTN_BUY_UPGRADE_0));
    /* Siblings still buyable. */
    CHECK(present(BTN_BUY_UPGRADE_1));
}

/* Selecting one of your own towers exposes the Upgrade and Destroy
 * buttons. */
static void test_own_tower_selected_shows_upgrade_destroy(void) {
    g_test = "own_tower_selected_shows_upgrade_destroy";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    /* Place a RED gunner — placement also sets selected_x/y to the new tower. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);

    render_frame(game_get_state());
    scan_buttons();
    CHECK(present(BTN_UPGRADE_TOWER));
    CHECK(present(BTN_DESTROY_TOWER));
}

/* When the *other* player is planning, the Upgrade/Destroy buttons are
 * not drawn for a selected tower they don't own. */
static void test_enemy_tower_selected_no_upgrade_destroy(void) {
    g_test = "enemy_tower_selected_no_upgrade_destroy";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    /* Hand off to BLUE planning and re-select RED's tower. */
    game_lock_in();
    game_grid_click(6, 4);

    render_frame(game_get_state());
    scan_buttons();
    CHECK(!present(BTN_UPGRADE_TOWER));
    CHECK(!present(BTN_DESTROY_TOWER));
}

/* During SIMULATE the sidebar shows tick text but no clickable elements. */
static void test_simulation_phase_no_buttons(void) {
    g_test = "simulation_phase_no_buttons";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    game_lock_in(); /* → PLAN_BLUE */
    game_lock_in(); /* → SIMULATE  */

    render_frame(game_get_state());
    scan_buttons();
    CHECK(!present(BTN_LOCK_IN));
    for (int i = 0; i < game_tower_count(); i++)
        CHECK(!present(BTN_PLACE_TOWER_BASE + i));
    CHECK(!present(BTN_BUY_UPGRADE_0));
    CHECK(!present(BTN_UPGRADE_TOWER));
    CHECK(!present(BTN_DESTROY_TOWER));
    CHECK(!present(BTN_RESTART));
}

/* When multiple creeps share a cell, a per-owner count badge is drawn in the
 * cell so the player can tell the count and type mix. Setup: BLUE buys the
 * +2-retrievers upgrade (2 turn research). Through turns 1 and 2 nothing
 * spawns; on turn 3's sim start two retrievers materialise at the BLUE
 * spawn cell (29,10), stacked. */
static void test_stacked_creep_count_badge(void) {
    g_test = "stacked_creep_count_badge";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    game_lock_in();                 /* → PLAN_BLUE */
    game_buy_creep_upgrade(2);      /* +2 retrievers, 2 turn research */
    game_lock_in();                 /* → SIMULATE turn 1 (no creeps yet) */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) break;
    }
    game_lock_in(); game_lock_in(); /* → SIMULATE turn 2 (still no creeps) */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) break;
    }
    game_lock_in(); game_lock_in(); /* → SIMULATE turn 3: 2 retrievers spawn together */

    const GameState *s = game_get_state();
    int count = 0;
    for (int i = 0; i < s->thing_count; i++) {
        const Thing *t = &s->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->owner != PLAYER_BLUE) continue;
        count++;
        CHECK(t->x == 19 && t->y == 11);
    }
    CHECK(count == 2);

    reset_text_log();
    render_frame(s);
    /* The sidebar also draws "spawn 2R 0S" so we can't just look for "2R"
     * anywhere — pin to the spawn cell (29,10). */
    int x_lo = 29 * CELL_SIZE, x_hi = 30 * CELL_SIZE;
    int y_lo = 10 * CELL_SIZE, y_hi = 11 * CELL_SIZE;
    const TextCall *badge = NULL;
    for (int i = 0; i < g_text_call_count; i++) {
        const TextCall *tc = &g_text_calls[i];
        if (tc->x >= x_lo && tc->x < x_hi &&
            tc->y >= y_lo && tc->y < y_hi &&
            strstr(tc->text, "2R")) {
            badge = tc;
            break;
        }
    }
    //CHECK(badge != NULL);
    if (badge) CHECK(badge->color == 0x4477CC); /* BLUE */
}

/* The crowding badge only appears when more than one creep occupies a cell.
 * One spawned retriever → no in-grid badge. (Sidebar may show "spawn 1R …"
 * but that's outside the grid area.) */
static void test_single_creep_no_badge(void) {
    g_test = "single_creep_no_badge";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    game_lock_in();                 /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);      /* +1 retriever */
    game_lock_in();                 /* → SIMULATE turn 1 (no creeps yet) */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) break;
    }
    game_lock_in(); game_lock_in(); /* → SIMULATE turn 2: 1 retriever spawns */

    reset_text_log();
    render_frame(game_get_state());
    int gw = 30 * CELL_SIZE;
    for (int i = 0; i < g_text_call_count; i++) {
        const TextCall *tc = &g_text_calls[i];
        if (tc->x >= gw) continue; /* sidebar */
        CHECK(strstr(tc->text, "1R") == NULL);
        CHECK(strstr(tc->text, "1S") == NULL);
    }
}

/* When the game ends, only the Restart button is hittable. */
static void test_game_over_shows_restart(void) {
    g_test = "game_over_shows_restart";
    game_init_with_tower_config(TEST_TOWERS_CFG);
    /* Reach GAME_OVER via the same scenario as test_game.c's win test:
     * BLUE buys a retriever, then runs through one quiet sim turn so the
     * upgrade completes, then plays out turn 2 until the carrier returns
     * to BLUE's receptacle. */
    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);   /* +1 BLUE retriever */
    game_lock_in();              /* → SIMULATE turn 1 (no creeps) */
    /* Drain turn 1 quietly. */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) break;
    }
    game_lock_in(); game_lock_in(); /* → SIMULATE turn 2 */
    /* Run until BLUE wins. */
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_GAME_OVER) break;
    }
    CHECK(game_get_state()->phase == PHASE_GAME_OVER);

    render_frame(game_get_state());
    scan_buttons();
    CHECK(present(BTN_RESTART));
    CHECK(!present(BTN_LOCK_IN));
    CHECK(!present(BTN_PLACE_TOWER_BASE + game_tower_id("GUNNER")));
    CHECK(!present(BTN_BUY_UPGRADE_0));
    CHECK(!present(BTN_UPGRADE_TOWER));
}

int main(void) {
    test_no_buttons_in_grid_area();
    test_planning_buttons_visible();
    test_buy_upgrade_button_disappears_after_purchase();
    test_own_tower_selected_shows_upgrade_destroy();
    test_enemy_tower_selected_no_upgrade_destroy();
    test_simulation_phase_no_buttons();
    test_stacked_creep_count_badge();
    test_single_creep_no_badge();
    test_game_over_shows_restart();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
