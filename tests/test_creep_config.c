#include "game/creep_config.h"
#include <stdio.h>
#include <string.h>

/* Parser tests at the creep_config layer. Sub-behaviors (per-key parsing,
 * comment stripping, section dispatch, error returns, multi-word
 * descriptions, the two-token `spawn` directive, type-reference
 * resolution) are exercised here directly so the higher-layer game
 * tests don't have to. */

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

/* ── 1. A canonical type + upgrade combo parses every field cleanly. ─── */
static void test_basic_parse(void) {
    g_test = "basic_parse";
    const char *src =
        "creep RUNNER\n"
        "  code            R\n"
        "  hp              20\n"
        "  can_carry_flag  1\n"
        "creep MAULER\n"
        "  code            M\n"
        "  hp              40\n"
        "  melee_damage    5\n"
        "upgrade ALPHA\n"
        "  cost            30\n"
        "  research_turns  1\n"
        "  spawn           RUNNER 1\n"
        "  description     +1 Runner\n"
        "upgrade BETA\n"
        "  cost            70\n"
        "  research_turns  2\n"
        "  spawn           MAULER 2\n"
        "  description     +2 Mauler II\n";
    CHECK(creep_config_load_from_string(src) == 0);
    CHECK(creep_config_get()->type_count == 2);
    CHECK(creep_config_get()->upgrade_count == 2);

    int runner = creep_config_lookup_type("RUNNER");
    int mauler = creep_config_lookup_type("MAULER");
    CHECK(runner == 0);
    CHECK(mauler == 1);

    const CreepTypeConfig *R = &creep_config_get()->types[runner];
    CHECK(R->code == 'R');
    CHECK(R->hp == 20);
    CHECK(R->can_carry_flag == 1);
    CHECK(R->melee_damage == 0);

    const CreepTypeConfig *M = &creep_config_get()->types[mauler];
    CHECK(M->code == 'M');
    CHECK(M->hp == 40);
    CHECK(M->can_carry_flag == 0);
    CHECK(M->melee_damage == 5);

    int a = creep_config_lookup_upgrade("ALPHA");
    const CreepUpgradeConfig *A = &creep_config_get()->upgrades[a];
    CHECK(A->cost == 30);
    CHECK(A->research_turns == 1);
    CHECK(A->spawn_type == runner);
    CHECK(A->spawn_count == 1);
    CHECK(!strcmp(A->description, "+1 Runner"));

    const CreepUpgradeConfig *B = &creep_config_get()->upgrades[creep_config_lookup_upgrade("BETA")];
    CHECK(B->spawn_type == mauler);
    CHECK(B->spawn_count == 2);
    CHECK(!strcmp(B->description, "+2 Mauler II"));
}

/* ── 2. A single creep type can combine carry+melee — this is the
 *      "BANANA" property at the parser layer. ─────────────────────── */
static void test_combined_behaviors(void) {
    g_test = "combined_behaviors";
    const char *src =
        "creep BANANA\n"
        "  code            N\n"
        "  hp              30\n"
        "  can_carry_flag  1\n"
        "  melee_damage    3\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepTypeConfig *b = &creep_config_get()->types[0];
    CHECK(b->can_carry_flag == 1);
    CHECK(b->melee_damage == 3);
    CHECK(b->hp == 30);
}

/* ── 3. `spawn` resolves the creep id to a type index. Bad ids fail. ── */
static void test_spawn_resolution(void) {
    g_test = "spawn_resolution";
    /* Forward reference: `spawn` requires the creep type to be declared
     * BEFORE the upgrade that references it (catalog is built top-down
     * and lookup happens at parse time). */
    const char *forward =
        "upgrade U\n"
        "  cost 1\n"
        "  spawn RUNNER 1\n"
        "creep RUNNER\n"
        "  code R\n";
    CHECK(creep_config_load_from_string(forward) != 0);

    /* Unknown creep id in `spawn` is an error. */
    const char *unknown =
        "creep RUNNER\n  code R\n"
        "upgrade U\n  cost 1\n  spawn GHOST 1\n";
    CHECK(creep_config_load_from_string(unknown) != 0);

    /* `spawn` missing count token is an error. */
    const char *no_count =
        "creep RUNNER\n  code R\n"
        "upgrade U\n  cost 1\n  spawn RUNNER\n";
    CHECK(creep_config_load_from_string(no_count) != 0);
}

/* ── 4. Descriptions still consume the rest of the line, with embedded
 *      spaces preserved. ──────────────────────────────────────────── */
static void test_description_multi_word(void) {
    g_test = "description_multi_word";
    const char *src =
        "creep R\n  code R\n"
        "upgrade FOO\n"
        "  cost 10\n"
        "  description  hello   world   from creeps  \n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    CHECK(!strcmp(u->description, "hello   world   from creeps"));
}

/* ── 5. Comments, blank lines, and indentation are all ignored. ───── */
static void test_whitespace_and_comments(void) {
    g_test = "whitespace_and_comments";
    const char *src =
        "# leading comment\n"
        "\n"
        "creep G     # inline comment\n"
        "      code   G\n"
        "\thp\t44\n"
        "  # commented-out: hp 999\n"
        "  melee_damage 4\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepTypeConfig *g = &creep_config_get()->types[0];
    CHECK(g->code == 'G');
    CHECK(g->hp == 44);             /* not 999 — that line is a comment */
    CHECK(g->melee_damage == 4);
}

/* ── 6. Unspecified keys default to 0. An upgrade with no `spawn`
 *      directive has spawn_type = -1 (sentinel for "spawns nothing"). ─ */
static void test_defaults(void) {
    g_test = "defaults";
    const char *src =
        "upgrade ZERO\n"
        "  cost 5\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    CHECK(u->cost == 5);
    CHECK(u->research_turns == 0);
    CHECK(u->spawn_type == -1);
    CHECK(u->spawn_count == 0);
    CHECK(u->description[0] == 0);
}

/* ── 7. Structural error cases reject loudly. ─────────────────────── */
static void test_error_cases(void) {
    g_test = "error_cases";
    /* Baseline: a minimal valid config. */
    CHECK(creep_config_load_from_string("upgrade X\n  cost 1\n") == 0);

    /* Key with no enclosing section. */
    CHECK(creep_config_load_from_string("cost 10\n") != 0);

    /* Unknown key inside an upgrade section. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  fnord 1\n") != 0);

    /* Unknown key inside a creep section. */
    CHECK(creep_config_load_from_string(
              "creep X\n  code X\n  fnord 1\n") != 0);

    /* Non-integer where an int is expected. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost abc\n") != 0);

    /* `code` must be exactly one char. */
    CHECK(creep_config_load_from_string(
              "creep X\n  code BIG\n") != 0);

    /* Section directive with no id. */
    CHECK(creep_config_load_from_string("upgrade\n") != 0);
    CHECK(creep_config_load_from_string("creep\n") != 0);

    /* Duplicate upgrade id. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost 1\n"
              "upgrade X\n  cost 2\n") != 0);

    /* Duplicate creep id. */
    CHECK(creep_config_load_from_string(
              "creep X\n  code X\n"
              "creep X\n  code Y\n") != 0);

    /* Key present but value missing. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost\n") != 0);
}

/* ── 8. Declaration order is preserved as catalog index order, separately
 *      for types and upgrades. ────────────────────────────────────── */
static void test_declaration_order(void) {
    g_test = "declaration_order";
    const char *src =
        "creep ZULU\n  code Z\n"
        "creep ALPHA\n  code A\n"
        "creep MIKE\n  code M\n"
        "upgrade U1\n  cost 1\n"
        "upgrade U0\n  cost 1\n";
    CHECK(creep_config_load_from_string(src) == 0);
    CHECK(creep_config_get()->type_count == 3);
    CHECK(creep_config_get()->upgrade_count == 2);
    CHECK(creep_config_lookup_type("ZULU") == 0);
    CHECK(creep_config_lookup_type("ALPHA") == 1);
    CHECK(creep_config_lookup_type("MIKE") == 2);
    CHECK(creep_config_lookup_upgrade("U1") == 0);
    CHECK(creep_config_lookup_upgrade("U0") == 1);
}

int main(void) {
    test_basic_parse();
    test_combined_behaviors();
    test_spawn_resolution();
    test_description_multi_word();
    test_whitespace_and_comments();
    test_defaults();
    test_error_cases();
    test_declaration_order();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
