#include "game/game.h"
#include "test_fixtures.h"
#include <stdio.h>
#include <string.h>

/* Snapshot encode/decode lives at the game layer (no canvas, no platform).
 * Each test drives the public game_* API to set up some state, encodes it,
 * mutates the live state (or reloads with a different catalog), and asserts
 * the snapshot restores what it claims to. */

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

static int tower_id_at(int x, int y) {
    return game_get_state()->grid[x][y].thing_id;
}

/* ── 1. Round-trip preserves turn / phase / resources / towers / flags ── */

static void test_round_trip_basic(void) {
    g_test = "round_trip_basic";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);

    /* Build up some state: place a tower, buy an upgrade, lock in (so the
     * turn counter actually moves through PLAN_RED → PLAN_BLUE). */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    game_buy_creep_upgrade(0);                  /* RETRIEVER_1 */
    int red_res_before  = game_get_state()->players[PLAYER_RED].resources;
    int blue_res_before = game_get_state()->players[PLAYER_BLUE].resources;
    game_lock_in();                             /* → PLAN_BLUE */

    char buf[4096];
    int n = game_snapshot_encode(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(n < (int)sizeof(buf));

    /* Wipe state then reload from the snapshot. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    CHECK(game_snapshot_load(buf) == 0);

    const GameState *s = game_get_state();
    CHECK(s->phase == PHASE_PLAN_BLUE);
    CHECK(s->turn  == 1);
    CHECK(s->players[PLAYER_RED].resources  == red_res_before);
    CHECK(s->players[PLAYER_BLUE].resources == blue_res_before);

    int id = tower_id_at(6, 4);
    CHECK(id >= 0);
    CHECK(s->things[id].tag == THING_TOWER);
    CHECK(s->things[id].owner == PLAYER_RED);
    CHECK(s->things[id].tower.type == game_tower_id("GUNNER"));
    CHECK(s->things[id].tower.level == 1);

    /* The RETRIEVER_1 upgrade we bought is purchased but not yet completed
     * (its research_turns is 1 and no sim has run). */
    int ridx = 0; /* slot 0 = RETRIEVER_1 in TEST_CREEP_UPGRADES_CFG */
    CHECK(s->players[PLAYER_RED].creep_upgrades[ridx].purchased == 1);
    CHECK(s->players[PLAYER_RED].creep_upgrades[ridx].completed == 0);
    CHECK(s->players[PLAYER_RED].creep_upgrades[ridx].turns_remaining == 1);
}

/* ── 2. Tower HP is preserved through the snapshot (not just refilled) ── */

/* If a tower took damage in a previous turn, the snapshot must remember the
 * current HP — not silently restore it to max_hp, which would let players
 * heal by bookmarking. We can't easily damage a tower without running sim,
 * so instead we mutate via the internal grid to simulate prior damage. The
 * value lands in the encoded string and is preserved on reload. */
static void test_round_trip_preserves_tower_hp(void) {
    g_test = "round_trip_preserves_tower_hp";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    game_set_placement(game_tower_id("BLOCKER"));
    game_grid_click(6, 4);
    int id = tower_id_at(6, 4);
    CHECK(id >= 0);
    /* Hack the HP down. The const-ness is a public-API courtesy, not a
     * physical guarantee — game.c owns the only mutable copy and lets the
     * suite reach in. */
    GameState *mut = (GameState *)game_get_state();
    mut->things[id].hp = 37;
    int max_hp = mut->things[id].max_hp;

    char buf[4096];
    CHECK(game_snapshot_encode(buf, sizeof(buf)) > 0);

    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    CHECK(game_snapshot_load(buf) == 0);

    const GameState *s = game_get_state();
    int rid = tower_id_at(6, 4);
    CHECK(rid >= 0);
    CHECK(s->things[rid].hp == 37);
    CHECK(s->things[rid].max_hp == max_hp);  /* derived from current cfg */
}

/* ── 3. Snapshot survives a tower-config edit that removes/renames IDs ─── */

/* If the player snapshots with a GUNNER on the board and later the cfg is
 * edited to remove GUNNER, the load must succeed — the GUNNER reference is
 * silently dropped, everything else still resolves. This is the property
 * that lets the snapshot live in a URL even across cfg edits. */
static const char TEST_TOWERS_CFG_NO_GUNNER[] =
    "tower BLOCKER\n"
    "  code B\n"
    "  name Blocker\n"
    "level BLOCKER 1\n"
    "  cost 20\n"
    "  hp 100\n"
    "  build_turns 1\n"

    "tower SLAMMER\n"
    "  code S\n"
    "  name Slammer\n"
    "level SLAMMER 1\n"
    "  cost 50\n"
    "  hp 50\n"
    "  build_turns 2\n"
    "  dmg 5\n"
    "  range 3\n"
    "  aoe 1\n"
    "  slow 2\n"
    "  cooldown 3\n";

static void test_load_drops_unknown_tower_id(void) {
    g_test = "load_drops_unknown_tower_id";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    /* Place a GUNNER and a BLOCKER. After the cfg edit only BLOCKER remains
     * resolvable. */
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    game_set_placement(game_tower_id("BLOCKER"));
    game_grid_click(7, 4);

    char buf[4096];
    CHECK(game_snapshot_encode(buf, sizeof(buf)) > 0);

    /* Reload with a cfg that doesn't define GUNNER. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG_NO_GUNNER,
                                   TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    CHECK(game_snapshot_load(buf) == 0);

    /* GUNNER cell is empty (the snapshot's GUNNER reference resolved to -1
     * and was silently dropped). BLOCKER still resolves and is in place. */
    CHECK(tower_id_at(6, 4) == -1);
    int bid = tower_id_at(7, 4);
    CHECK(bid >= 0);
    const GameState *s = game_get_state();
    CHECK(s->things[bid].tower.type == game_tower_id("BLOCKER"));
}

/* ── 4. Snapshot survives a creep-upgrade reorder + a removed upgrade ──── */

/* Same property for creep upgrades: a snapshot referencing RETRIEVER_2X by
 * ID still finds it after the cfg has been reordered, and references to
 * removed upgrades are dropped. */
static const char TEST_CREEP_UPGRADES_REORDERED_CFG[] =
    "creep RETRIEVER\n"
    "  code            R\n"
    "  hp              20\n"
    "  can_carry_flag  1\n"

    "creep SIEGE\n"
    "  code            S\n"
    "  hp              40\n"
    "  melee_damage    5\n"

    /* New upgrade inserted at slot 0; RETRIEVER_1 is now at slot 1. */
    "upgrade BRAND_NEW\n"
    "  cost            5\n"
    "  research_turns  1\n"
    "  spawn           RETRIEVER 1\n"
    "  description     +1 New\n"

    "upgrade RETRIEVER_1\n"
    "  cost            30\n"
    "  research_turns  1\n"
    "  spawn           RETRIEVER 1\n"
    "  description     +1 Retriever\n"

    /* SIEGE_2 has been removed. */

    "upgrade RETRIEVER_2X\n"
    "  cost            60\n"
    "  research_turns  2\n"
    "  spawn           RETRIEVER 2\n"
    "  description     +2 Retrievers\n";

static void test_load_survives_upgrade_reorder(void) {
    g_test = "load_survives_upgrade_reorder";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    /* Buy RETRIEVER_1 (slot 0) and SIEGE_2 (slot 1) under the original cfg. */
    game_buy_creep_upgrade(0);
    game_buy_creep_upgrade(1);

    char buf[4096];
    CHECK(game_snapshot_encode(buf, sizeof(buf)) > 0);

    /* Reload under a cfg where RETRIEVER_1 has moved to slot 1 and SIEGE_2
     * has been removed entirely. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG,
                                   TEST_CREEP_UPGRADES_REORDERED_CFG,
                                   TEST_MAP_CFG);
    CHECK(game_snapshot_load(buf) == 0);

    const GameState *s = game_get_state();
    /* RETRIEVER_1 now lives at slot 1 under the new cfg — and its purchased
     * state must have followed the name, not the index. */
    int new_r1 = 1;
    CHECK(s->players[PLAYER_RED].creep_upgrades[new_r1].purchased == 1);
    /* The brand-new slot 0 stayed at defaults (not from the snapshot). */
    CHECK(s->players[PLAYER_RED].creep_upgrades[0].purchased == 0);
}

/* ── 5. Phase round-trip including SIMULATE re-spawns creeps ──────────── */

/* Encoding while phase==SIMULATE must restore back to SIMULATE on load,
 * and start_simulation must run so the deterministic spawn list is
 * recreated from the completed upgrades. */
static void test_simulate_phase_respawns_creeps(void) {
    g_test = "simulate_phase_respawns_creeps";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);

    /* Force-complete RETRIEVER_1 for RED so a creep is guaranteed to spawn
     * when SIMULATE starts. Going through the normal purchase+research path
     * would require running a turn (which itself triggers a SIMULATE) and
     * is what we want to test resuming from, so we set the flags directly. */
    GameState *mut = (GameState *)game_get_state();
    mut->players[PLAYER_RED].creep_upgrades[0].purchased = 1;
    mut->players[PLAYER_RED].creep_upgrades[0].completed = 1;
    mut->players[PLAYER_RED].creep_upgrades[0].turns_remaining = 0;

    /* Get into PHASE_SIMULATE the regular way — both lock-ins. The 2nd
     * one calls start_simulation, which spawns the creep. */
    game_lock_in();
    game_lock_in();
    const GameState *s = game_get_state();
    CHECK(s->phase == PHASE_SIMULATE);

    char buf[4096];
    CHECK(game_snapshot_encode(buf, sizeof(buf)) > 0);

    /* Reload from scratch. */
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    CHECK(game_snapshot_load(buf) == 0);
    CHECK(s->phase == PHASE_SIMULATE);

    /* start_simulation should have run during the load, re-spawning the
     * retriever at RED's spawn point. */
    int rt_id = game_creep_type_id("RETRIEVER");
    int n = 0;
    for (int i = 0; i < s->thing_count; i++) {
        const Thing *t = &s->things[i];
        if (t->tag == THING_CREEP && t->alive && t->owner == PLAYER_RED &&
            t->creep.type == rt_id) n++;
    }
    CHECK(n == 1);
    CHECK(s->sim_tick == 0);
}

/* ── 6. Encoded form uses only URL-safe characters ────────────────────── */

/* The encoded snapshot lives in a query param. Restricting it to RFC 3986
 * unreserved set + the field separators we picked (~ , : ) keeps the URL
 * readable; the URL bar won't %-encode anything in this range. */
static void test_encoded_form_is_url_safe(void) {
    g_test = "encoded_form_is_url_safe";
    game_init_with_configs_and_map(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG, TEST_MAP_CFG);
    game_set_placement(game_tower_id("GUNNER"));
    game_grid_click(6, 4);
    game_buy_creep_upgrade(2);  /* RETRIEVER_2X — has an underscore in id */

    char buf[4096];
    CHECK(game_snapshot_encode(buf, sizeof(buf)) > 0);

    for (const char *p = buf; *p; p++) {
        char c = *p;
        int safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                || (c >= '0' && c <= '9')
                || c == '~' || c == ':' || c == ',' || c == '-'
                || c == '_' || c == '.';
        CHECK(safe);
        if (!safe) {
            fprintf(stderr, "    offending char: 0x%02x at %ld\n",
                    (unsigned char)c, (long)(p - buf));
            break;
        }
    }
}

/* ── main ────────────────────────────────────────────────────────────── */

int main(void) {
    test_round_trip_basic();
    test_round_trip_preserves_tower_hp();
    test_load_drops_unknown_tower_id();
    test_load_survives_upgrade_reorder();
    test_simulate_phase_respawns_creeps();
    test_encoded_form_is_url_safe();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
