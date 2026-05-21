#include "game/tower_config.h"
#include <stdio.h>
#include <string.h>

/* Parser tests at the tower_config layer. Sub-behaviors (per-key parsing,
 * comment stripping, section dispatch, error returns, level sequencing)
 * are exercised here directly so the higher-layer game tests don't have to. */

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

/* ── 2. Comments, blank lines, and indentation are all ignored. ─────── */
static void test_whitespace_and_comments(void) {
    g_test = "whitespace_and_comments";
    const char *src =
        "# leading comment\n"
        "\n"
        "tower GUNNER     # inline comment\n"
        "  code G\n"
        "\n"
        "level GUNNER 1\n"
        "      cost   33\n"
        "\thp\t44\n"
        "  # commented-out: cost 999\n"
        "  dmg 7\n";
    CHECK(tower_config_load_from_string(src) == 0);
    int gi = tower_config_lookup("GUNNER");
    CHECK(gi >= 0);
    const TowerConfig *g = &tower_config_get()->towers[gi];
    CHECK(g->code == 'G');
    CHECK(g->level_count == 1);
    CHECK(g->level[0].cost == 33);          /* not 999 — the cost-999 line is a comment */
    CHECK(g->level[0].hp == 44);
    CHECK(g->level[0].dmg == 7);
}

/* ── 3. Any tower can have any combination of features. A single tower
 *      that damages, AoEs, slows, AND generates income parses cleanly
 *      and surfaces every field — there's no implicit "this kind of
 *      tower doesn't have that field" rule. ───────────────────────── */
static void test_combined_features(void) {
    g_test = "combined_features";
    const char *src =
        "tower BUFFALO\n"
        "  code U\n"
        "  name Buffalo\n"
        "level BUFFALO 1\n"
        "  cost 75\n"
        "  hp 60\n"
        "  build_turns 2\n"
        "  dmg 4\n"
        "  range 3\n"
        "  aoe 1\n"
        "  slow 1\n"
        "  cooldown 2\n"
        "  income 5\n"
        "  crit_chance 25\n"
        "  crit_dmg 17\n";
    CHECK(tower_config_load_from_string(src) == 0);
    int bi = tower_config_lookup("BUFFALO");
    CHECK(bi >= 0);
    const TowerConfig *b = &tower_config_get()->towers[bi];
    CHECK(b->level_count == 1);
    const TowerLevelStats *L = &b->level[0];
    CHECK(L->cost == 75 && L->hp == 60 && L->build_turns == 2);
    CHECK(L->dmg == 4 && L->range == 3 && L->aoe == 1);
    CHECK(L->slow == 1 && L->cooldown == 2 && L->income == 5);
    CHECK(L->crit_chance == 25 && L->crit_dmg == 17);

    /* Unspecified crit_* keys default to 0 — implicit-zero is what callers
     * rely on to mean "no crits", so test it explicitly. */
    const char *no_crit =
        "tower PLAIN\n"
        "  code P\n"
        "level PLAIN 1\n"
        "  cost 1\n"
        "  hp 1\n"
        "  dmg 3\n";
    CHECK(tower_config_load_from_string(no_crit) == 0);
    const TowerConfig *p = &tower_config_get()->towers[tower_config_lookup("PLAIN")];
    CHECK(p->level[0].crit_chance == 0 && p->level[0].crit_dmg == 0);
}

/* ── 4. Each level fully redefines everything. Level 2 can flip every
 *      stat — cost, hp, build_turns, attack profile — relative to L1. ─ */
static void test_levels_independent(void) {
    g_test = "levels_independent";
    const char *src =
        "tower SHAPESHIFTER\n"
        "  code Z\n"
        "level SHAPESHIFTER 1\n"
        "  cost 10\n"
        "  hp 1\n"
        "  range 1\n"
        "  dmg 100\n"
        "level SHAPESHIFTER 2\n"
        "  cost 999\n"
        "  hp 9999\n"
        "  range 0\n"
        "  dmg 0\n"
        "  income 50\n";
    CHECK(tower_config_load_from_string(src) == 0);
    int idx = tower_config_lookup("SHAPESHIFTER");
    const TowerConfig *t = &tower_config_get()->towers[idx];
    CHECK(t->level_count == 2);
    CHECK(t->level[0].cost == 10   && t->level[0].hp == 1    && t->level[0].dmg == 100);
    CHECK(t->level[0].income == 0  && t->level[0].range == 1);
    CHECK(t->level[1].cost == 999  && t->level[1].hp == 9999 && t->level[1].dmg == 0);
    CHECK(t->level[1].income == 50 && t->level[1].range == 0);
}

/* ── 5. Structural error cases reject loudly. ───────────────────────── */
static void test_error_cases(void) {
    g_test = "error_cases";
    /* Baseline: a minimal valid config. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\nlevel X 1\n  cost 10\n  hp 1\n") == 0);

    /* Key with no enclosing section. */
    CHECK(tower_config_load_from_string("cost 10\n") != 0);

    /* Unknown key inside a tower section. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\n  fnord 1\nlevel X 1\n  cost 1\n") != 0);

    /* Unknown key inside a level section. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\nlevel X 1\n  fnord 1\n") != 0);

    /* Non-integer where an int is expected. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\nlevel X 1\n  cost abc\n") != 0);

    /* `code` must be exactly one char. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code BIG\nlevel X 1\n  cost 1\n") != 0);

    /* Level before its tower is declared. */
    CHECK(tower_config_load_from_string(
              "level X 1\n  cost 1\n") != 0);

    /* Duplicate tower name. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\nlevel X 1\n  cost 1\n"
              "tower X\n  code Y\n") != 0);

    /* Level numbers must be sequential — declaring level 2 before level 1
     * is an error. */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\nlevel X 2\n  cost 1\n") != 0);

    /* Non-sequential level (skipping). */
    CHECK(tower_config_load_from_string(
              "tower X\n  code X\n"
              "level X 1\n  cost 1\n"
              "level X 3\n  cost 1\n") != 0);

    /* A tower with no levels at all — placement code needs level[0]. */
    CHECK(tower_config_load_from_string("tower X\n  code X\n") != 0);
}

/* ── 6. Tower order is preserved in the catalog (declaration order
 *      becomes index order), and lookup resolves names to those indices. */
static void test_declaration_order(void) {
    g_test = "declaration_order";
    const char *src =
        "tower ZULU\n  code Z\nlevel ZULU 1\n  cost 1\n  hp 1\n"
        "tower ALPHA\n  code A\nlevel ALPHA 1\n  cost 1\n  hp 1\n"
        "tower MIKE\n  code M\nlevel MIKE 1\n  cost 1\n  hp 1\n";
    CHECK(tower_config_load_from_string(src) == 0);
    CHECK(tower_config_get()->count == 3);
    CHECK(tower_config_lookup("ZULU") == 0);
    CHECK(tower_config_lookup("ALPHA") == 1);
    CHECK(tower_config_lookup("MIKE") == 2);
}

int main(void) {
    test_whitespace_and_comments();
    test_combined_features();
    test_levels_independent();
    test_error_cases();
    test_declaration_order();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
