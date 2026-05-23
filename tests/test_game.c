#include "game/game.h"
#include "test_fixtures.h"
#include <stdio.h>
#include <string.h>

/* Consumer-driven tests for game-layer behaviors not already covered by
 * test_pathing.c. Each test drives the public game_* API and inspects state
 * via game_get_state(). No internal helpers are touched directly. Add new
 * tests here as game features land. */

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

/* ── helpers ─────────────────────────────────────────────────────────── */

static void step_ticks(int n) {
    for (int i = 0; i < n * SIM_FRAMES_PER_TICK; i++) game_frame();
}

static void enter_sim(void) {
    game_lock_in();
    game_lock_in();
}

/* Advance frames until we're back in the PLAN_RED phase (i.e. the current
 * simulation has wrapped). Bounded so a regression can't hang the test. */
static void run_sim_to_completion(void) {
    for (int i = 0; i < SIM_TICKS_PER_TURN * SIM_FRAMES_PER_TICK + 200; i++) {
        game_frame();
        if (game_get_state()->phase == PHASE_PLAN_RED) return;
    }
}

static const Thing *find_creep(PlayerID owner, CreepType type) {
    const GameState *s = game_get_state();
    for (int i = 0; i < s->thing_count; i++) {
        const Thing *t = &s->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->owner == owner && t->creep.type == type) return t;
    }
    return NULL;
}

static int count_creeps(PlayerID owner, CreepType type) {
    const GameState *s = game_get_state();
    int n = 0;
    for (int i = 0; i < s->thing_count; i++) {
        const Thing *t = &s->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->owner == owner && t->creep.type == type) n++;
    }
    return n;
}

static int tower_id_at(int x, int y) {
    const GameState *s = game_get_state();
    return s->grid[x][y].thing_id;
}

/* ── 2. Phase transitions / turn counter ─────────────────────────────── */

static void test_phase_transitions(void) {
    g_test = "phase_transitions";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();
    CHECK(s->phase == PHASE_PLAN_RED);
    CHECK(game_planning_player() == PLAYER_RED);

    game_lock_in();
    CHECK(s->phase == PHASE_PLAN_BLUE);
    CHECK(game_planning_player() == PLAYER_BLUE);

    game_lock_in();
    CHECK(s->phase == PHASE_SIMULATE);

    /* No creeps spawn (no upgrades purchased), sim ends quickly and we
     * roll over into turn 2 / PLAN_RED. */
    int prev_turn = s->turn;
    run_sim_to_completion();
    CHECK(s->phase == PHASE_PLAN_RED);
    CHECK(s->turn == prev_turn + 1);
    /* Base income (+0/turn) applied to both players at sim end. */
    CHECK(s->players[PLAYER_RED].resources == 100);
    CHECK(s->players[PLAYER_BLUE].resources == 100);
}

/* ── 3. Tower placement: cost, attributes, intent clearing ──────────── */

static void test_tower_placement_basics(void) {
    g_test = "tower_placement_basics";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    game_set_placement(game_tower_id("GUNNER"));
    CHECK(s->placement_intent == game_tower_id("GUNNER"));

    game_grid_click(6, 4);
    /* placed: cost deducted, intent cleared, cell occupied. */
    CHECK(s->players[PLAYER_RED].resources == 100 - game_tower_cost(game_tower_id("GUNNER")));
    CHECK(s->placement_intent == -1);
    int id = tower_id_at(6, 4);
    CHECK(id >= 0);
    CHECK(s->things[id].tag == THING_TOWER);
    CHECK(s->things[id].owner == PLAYER_RED);
    CHECK(s->things[id].alive == 1);
    CHECK(s->things[id].hp == s->things[id].max_hp);
    CHECK(s->things[id].tower.type == game_tower_id("GUNNER"));
    CHECK(s->things[id].tower.level == 1);
    /* Selected cursor moves to placed tower. */
    CHECK(s->selected_x == 6 && s->selected_y == 4);
}

/* ── 4. Tower placement: zone restrictions ──────────────────────────── */

static void test_placement_zone_restrictions(void) {
    g_test = "placement_zone_restrictions";
    /* Pins the map too: this test asserts that a specific neutral-zone cell
     * (15,10) is placeable, which only holds for the standard layout. The
     * shipped data/map.cfg may diverge (e.g. add debris in the neutral
     * strip), so use TEST_MAP_CFG instead. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* RED cannot place in BLUE zone (x >= 20). placement_intent stays set on
     * a failed click — re-calling game_set_placement with the same type would
     * TOGGLE it off (see game_set_placement), so the follow-up click reuses
     * the still-active intent. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(25, 10);
    CHECK(tower_id_at(25, 10) == -1);
    CHECK(s->players[PLAYER_RED].resources == 100);

    /* RED can place in NEUTRAL zone (10 <= x < 20). */
    game_grid_click(15, 10);
    CHECK(tower_id_at(15, 10) >= 0);

    /* BLUE's turn. lock_in clears placement_intent so we re-set it. BLUE
     * cannot place in RED zone (x < 10). */
    game_lock_in();
    CHECK(s->phase == PHASE_PLAN_BLUE);
    CHECK(s->placement_intent == -1);
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(3, 10);
    CHECK(tower_id_at(3, 10) == -1);
    CHECK(s->players[PLAYER_BLUE].resources == 100);
}

/* ── 5. Placement rejected when broke / cell occupied ───────────────── */

static void test_placement_insufficient_resources(void) {
    g_test = "placement_insufficient_resources";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* Cost 80 R tower can fit once (100→20), the second placement fails. */
    game_set_placement(game_tower_id("RESOURCE"));
    game_grid_click(5, 6);
    CHECK(tower_id_at(5, 6) >= 0);
    CHECK(s->players[PLAYER_RED].resources == 20);

    game_set_placement(game_tower_id("RESOURCE"));
    game_grid_click(6, 6);
    CHECK(tower_id_at(6, 6) == -1);
    CHECK(s->players[PLAYER_RED].resources == 20);

    /* Same cell re-click while intent is set: cell already occupied, rejected. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(5, 6);
    /* placement_intent stays set since try_place returns early on invalid cell;
     * the original tower is still the only one at (5,6). */
    int id = tower_id_at(5, 6);
    CHECK(id >= 0);
    CHECK(s->things[id].tower.type == game_tower_id("RESOURCE"));
}

/* ── 6. Tower upgrade ───────────────────────────────────────────────── */

static void test_tower_upgrade(void) {
    g_test = "tower_upgrade";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* Gunner has build_turns=0 so it's eligible to upgrade immediately. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    int id = tower_id_at(6, 4);
    CHECK(id >= 0);
    int res_before   = s->players[PLAYER_RED].resources;
    int up_cost      = game_tower_upgrade_cost(game_tower_id("GUNNER"), 1);

    /* selected_x/y is on the tower already (set by placement). */
    game_upgrade_selected();
    CHECK(s->things[id].tower.level == 2);
    CHECK(s->things[id].tower.build_turns == 1);
    /* Level 2 GUNNER has max HP 70 in data/towers.cfg (vs 50 at level 1). */
    CHECK(s->things[id].max_hp == 70);
    CHECK(s->things[id].hp == s->things[id].max_hp);
    CHECK(s->players[PLAYER_RED].resources == res_before - up_cost);

    /* Already at max level → upgrade is a no-op. */
    int res_after = s->players[PLAYER_RED].resources;
    game_upgrade_selected();
    CHECK(s->things[id].tower.level == 2);
    CHECK(s->players[PLAYER_RED].resources == res_after);
}

static void test_tower_upgrade_rejects_enemy(void) {
    g_test = "tower_upgrade_rejects_enemy";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* RED places a gunner. Switch to BLUE planning and try to upgrade it. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    int id = tower_id_at(6, 4);
    int level_before = s->things[id].tower.level;
    int res_before   = s->players[PLAYER_BLUE].resources;

    game_lock_in(); /* → PLAN_BLUE */
    game_grid_click(6, 4); /* select RED's tower */
    game_upgrade_selected();
    CHECK(s->things[id].tower.level == level_before);
    CHECK(s->players[PLAYER_BLUE].resources == res_before);
}

/* ── 7. Tower destroy ───────────────────────────────────────────────── */

static void test_tower_destroy(void) {
    g_test = "tower_destroy";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    int id = tower_id_at(6, 4);
    CHECK(id >= 0);

    game_destroy_selected();
    CHECK(tower_id_at(6, 4) == -1);
    CHECK(s->things[id].alive == 0);
    CHECK(s->things[id].tag == THING_NONE);
}

static void test_tower_destroy_rejects_enemy(void) {
    g_test = "tower_destroy_rejects_enemy";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    int id = tower_id_at(6, 4);

    game_lock_in(); /* → PLAN_BLUE */
    game_grid_click(6, 4);
    game_destroy_selected();
    CHECK(tower_id_at(6, 4) == id);  /* still RED's tower */
    CHECK(s->things[id].alive == 1);
}

/* ── 8. Creep upgrade purchase + research countdown ──────────────────── */

static void test_creep_upgrade_purchase_and_research(void) {
    g_test = "creep_upgrade_purchase_and_research";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    int cost  = game_creep_upgrade_cost(2);            /* +2 Retrievers */
    int turns = game_creep_upgrade_research_turns(2);
    CHECK(turns == 2);
    game_buy_creep_upgrade(2);
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].purchased == 1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].turns_remaining == 2);
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].completed == 0);
    CHECK(s->players[PLAYER_RED].resources == 100 - cost);

    /* Re-purchase is a no-op. */
    int res = s->players[PLAYER_RED].resources;
    game_buy_creep_upgrade(2);
    CHECK(s->players[PLAYER_RED].resources == res);

    /* One quiet sim turn → turns_remaining decrements. */
    enter_sim();
    run_sim_to_completion();
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].turns_remaining == 1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].completed == 0);

    /* Another quiet sim turn → completed. */
    enter_sim();
    run_sim_to_completion();
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].turns_remaining == 0);
    CHECK(s->players[PLAYER_RED].creep_upgrades[2].completed == 1);
}

/* ── 8b. research_turns=0 completes at purchase + spawns same-turn ────── */

static void test_zero_research_turns_spawns_same_turn(void) {
    g_test = "zero_research_turns_spawns_same_turn";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* Slot 4 is the research_turns=0 fixture. */
    CHECK(game_creep_upgrade_research_turns(4) == 0);
    game_buy_creep_upgrade(4);
    /* Completed flips to 1 at purchase — without the fix, this would stay 0
     * forever because the end-of-sim decrement only fires when
     * turns_remaining > 0. */
    CHECK(s->players[PLAYER_RED].creep_upgrades[4].purchased == 1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[4].completed == 1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[4].turns_remaining == 0);

    /* Enter sim THIS turn — creeps must enter the spawn queue at the start
     * of simulation, matching the tower build_turns=0 same-turn-active
     * semantic. One sim tick later the queue head pops onto the map. */
    enter_sim();
    step_ticks(1);
    CHECK(count_creeps(PLAYER_RED, game_creep_type_id("RETRIEVER")) == 1);
}

/* ── 8c. Upgrade with unmet `requires` can't be purchased; once the
 *       prerequisite completes, the buy goes through. ───────────────── */

static void test_creep_upgrade_requires_gating(void) {
    g_test = "creep_upgrade_requires_gating";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_REQUIRES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* Slot 1 (LOCKED) requires slot 0 (GATE). Catalog exposes the link. */
    CHECK(game_creep_upgrade_requires(0) == -1);
    CHECK(game_creep_upgrade_requires(1) == 0);

    /* Attempting to buy the locked upgrade before the gate is completed
     * is a no-op: no resource debit, no purchased flag. */
    int res_before = s->players[PLAYER_RED].resources;
    game_buy_creep_upgrade(1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[1].purchased == 0);
    CHECK(s->players[PLAYER_RED].resources == res_before);

    /* Complete the gate (research_turns=0 → completed immediately). */
    game_buy_creep_upgrade(0);
    CHECK(s->players[PLAYER_RED].creep_upgrades[0].completed == 1);

    /* Now the locked upgrade is purchasable. */
    game_buy_creep_upgrade(1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[1].purchased == 1);
}

/* ── 9. Creep spawn count reflects completed upgrades ───────────────── */

static void test_completed_upgrade_spawns_retriever(void) {
    g_test = "completed_upgrade_spawns_retriever";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    /* No upgrades complete → turn 1 sim has no creeps. */
    enter_sim();
    CHECK(find_creep(PLAYER_RED, game_creep_type_id("RETRIEVER")) == NULL);
    run_sim_to_completion();

    /* Turn 2: buy +1 retriever, run sim. Upgrade research is 1 turn so
     * the in-progress sim is still creepless, but the next one will spawn. */
    game_buy_creep_upgrade(0);
    enter_sim();
    /* sim turn 2: upgrade not yet completed, no creeps spawn. */
    CHECK(count_creeps(PLAYER_RED, game_creep_type_id("RETRIEVER")) == 0);
    run_sim_to_completion();

    /* Turn 3: upgrade is complete → 1 retriever enters the spawn queue and
     * appears on the first sim tick. */
    enter_sim();
    step_ticks(1);
    CHECK(count_creeps(PLAYER_RED, game_creep_type_id("RETRIEVER")) == 1);
    const Thing *r = find_creep(PLAYER_RED, game_creep_type_id("RETRIEVER"));
    CHECK(r != NULL);
    if (r) {
        CHECK(r->creep.has_flag == 0);
        CHECK(r->hp > 0 && r->hp == r->max_hp);
    }
}

/* ── 10. Resource tower contributes to income ───────────────────────── */

static void test_resource_tower_income(void) {
    g_test = "resource_tower_income";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();
    /* game_tower_id("RESOURCE"): build_turns=3, +10/turn when built. */
    game_set_placement(game_tower_id("RESOURCE"));
    game_grid_click(5, 10);
    int id = tower_id_at(5, 10);
    CHECK(id >= 0);

    /* Turn 1: build_turns 3→2 — still building, income stays at base 20. */
    enter_sim();
    run_sim_to_completion();
    CHECK(s->things[id].tower.build_turns == 2);
    CHECK(s->players[PLAYER_RED].income_per_turn == 0);

    /* Turn 2: 2→1 — still building. */
    enter_sim();
    run_sim_to_completion();
    CHECK(s->things[id].tower.build_turns == 1);
    CHECK(s->players[PLAYER_RED].income_per_turn == 0);

    /* Turn 3: 1→0. end_simulation decrements first, then recomputes income,
     * so the recompute already sees build_turns==0 and adds the +10. */
    enter_sim();
    run_sim_to_completion();
    CHECK(s->things[id].tower.build_turns == 0);
    CHECK(s->players[PLAYER_RED].income_per_turn == 10);

    /* Subsequent turns: income stays at 30, resources tick up by 30. */
    int res_before = s->players[PLAYER_RED].resources;
    enter_sim();
    run_sim_to_completion();
    CHECK(s->players[PLAYER_RED].income_per_turn == 10);
    CHECK(s->players[PLAYER_RED].resources == res_before + 10);
}

/* ── 11. Gunner damages a creep in range ─────────────────────────────── */

static void test_gunner_damages_creep(void) {
    g_test = "gunner_damages_creep";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* RED places gunner at (4,14): RED zone, off BLUE's y=15 path, range 3
     * reaches the BLUE retriever as it walks past y=15 toward (4,15). */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(4, 14);
    CHECK(tower_id_at(4, 14) >= 0);

    game_lock_in(); /* → PLAN_BLUE */
    game_buy_creep_upgrade(0); /* BLUE +1 retriever */
    game_lock_in(); /* → SIMULATE turn 1 (no creep yet) */
    run_sim_to_completion();

    /* Turn 2: BLUE retriever spawns at (29,10) and walks west to (4,10),
     * then south toward (4,15). With (4,14) blocked by RED's gunner it
     * detours via (3,14)/(5,14) — either way the creep ends up well within
     * the gunner's range-3 envelope. ~32 ticks covers the worst case. */
    enter_sim();
    step_ticks(32);
    const Thing *r = find_creep(PLAYER_BLUE, game_creep_type_id("RETRIEVER"));
    /* The creep either has been damaged or has already died. */
    if (r) {
        CHECK(r->hp < r->max_hp);
    } else {
        /* No surviving retriever — the gunner killed it. */
        CHECK(1);
    }
    /* The gunner emits a beam ttl when it shoots; this is a smoke test that
     * an attack actually fired during the run. */
    int gid = tower_id_at(4, 14);
    CHECK(gid >= 0);
    CHECK(s->things[gid].last_target_x != 0 || s->things[gid].last_target_y != 0);
}

/* ── 11b. Crit replaces dmg on a successful roll ────────────────────── */

/* CRITTER is configured crit_chance=100 + crit_dmg=999 against retriever
 * hp=20 — every shot should one-shot the creep. A plain GUNNER at the same
 * spot with dmg=10 needs 2 shots to kill, so the visible difference between
 * "dmg" and "crit_dmg" is that the creep dies before the second tick. */
static void test_tower_crit_uses_crit_dmg(void) {
    g_test = "tower_crit_uses_crit_dmg";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);

    game_set_placement(game_tower_id("CRITTER"));
    game_grid_click(4, 14);
    CHECK(tower_id_at(4, 14) >= 0);

    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);   /* BLUE +1 retriever, 1 turn research */
    game_lock_in();              /* → SIMULATE turn 1 (no creeps) */
    run_sim_to_completion();
    enter_sim();                 /* SIMULATE turn 2 */

    /* From spawn (29,10) the retriever walks west then south. ~32 ticks
     * covers it reaching the kill zone around (4,14). The moment it enters
     * range a single shot at 999 dmg must drop the 20-HP creep. */
    int saw_kill_in_one_tick = 0;
    int prev_hp = -1;
    for (int i = 0; i < 35; i++) {
        step_ticks(1);
        const Thing *r = find_creep(PLAYER_BLUE, game_creep_type_id("RETRIEVER"));
        if (!r) {
            /* Creep gone. If it took damage before that, the previous-tick HP
             * was > 0 and the only way it died was one >= 20 dmg hit — i.e. a
             * crit. (A non-crit 1-dmg shot couldn't kill from any prev_hp.) */
            if (prev_hp > 1) saw_kill_in_one_tick = 1;
            break;
        }
        prev_hp = r->hp;
    }
    CHECK(saw_kill_in_one_tick);
}

/* ── 11c. Tower targets the creep closest to the defended flag ──────── */

/* Towers no longer pick the creep closest to themselves — they pick the
 * in-range enemy creep with the smallest Manhattan distance to the tower
 * owner's own flag (the one the enemy is trying to steal). Setup snaps
 * two BLUE retrievers onto cells where the two metrics disagree:
 *   A at (4,14) — tower dist 2, flag dist 1
 *   B at (4,13) — tower dist 1, flag dist 2
 * The old closest-to-tower rule would shoot B; the new rule shoots A. */
static void test_tower_targets_creep_closest_to_flag(void) {
    g_test = "tower_targets_creep_closest_to_flag";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* RED places GUNNER at (5,13). RED's flag (the defended flag) is at
     * (4,15). Range 3, dmg 10, build_turns 0 so it's hot immediately. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(5, 13);
    int tid = tower_id_at(5, 13);
    CHECK(tid >= 0);

    /* BLUE buys +2 retrievers (slot 2, 2-turn research) so two BLUE creeps
     * land on the field — we'll snap them onto chosen cells below. Two
     * turns of research means the creeps spawn on sim turn 3. */
    game_lock_in();              /* PLAN_RED → PLAN_BLUE */
    game_buy_creep_upgrade(2);   /* RETRIEVER_2X */
    game_lock_in();              /* → SIMULATE turn 1 (research running) */
    run_sim_to_completion();
    enter_sim();                 /* → SIMULATE turn 2 (research finishes at end) */
    run_sim_to_completion();
    enter_sim();                 /* → SIMULATE turn 3, queue has 2 retrievers */
    step_ticks(2);               /* one per tick → both retrievers on the field */

    int rt = game_creep_type_id("RETRIEVER");
    int idx_a = -1, idx_b = -1;
    for (int i = 0; i < s->thing_count; i++) {
        const Thing *t = &s->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->owner != PLAYER_BLUE || t->creep.type != rt) continue;
        if (idx_a < 0) idx_a = i; else { idx_b = i; break; }
    }
    CHECK(idx_a >= 0 && idx_b >= 0);

    GameState *mut = (GameState *)s;
    mut->things[idx_a].x = 4; mut->things[idx_a].y = 14;
    mut->things[idx_b].x = 4; mut->things[idx_b].y = 13;
    /* Freeze movement for this one tick so positions stay fixed when the
     * tower picks its target. */
    mut->things[idx_a].creep.slow_ticks = 1;
    mut->things[idx_b].creep.slow_ticks = 1;
    mut->things[tid].tower.cooldown = 0;   /* ensure the tower fires this tick */
    int hp_a_before = mut->things[idx_a].hp;
    int hp_b_before = mut->things[idx_b].hp;

    step_ticks(1);

    CHECK(mut->things[idx_a].hp == hp_a_before - 10);  /* flag-closer creep hit */
    CHECK(mut->things[idx_b].hp == hp_b_before);       /* tower-closer creep spared */
}

/* ── 12. Slammer applies slow_ticks (AoE + slow effect) ──────────────── */

static void test_slammer_slows_creep(void) {
    g_test = "slammer_slows_creep";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* Slammer has build_turns=2 so we burn 2 turns before its effect lands.
     * Place the slammer first, then BLUE only buys the retriever upgrade
     * on a later turn so the creep arrives after the slammer is built. */
    game_set_placement(game_tower_id("SLAMMER"));
    game_grid_click(4, 14);
    int sid = tower_id_at(4, 14);
    CHECK(sid >= 0);

    enter_sim();              /* sim turn 1: slammer build_turns 2→1, no creeps */
    run_sim_to_completion();
    enter_sim();              /* sim turn 2: slammer build_turns 1→0, still no creeps */
    run_sim_to_completion();
    CHECK(s->things[sid].tower.build_turns == 0);

    /* Now turn 3: BLUE buys +1 retriever (1 turn research). */
    game_lock_in();           /* PLAN_RED → PLAN_BLUE */
    game_buy_creep_upgrade(0);
    game_lock_in();           /* → SIMULATE turn 3 (upgrade not yet complete) */
    run_sim_to_completion();
    /* Turn 4: retriever spawns at (29,10), walks west then south toward
     * (4,15) — the slammer at (4,14) is in range as the creep approaches.
     * ~32 ticks is enough to reach the kill zone even with a detour. */
    enter_sim();
    int saw_slow = 0;
    int saw_hp_drop = 0;
    for (int i = 0; i < 32; i++) {
        step_ticks(1);
        const Thing *r = find_creep(PLAYER_BLUE, game_creep_type_id("RETRIEVER"));
        if (!r) break;
        if (r->creep.slow_ticks > 0) saw_slow = 1;
        if (r->hp < r->max_hp)       saw_hp_drop = 1;
    }
    CHECK(saw_slow);
    CHECK(saw_hp_drop);
}

/* ── 13. Siege creep attacks adjacent enemy tower ────────────────────── */

static void test_siege_attacks_tower(void) {
    g_test = "siege_attacks_tower";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* RED places BLOCKER at (5,15) — on BLUE's path, RED zone. BLUE siege
     * creeps walking left will be adjacent to the blocker (at (6,15) etc.)
     * and attack it. */
    game_set_placement(game_tower_id("BLOCKER"));
    game_grid_click(5, 15);
    int bid = tower_id_at(5, 15);
    CHECK(bid >= 0);
    int hp_before = s->things[bid].hp;

    game_lock_in();                /* → PLAN_BLUE */
    game_buy_creep_upgrade(1);     /* BLUE +2 siege, research_turns=1 */
    game_lock_in();                /* → SIMULATE turn 1 */
    run_sim_to_completion();
    /* Turn 2: 2 BLUE siege creeps queue up. They spawn one per tick at
     * (25,15), forming a line, and walk left toward the blocker. */
    enter_sim();
    step_ticks(2);
    CHECK(count_creeps(PLAYER_BLUE, game_creep_type_id("SIEGE")) == 2);

    /* Step enough ticks for the siege creeps to approach. ~20 ticks of
     * travel + a few melee hits before the blocker dies. */
    for (int i = 0; i < 40; i++) {
        step_ticks(1);
        if (tower_id_at(5, 15) == -1) break;
        if (s->things[bid].hp < hp_before) break;
    }
    /* Either HP dropped or the tower was destroyed outright. */
    CHECK(s->things[bid].hp < hp_before || s->things[bid].alive == 0);
}

/* ── 14. Flag pickup ────────────────────────────────────────────────── */

static void test_flag_pickup(void) {
    g_test = "flag_pickup";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* BLUE buys +1 retriever; turn 1 quiet; turn 2 retriever walks to RED
     * flag at (4,15) and picks it up. */
    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);
    game_lock_in();              /* → SIMULATE turn 1 */
    run_sim_to_completion();
    enter_sim();                 /* SIMULATE turn 2 */

    int picked = 0;
    /* From spawn (29,10), the retriever takes ~30 ticks to reach the flag at
     * (4,15): 25 steps west to (4,10), then 5 south. */
    for (int i = 0; i < 35; i++) {
        step_ticks(1);
        const Thing *r = find_creep(PLAYER_BLUE, game_creep_type_id("RETRIEVER"));
        if (!r) break;
        if (r->creep.has_flag) {
            picked = 1;
            CHECK(r->x == 4 && r->y == 15);
            CHECK(s->flags[PLAYER_RED].carried_by >= 0);
            CHECK(s->flags[PLAYER_RED].at_home == 0);
            break;
        }
    }
    CHECK(picked);
}

/* ── 15. Win condition: carrier reaches own receptacle ───────────────── */

static void test_win_condition(void) {
    g_test = "win_condition";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);
    game_lock_in();              /* → SIMULATE turn 1 */
    run_sim_to_completion();
    enter_sim();                 /* SIMULATE turn 2 */

    /* BLUE retriever walks 21 cells to (4,15), picks up flag, walks 21 cells
     * back to receptacle (25,15) → win. Run plenty of ticks but bounded. */
    for (int i = 0; i < 60; i++) {
        step_ticks(1);
        if (s->phase == PHASE_GAME_OVER) break;
    }
    CHECK(s->phase == PHASE_GAME_OVER);
    CHECK(s->winner == PLAYER_BLUE);
}

/* ── 16. Flag drop on carrier death ─────────────────────────────────── */

static void test_flag_drop_on_death(void) {
    g_test = "flag_drop_on_death";
    /* Corridor map: BLUE spawn (19,2) walks west along y=2 to RED flag
     * (3,2). The straight-line corridor has no shortest-path ties so
     * pathing wobble doesn't perturb when the carrier enters tower range. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CORRIDOR_CFG);
    const GameState *s = game_get_state();

    /* GUNNER at (3,1), one cell above the flag. Range 3 + cooldown 2 +
     * dmg 10 against the 20-HP retriever produces this timing:
     *   tick 14 — creep at (5,2), Manhattan 3 → fires (HP 20→10), cd=2
     *   tick 15 — creep at (4,2), cd 2→1, no fire
     *   tick 16 — creep at (3,2), pickup, cd 1→0, no fire (still cooling)
     *   tick 17 — creep moved east of flag, cd 0 → fires (HP 10→0)
     *             carrier dies → flag drops at its current cell.
     * The two shots are calibrated on opposite sides of pickup so the
     * carrier is alive AT pickup but doomed after, exercising the
     * "drop on carrier death" path (rather than "kill before pickup").
     * (Doubling the gunner — or moving it onto the approach — would kill
     * pre-pickup and leave the flag at_home.) */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(3, 1);
    CHECK(tower_id_at(3, 1) >= 0);

    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);
    game_lock_in();              /* → SIMULATE turn 1 (no creeps yet) */
    run_sim_to_completion();
    enter_sim();                 /* SIMULATE turn 2 */

    int observed_drop = 0;
    for (int i = 0; i < 30; i++) {
        step_ticks(1);
        /* Check whether the flag is ever in a "dropped" state: not at_home
         * AND not carried. That can only happen if a carrier died. */
        if (!s->flags[PLAYER_RED].at_home && s->flags[PLAYER_RED].carried_by == -1) {
            observed_drop = 1;
            break;
        }
    }
    CHECK(observed_drop);
}

/* ── 17. Combined-behavior creep type: BANANA carries AND attacks ─────── */

/* Drives the "creep behaviors are independent config flags" property end
 * to end. A BANANA creep type sets BOTH can_carry_flag=1 AND
 * melee_damage>0, then we observe a single BANANA creep:
 *   - dealing damage to an adjacent enemy tower (melee_damage path), and
 *   - picking up the enemy flag on contact (can_carry_flag path).
 * Pinned numbers (BANANA hp 30, melee_damage 3, plus GUNNER 2-shot kill in
 * TEST_TOWERS_CFG) live in test_fixtures.h. */
static void test_banana_creep_carries_and_attacks(void) {
    g_test = "banana_creep_carries_and_attacks";
    /* Corridor map: BLUE spawn (19,2) walks west along y=2 to RED flag
     * (3,2). One BLOCKER placed on y=1, adjacent to the corridor, lets
     * the BANANA brush past and melee it on a known tick regardless of
     * pathing wobble. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_BANANA_CFG, TEST_MAP_CORRIDOR_CFG);
    const GameState *s = game_get_state();

    /* BLOCKER at (10,1): tick 9 the BANANA arrives at (10,2) — adjacent —
     * and the melee phase deals melee_damage. The BANANA continues west
     * to the flag at (3,2) and picks it up at tick 16. */
    game_set_placement(game_tower_id("BLOCKER"));
    game_grid_click(10, 1);
    int bid = tower_id_at(10, 1);
    CHECK(bid >= 0);
    int hp_before = s->things[bid].hp;

    game_lock_in();                /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);     /* BLUE: BANANA upgrade, research_turns=1 */
    game_lock_in();                /* → SIMULATE turn 1 (no creeps yet) */
    run_sim_to_completion();
    enter_sim();                   /* SIMULATE turn 2 — BANANA queued */
    step_ticks(1);                 /* queue head pops onto the map */

    int banana_type = game_creep_type_id("BANANA");
    CHECK(count_creeps(PLAYER_BLUE, banana_type) == 1);

    int saw_damage = 0;
    int saw_pickup = 0;
    /* 16 ticks to walk spawn → flag; loop a bit beyond for safety. */
    for (int i = 0; i < 25; i++) {
        step_ticks(1);
        if (s->things[bid].hp < hp_before) saw_damage = 1;
        const Thing *banana = find_creep(PLAYER_BLUE, banana_type);
        if (banana && banana->creep.has_flag) saw_pickup = 1;
        if (saw_damage && saw_pickup) break;
    }
    CHECK(saw_damage);
    CHECK(saw_pickup);
}

/* ── 17b. Spawn ordering: queue sorted by creep type's spawn_order ────── */

/* TEST_CREEP_SPAWN_ORDER_CFG declares RETRIEVER first (spawn_order 2) and
 * SIEGE second (spawn_order 1). Both upgrades are instant. We buy
 * RETRIEVER first (slot 0) then SIEGE (slot 1), so without sorting the
 * queue order would be [R, S] (declaration-order push). Sorting by
 * spawn_order produces [S, R], so tick 1 spawns SIEGE before tick 2's
 * RETRIEVER — proving the sort key actually controls the order. */
static void test_spawn_order_controls_queue(void) {
    g_test = "spawn_order_controls_queue";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_SPAWN_ORDER_CFG, TEST_MAP_CFG);
    game_buy_creep_upgrade(0); /* RETRIEVER_1 (spawn_order 2) */
    game_buy_creep_upgrade(1); /* SIEGE_1     (spawn_order 1) */
    enter_sim();

    int rt = game_creep_type_id("RETRIEVER");
    int sg = game_creep_type_id("SIEGE");

    /* After the first sim tick exactly one creep — SIEGE — is on the map. */
    step_ticks(1);
    CHECK(count_creeps(PLAYER_RED, sg) == 1);
    CHECK(count_creeps(PLAYER_RED, rt) == 0);

    /* After the second tick the RETRIEVER joins. */
    step_ticks(1);
    CHECK(count_creeps(PLAYER_RED, sg) == 1);
    CHECK(count_creeps(PLAYER_RED, rt) == 1);
}

/* ── 17c. Spawn-in-a-line: one creep per tick, queue drains over time ──── */

/* When several creeps are queued, they pop one-per-tick rather than all
 * appearing on the spawn cell at tick 0. Setup: 2 SIEGE creeps (the
 * SIEGE_2 upgrade) with 1 turn of research, then burn that turn so the
 * upgrade is live for the following sim. */
static void test_creeps_spawn_one_per_tick(void) {
    g_test = "creeps_spawn_one_per_tick";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(1);   /* BLUE SIEGE_2 (+2 siege, 1 turn research) */
    game_lock_in();              /* → SIMULATE turn 1 (research running) */
    run_sim_to_completion();
    enter_sim();                 /* → SIMULATE turn 2: 2 siege queued */

    int sg = game_creep_type_id("SIEGE");
    /* Tick 0 invocation pops 1 siege; tick 1 pops the second. No tick is
     * allowed to spawn both. */
    CHECK(count_creeps(PLAYER_BLUE, sg) == 0);
    step_ticks(1);
    CHECK(count_creeps(PLAYER_BLUE, sg) == 1);
    step_ticks(1);
    CHECK(count_creeps(PLAYER_BLUE, sg) == 2);

    /* And once the queue is drained, no further creeps appear. */
    step_ticks(2);
    CHECK(count_creeps(PLAYER_BLUE, sg) == 2);
}

/* ── 17d. Merge semantic: later upgrades overlay only set fields ──────── */

/* The fixture TEST_CREEP_MERGE_CFG sets up two upgrades targeting the
 * same creep type: slot 0 establishes the full profile
 * (count=1, hp=25, melee_damage=5), slot 1 sets ONLY hp=50. With both
 * completed, the merged profile must be (count=1 from slot 0,
 * hp=50 from slot 1, melee_damage=5 from slot 0) — proving that
 * unspecified fields inherit and specified fields override per-field. */
static const char TEST_CREEP_MERGE_CFG[] =
    "creep RUNNER\n"
    "upgrade BASE\n"
    "  cost            10\n"
    "  research_turns  0\n"
    "  creep           RUNNER\n"
    "  count           1\n"
    "  code            R\n"
    "  hp              25\n"
    "  melee_damage    5\n"
    "  spawn_order     2\n"
    "  description     base\n"
    "upgrade BUFF_HP\n"
    "  cost            10\n"
    "  research_turns  0\n"
    "  creep           RUNNER\n"
    "  hp              50\n"
    "  description     +HP only\n";

static void test_merge_semantic_field_overlay(void) {
    g_test = "merge_semantic_field_overlay";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_MERGE_CFG, TEST_MAP_CFG);
    int runner = game_creep_type_id("RUNNER");

    /* Slot 0 alone: the full profile applies as-is. */
    game_buy_creep_upgrade(0);
    enter_sim();
    CHECK(game_creep_is_active(PLAYER_RED, runner));
    CHECK(game_creep_active_count(PLAYER_RED, runner)        == 1);
    CHECK(game_creep_active_hp(PLAYER_RED, runner)           == 25);
    CHECK(game_creep_active_melee_damage(PLAYER_RED, runner) == 5);
    CHECK(game_creep_active_code(PLAYER_RED, runner)         == 'R');
    CHECK(game_creep_active_spawn_order(PLAYER_RED, runner)  == 2);
    run_sim_to_completion();

    /* Add slot 1 (hp-only buff). Next sim's merged profile must show
     * hp swapped to 50 while count / melee_damage / code / spawn_order
     * carry over from slot 0 — pure per-field overlay. */
    game_buy_creep_upgrade(1);
    enter_sim();
    CHECK(game_creep_active_hp(PLAYER_RED, runner)           == 50);
    CHECK(game_creep_active_count(PLAYER_RED, runner)        == 1);
    CHECK(game_creep_active_melee_damage(PLAYER_RED, runner) == 5);
    CHECK(game_creep_active_code(PLAYER_RED, runner)         == 'R');
    CHECK(game_creep_active_spawn_order(PLAYER_RED, runner)  == 2);
}

/* ── 18. Placement validity ──────────────────────────────────────────── */

/* placement_valid rejects the obvious illegal cells (receptacle, at-home
 * flag) via explicit guards, and consults paths_valid for everything else.
 * paths_valid in turn requires the full loop (spawn → enemy flag → own
 * receptacle) to remain walkable. A normal placement on a line cell should
 * succeed when an alternate BFS route exists. */
static void test_placement_validity(void) {
    g_test = "placement_validity";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    const GameState *s = game_get_state();

    /* Receptacle cell rejected. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(s->receptacle_x[PLAYER_RED], s->receptacle_y[PLAYER_RED]);
    CHECK(s->grid[s->receptacle_x[PLAYER_RED]][s->receptacle_y[PLAYER_RED]].thing_id == -1);

    /* At-home flag cell rejected. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(s->flags[PLAYER_RED].x, s->flags[PLAYER_RED].y);
    CHECK(s->grid[s->flags[PLAYER_RED].x][s->flags[PLAYER_RED].y].thing_id == -1);

    /* A line cell with alternate routes around it succeeds. (10,10) sits
     * on RED's outbound y=10 corridor; the rest of the grid is wide open. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(10, 10);
    CHECK(s->grid[10][10].thing_id != -1);
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(void) {
    test_phase_transitions();
    test_tower_placement_basics();
    test_placement_zone_restrictions();
    test_placement_insufficient_resources();
    test_tower_upgrade();
    test_tower_upgrade_rejects_enemy();
    test_tower_destroy();
    test_tower_destroy_rejects_enemy();
    test_creep_upgrade_purchase_and_research();
    test_zero_research_turns_spawns_same_turn();
    test_creep_upgrade_requires_gating();
    test_completed_upgrade_spawns_retriever();
    test_resource_tower_income();
    test_gunner_damages_creep();
    test_tower_targets_creep_closest_to_flag();
    test_tower_crit_uses_crit_dmg();
    test_slammer_slows_creep();
    test_siege_attacks_tower();
    test_flag_pickup();
    test_win_condition();
    test_flag_drop_on_death();
    test_banana_creep_carries_and_attacks();
    test_spawn_order_controls_queue();
    test_creeps_spawn_one_per_tick();
    test_merge_semantic_field_overlay();
    test_placement_validity();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
