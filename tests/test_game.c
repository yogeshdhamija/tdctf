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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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

/* ── 9. Creep spawn count reflects completed upgrades ───────────────── */

static void test_completed_upgrade_spawns_retriever(void) {
    g_test = "completed_upgrade_spawns_retriever";
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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

    /* Turn 3: upgrade is complete → 1 retriever spawns at start_simulation. */
    enter_sim();
    CHECK(count_creeps(PLAYER_RED, game_creep_type_id("RETRIEVER")) == 1);
    const Thing *r = find_creep(PLAYER_RED, game_creep_type_id("RETRIEVER"));
    CHECK(r != NULL);
    if (r) {
        CHECK(r->x == 10 && r->y == 9); /* RED spawn cell */
        CHECK(r->creep.has_flag == 0);
        CHECK(r->hp > 0 && r->hp == r->max_hp);
    }
}

/* ── 10. Resource tower contributes to income ───────────────────────── */

static void test_resource_tower_income(void) {
    g_test = "resource_tower_income";
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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

/* ── 12. Slammer applies slow_ticks (AoE + slow effect) ──────────────── */

static void test_slammer_slows_creep(void) {
    g_test = "slammer_slows_creep";
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    /* Turn 2: 2 BLUE siege creeps spawn at (25,15). They walk left and
     * eventually adjacent siege creeps will hit the blocker. */
    enter_sim();
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
    const GameState *s = game_get_state();

    /* RED places one gunner at (5,15) — the first cell of BLUE's return
     * half. Damage profile: gunner = 10/shot @ cooldown 2; retriever HP =
     * 20. As the BLUE retriever approaches the flag at (4,15), the gunner
     * fires once at range 3 (HP 20→10). The retriever picks up the flag at
     * (4,15), then has to detour around the gunner cell — its first detour
     * step lands within range again and the second shot kills it carrying
     * the flag, leaving a drop at (4,16). (Two gunners would kill before
     * pickup, leaving the flag at_home.) */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(5, 15);
    CHECK(tower_id_at(5, 15) >= 0);

    game_lock_in();              /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);
    game_lock_in();              /* → SIMULATE turn 1 (no creeps yet) */
    run_sim_to_completion();
    enter_sim();                 /* SIMULATE turn 2 */

    /* Drop lands at ~tick 32 of the sim phase (25 ticks west + 5 south +
     * 1 detour step). Wait a bit longer to be safe. */
    int observed_drop = 0;
    for (int i = 0; i < 40; i++) {
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
    /* TEST_CREEP_BANANA_CFG declares a single upgrade (slot 0) that spawns
     * 1 BANANA per turn after 1 turn of research. */
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_BANANA_CFG);
    const GameState *s = game_get_state();

    /* RED places a BLOCKER at (4,11). BLUE's spawn is (19,11) and RED's
     * flag is at (4,15). BLUE's BANANA walks west along y=11, reaches
     * (5,11) adjacent to the blocker — melee_damage lands — then detours
     * south to (5,15)→(4,15) and picks up the flag. */
    game_set_placement(game_tower_id("BLOCKER"));
    game_grid_click(4, 11);
    int bid = tower_id_at(4, 11);
    CHECK(bid >= 0);
    int hp_before = s->things[bid].hp;

    game_lock_in();                /* → PLAN_BLUE */
    game_buy_creep_upgrade(0);     /* BLUE: BANANA upgrade, research_turns=1 */
    game_lock_in();                /* → SIMULATE turn 1 (no creeps yet) */
    run_sim_to_completion();
    enter_sim();                   /* SIMULATE turn 2 — BANANA spawns */

    int banana_type = game_creep_type_id("BANANA");
    CHECK(count_creeps(PLAYER_BLUE, banana_type) == 1);

    int saw_damage = 0;
    int saw_pickup = 0;
    /* 14 steps west + 4 south + 1 west = ~19 ticks. Run a bit beyond to
     * tolerate variability in BFS tiebreaks. */
    for (int i = 0; i < 30; i++) {
        step_ticks(1);
        if (s->things[bid].hp < hp_before) saw_damage = 1;
        const Thing *banana = find_creep(PLAYER_BLUE, banana_type);
        if (banana && banana->creep.has_flag) saw_pickup = 1;
        if (saw_damage && saw_pickup) break;
    }
    CHECK(saw_damage);
    CHECK(saw_pickup);
}

/* ── 18. Placement validity ──────────────────────────────────────────── */

/* placement_valid rejects the obvious illegal cells (receptacle, at-home
 * flag) via explicit guards, and consults paths_valid for everything else.
 * paths_valid in turn requires the full loop (spawn → enemy flag → own
 * receptacle) to remain walkable. A normal placement on a line cell should
 * succeed when an alternate BFS route exists. */
static void test_placement_validity(void) {
    g_test = "placement_validity";
    game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG);
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
    test_completed_upgrade_spawns_retriever();
    test_resource_tower_income();
    test_gunner_damages_creep();
    test_slammer_slows_creep();
    test_siege_attacks_tower();
    test_flag_pickup();
    test_win_condition();
    test_flag_drop_on_death();
    test_banana_creep_carries_and_attacks();
    test_placement_validity();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
