#include "game/tower_config.h"
#include <stdio.h>
#include <string.h>

/* Parser tests at the tower_config layer. Sub-behaviors (per-key parsing,
 * comment stripping, multi-section state, error returns) are exercised
 * here directly so the higher-layer game tests don't have to. */

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

/* ── 1. Embedded default config: every tower has the values the rest of
 *      the codebase relies on. If someone edits data/towers.cfg in a way
 *      that changes a load-bearing number, this is the test that catches
 *      it before the higher-layer game tests get confused. ─────────────── */
static void test_default_config_values(void) {
    g_test = "default_config_values";
    CHECK(tower_config_load_default() == 0);
    const TowerCatalog *c = tower_config_get();

    const TowerConfig *b = &c->towers[TOWER_BLOCKER];
    CHECK(b->cost == 20 && b->hp == 100 && b->build_turns == 1);
    CHECK(b->code == 'B' && !strcmp(b->name, "Blocker"));
    CHECK(b->upgrade_cost == 20 && b->upgrade_build == 1 && b->upgrade_hp_bonus == 20);
    CHECK(b->level[0].range == 0 && b->level[1].range == 0); /* no attack */

    const TowerConfig *g = &c->towers[TOWER_GUNNER];
    CHECK(g->cost == 30 && g->hp == 50 && g->build_turns == 0);
    CHECK(g->code == 'G' && !strcmp(g->name, "Gunner"));
    CHECK(g->level[0].dmg == 10 && g->level[0].range == 3 && g->level[0].cooldown == 2);
    CHECK(g->level[1].dmg == 15 && g->level[1].range == 4 && g->level[1].cooldown == 2);

    const TowerConfig *sl = &c->towers[TOWER_SLAMMER];
    CHECK(sl->cost == 50 && sl->hp == 50 && sl->build_turns == 2);
    CHECK(sl->level[0].dmg == 5 && sl->level[0].aoe == 1 && sl->level[0].slow == 2);
    CHECK(sl->level[1].dmg == 8 && sl->level[1].aoe == 2 && sl->level[1].slow == 2);

    const TowerConfig *r = &c->towers[TOWER_RESOURCE];
    CHECK(r->cost == 80 && r->hp == 30 && r->build_turns == 3);
    CHECK(r->level[0].income == 10 && r->level[1].income == 20);
}

/* ── 2. Comments, blank lines, indentation are all ignored. ─────────── */
static void test_whitespace_and_comments(void) {
    g_test = "whitespace_and_comments";
    const char *src =
        "# leading comment\n"
        "\n"
        "tower GUNNER     # inline comment\n"
        "\n"
        "      cost   33\n"
        "\thp\t44\n"
        "  # commented-out: build_turns 99\n"
        "  level1.dmg 7\n";
    CHECK(tower_config_load_from_string(src) == 0);
    const TowerConfig *g = &tower_config_get()->towers[TOWER_GUNNER];
    CHECK(g->cost == 33);
    CHECK(g->hp == 44);
    CHECK(g->build_turns == 0);          /* defaulted, not 99 */
    CHECK(g->level[0].dmg == 7);
}

/* ── 3. Unknown keys, unknown sections, and orphan keys fail loudly. ─── */
static void test_error_cases(void) {
    g_test = "error_cases";
    CHECK(tower_config_load_from_string("tower GUNNER\n  cost 10\n") == 0);

    CHECK(tower_config_load_from_string("tower NOTATOWER\n") != 0);
    CHECK(tower_config_load_from_string("cost 10\n") != 0);           /* no section */
    CHECK(tower_config_load_from_string("tower GUNNER\n  fnord 1\n") != 0);
    CHECK(tower_config_load_from_string("tower GUNNER\n  cost abc\n") != 0);
    CHECK(tower_config_load_from_string("tower GUNNER\n  code BIG\n") != 0); /* not 1 char */
}

/* ── 4. Multiple tower sections, including a re-entry. The parser should
 *      route keys to whichever section was most recently named. ─────── */
static void test_multiple_sections(void) {
    g_test = "multiple_sections";
    const char *src =
        "tower BLOCKER\n"
        "  cost 1\n"
        "tower GUNNER\n"
        "  cost 2\n"
        "tower BLOCKER\n"
        "  hp 99\n";
    CHECK(tower_config_load_from_string(src) == 0);
    const TowerCatalog *c = tower_config_get();
    CHECK(c->towers[TOWER_BLOCKER].cost == 1);
    CHECK(c->towers[TOWER_BLOCKER].hp == 99);
    CHECK(c->towers[TOWER_GUNNER].cost == 2);
}

int main(void) {
    test_default_config_values();
    test_whitespace_and_comments();
    test_error_cases();
    test_multiple_sections();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
