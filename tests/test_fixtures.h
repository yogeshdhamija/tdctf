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
    "  income 20\n";

/* Creep-upgrade fixture for behavior tests. Mirrors the slot-by-slot
 * shape that test_game.c and test_render.c hard-code:
 *   slot 0: +1 retriever, 1-turn research
 *   slot 1: +2 siege,      1-turn research
 *   slot 2: +2 retrievers, 2-turn research, cost 60 (creep_upgrade_purchase_and_research)
 *   slot 3: +2 siege II,   2-turn research
 * Slot order is what test_game.c references by index, so changes here must
 * keep those four slots aligned. */
static const char TEST_CREEP_UPGRADES_CFG[] =
    "upgrade RETRIEVER_1\n"
    "  cost            30\n"
    "  research_turns  1\n"
    "  add_retrievers  1\n"
    "  description     +1 Retriever\n"

    "upgrade SIEGE_2\n"
    "  cost            40\n"
    "  research_turns  1\n"
    "  add_siege       2\n"
    "  description     +2 Siege\n"

    "upgrade RETRIEVER_2X\n"
    "  cost            60\n"
    "  research_turns  2\n"
    "  add_retrievers  2\n"
    "  description     +2 Retrievers\n"

    "upgrade SIEGE_2_II\n"
    "  cost            70\n"
    "  research_turns  2\n"
    "  add_siege       2\n"
    "  description     +2 Siege II\n";

#endif
