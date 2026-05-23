#ifndef TEST_FIXTURES_H
#define TEST_FIXTURES_H

/* Tower-config fixture for behavior tests.
 *
 * Behavior tests in test_game.c and test_render.c call
 * game_init_with_tower_config(TEST_TOWERS_CFG) instead of game_init(), so the
 * shipped data/towers.cfg can be edited freely without breaking them.
 *
 * Pinned numbers below are the ones a behavior test would notice if changed:
 *   GUNNER L1   dmg=10 range=3 cooldown=2  → test_flag_drop_on_death is
 *                                            tuned to a 2-shot kill profile.
 *               build_turns=0              → test_tower_upgrade requires an
 *                                            immediately-upgradable tower.
 *   GUNNER L2   hp=70                       → test_tower_upgrade asserts
 *                                            absolute max HP at L2.
 *   RESOURCE    cost=80                     → test_placement_insufficient_*
 *                                            relies on RED having $20 left.
 *               build_turns=3, L1.income=10 → test_resource_tower_income
 *                                            counts 3 turns then +30/turn.
 *   SLAMMER     build_turns=2, slow>0, dmg>0 → test_slammer_slows_creep.
 *   CRITTER     crit_chance=100, dmg=1, crit_dmg=999 → test_tower_crit_uses_crit_dmg
 *                                            pins always-crit + lethal value so the
 *                                            test can assert from a single shot.
 * The other numbers exist for plausibility — tests don't pin them. */
static const char TEST_TOWERS_CFG[] =
    "tower BLOCKER\n"
    "  code B\n"
    "  name Blocker\n"
    "level BLOCKER 1\n"
    "  cost 20\n"
    "  hp 100\n"
    "  build_turns 1\n"
    "level BLOCKER 2\n"
    "  cost 20\n"
    "  hp 120\n"
    "  build_turns 1\n"

    "tower GUNNER\n"
    "  code G\n"
    "  name Gunner\n"
    "level GUNNER 1\n"
    "  cost 30\n"
    "  hp 50\n"
    "  build_turns 0\n"
    "  dmg 10\n"
    "  range 3\n"
    "  cooldown 2\n"
    "level GUNNER 2\n"
    "  cost 30\n"
    "  hp 70\n"
    "  build_turns 1\n"
    "  dmg 15\n"
    "  range 4\n"
    "  cooldown 2\n"

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
    "  cooldown 3\n"
    "level SLAMMER 2\n"
    "  cost 50\n"
    "  hp 70\n"
    "  build_turns 1\n"
    "  dmg 8\n"
    "  range 3\n"
    "  aoe 2\n"
    "  slow 2\n"
    "  cooldown 3\n"

    "tower RESOURCE\n"
    "  code R\n"
    "  name Resource\n"
    "level RESOURCE 1\n"
    "  cost 80\n"
    "  hp 30\n"
    "  build_turns 3\n"
    "  income 10\n"
    "level RESOURCE 2\n"
    "  cost 80\n"
    "  hp 50\n"
    "  build_turns 1\n"
    "  income 20\n"

    "tower CRITTER\n"
    "  code C\n"
    "  name Critter\n"
    "level CRITTER 1\n"
    "  cost 30\n"
    "  hp 50\n"
    "  build_turns 0\n"
    "  dmg 1\n"
    "  crit_chance 100\n"
    "  crit_dmg 999\n"
    "  range 3\n"
    "  cooldown 2\n";

/* Creep-type + creep-upgrade fixture for behavior tests. Mirrors the
 * slot-by-slot shape that test_game.c and test_render.c hard-code:
 *   creep type RETRIEVER (bare id)
 *   creep type SIEGE     (bare id)
 *   slot 0: +1 retriever, 1-turn research
 *   slot 1: +2 siege,      1-turn research
 *   slot 2: +2 retrievers, 2-turn research, cost 60 (creep_upgrade_purchase_and_research)
 *   slot 3: +2 siege II,   2-turn research
 *   slot 4: +1 retriever, 0-turn research (instant) — exercises the
 *           research_turns=0 same-turn-spawn path
 * Each upgrade carries its creep's full profile (code, hp,
 * can_carry_flag/melee_damage). Slot order is what test_game.c references
 * by index, so changes here must keep those slots aligned. RETRIEVER
 * upgrades target the same type — under the override semantic the
 * latest-declared completed one wins. */
static const char TEST_CREEP_UPGRADES_CFG[] =
    "creep RETRIEVER\n"
    "creep SIEGE\n"

    "upgrade RETRIEVER_1\n"
    "  cost            30\n"
    "  research_turns  1\n"
    "  creep           RETRIEVER\n"
    "  count           1\n"
    "  code            R\n"
    "  hp              20\n"
    "  can_carry_flag  1\n"
    "  description     +1 Retriever\n"

    "upgrade SIEGE_2\n"
    "  cost            40\n"
    "  research_turns  1\n"
    "  creep           SIEGE\n"
    "  count           2\n"
    "  code            S\n"
    "  hp              40\n"
    "  melee_damage    5\n"
    "  description     +2 Siege\n"

    "upgrade RETRIEVER_2X\n"
    "  cost            60\n"
    "  research_turns  2\n"
    "  creep           RETRIEVER\n"
    "  count           2\n"
    "  code            R\n"
    "  hp              20\n"
    "  can_carry_flag  1\n"
    "  description     +2 Retrievers\n"

    "upgrade SIEGE_2_II\n"
    "  cost            70\n"
    "  research_turns  2\n"
    "  creep           SIEGE\n"
    "  count           2\n"
    "  code            S\n"
    "  hp              40\n"
    "  melee_damage    5\n"
    "  description     +2 Siege II\n"

    "upgrade RETRIEVER_INSTANT\n"
    "  cost            20\n"
    "  research_turns  0\n"
    "  creep           RETRIEVER\n"
    "  count           1\n"
    "  code            R\n"
    "  hp              20\n"
    "  can_carry_flag  1\n"
    "  description     +1 Retriever instant\n";

/* Map fixture for behavior tests. Mirrors the 30x20 layout the suite was
 * originally written against — RED zone at x<10, BLUE at x>=20, neutral
 * strip in between with no debris. Landmark coords match what
 * test_game.c / test_render.c reference directly: RED spawn (10,8),
 * BLUE spawn (19,11), RED receptacle (4,4), BLUE receptacle (25,15),
 * RED flag (4,15), BLUE flag (25,4). Tests use this instead of
 * data/map.cfg so the shipped map can be edited freely. */
static const char TEST_MAP_CFG[] =
    "grid\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRR[RRRRR..........BBBBBbBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR1.........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR.........2BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRrRRRRR..........BBBBB]BBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n"
    "RRRRRRRRRR..........BBBBBBBBBB\n";

/* Narrow-corridor map for the two BLUE-creep behavior tests that need
 * a wobble-free path: test_flag_drop_on_death and
 * test_banana_creep_carries_and_attacks. With wobble (random tiebreak
 * among equal shortest paths) live, those tests can't depend on a
 * specific approach route through the 30x20 fixture map. Here the BLUE
 * spawn → RED flag leg is a straight horizontal corridor: spawn (19,2)
 * walking west along y=2 to the RED flag at (3,2). Every step on row 2
 * is the unique shortest move (a y=1 or y=3 detour adds 2 steps), so
 * BFS picks one candidate without rolling RNG — the tests can assert
 * "at tick N the creep is at (15-N, 2)" without flakiness. RED's
 * landmarks (red_spawn, red_recep, blue_flag) exist only to satisfy
 * map validity. Width 20, height 5 keeps the sim small. */
static const char TEST_MAP_CORRIDOR_CFG[] =
    "grid\n"
    "RRRRRRRRRR..........\n"
    "RRRRRRRRRR..........\n"
    "1RRrRRRRRR.........2\n"
    "[RRRRRRRRR..........\n"
    "RRRRRRRRRRb........]\n";

/* Spawn-order fixture: two upgrades with explicit spawn_order keys so
 * the queue's sort visibly disagrees with upgrade-buy order. RETRIEVER_1
 * is bought first (slot 0) but has spawn_order=2; SIEGE_1 is bought
 * second (slot 1) but has spawn_order=1, so after sort SIEGE appears
 * before RETRIEVER.
 *
 * Slots used by tests:
 *   0: RETRIEVER_1 — +1 RETRIEVER, instant research, spawn_order 2
 *   1: SIEGE_1    — +1 SIEGE,     instant research, spawn_order 1
 * Both instant so a single enter_sim() turn floats both into the queue
 * without needing a research-turn delay. */
static const char TEST_CREEP_SPAWN_ORDER_CFG[] =
    "creep RETRIEVER\n"
    "creep SIEGE\n"

    "upgrade RETRIEVER_1\n"
    "  cost            10\n"
    "  research_turns  0\n"
    "  creep           RETRIEVER\n"
    "  count           1\n"
    "  code            R\n"
    "  hp              20\n"
    "  can_carry_flag  1\n"
    "  spawn_order     2\n"
    "  description     +1 Retriever\n"

    "upgrade SIEGE_1\n"
    "  cost            10\n"
    "  research_turns  0\n"
    "  creep           SIEGE\n"
    "  count           1\n"
    "  code            S\n"
    "  hp              40\n"
    "  melee_damage    5\n"
    "  spawn_order     1\n"
    "  description     +1 Siege\n";

/* BANANA fixture for test_banana_creep_carries_and_attacks. A single
 * upgrade spawns 1 creep that both carries the flag (can_carry_flag=1)
 * AND damages adjacent enemy towers (melee_damage > 0). Slot 0 is the
 * only buyable upgrade. */
static const char TEST_CREEP_BANANA_CFG[] =
    "creep BANANA\n"

    "upgrade BANANA_UPG\n"
    "  cost            50\n"
    "  research_turns  1\n"
    "  creep           BANANA\n"
    "  count           1\n"
    "  code            N\n"
    "  hp              30\n"
    "  can_carry_flag  1\n"
    "  melee_damage    3\n"
    "  description     +1 Banana\n";

#endif
