#include "game/creep_config.h"
#include <stdio.h>
#include <string.h>

/* Parser tests at the creep_config layer. Sub-behaviors (per-key parsing,
 * comment stripping, section dispatch, error returns, multi-word
 * descriptions, the upgrade's `creep` target reference, body-only-on-
 * upgrades restriction) are exercised here directly so the higher-layer
 * game tests don't have to. */

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

/* ── 1. A canonical type list + upgrade combo parses every field cleanly.
 *      Types are bare identifiers; the upgrade owns the creep profile. ─ */
static void test_basic_parse(void) {
    g_test = "basic_parse";
    const char *src =
        "creep RUNNER\n"
        "creep MAULER\n"
        "upgrade ALPHA\n"
        "  cost            30\n"
        "  research_turns  1\n"
        "  creep           RUNNER\n"
        "  count           1\n"
        "  code            R\n"
        "  hp              20\n"
        "  can_carry_flag  1\n"
        "  description     +1 Runner\n"
        "upgrade BETA\n"
        "  cost            70\n"
        "  research_turns  2\n"
        "  creep           MAULER\n"
        "  count           2\n"
        "  code            M\n"
        "  hp              40\n"
        "  melee_damage    5\n"
        "  description     +2 Mauler II\n";
    CHECK(creep_config_load_from_string(src) == 0);
    CHECK(creep_config_get()->type_count == 2);
    CHECK(creep_config_get()->upgrade_count == 2);

    int runner = creep_config_lookup_type("RUNNER");
    int mauler = creep_config_lookup_type("MAULER");
    CHECK(runner == 0);
    CHECK(mauler == 1);

    int a = creep_config_lookup_upgrade("ALPHA");
    const CreepUpgradeConfig *A = &creep_config_get()->upgrades[a];
    CHECK(A->cost == 30);
    CHECK(A->research_turns == 1);
    CHECK(A->creep_type == runner);
    CHECK(A->count == 1);
    CHECK(A->code == 'R');
    CHECK(A->hp == 20);
    CHECK(A->can_carry_flag == 1);
    CHECK(A->melee_damage == 0);
    CHECK(!strcmp(A->description, "+1 Runner"));

    const CreepUpgradeConfig *B = &creep_config_get()->upgrades[creep_config_lookup_upgrade("BETA")];
    CHECK(B->creep_type == mauler);
    CHECK(B->count == 2);
    CHECK(B->code == 'M');
    CHECK(B->hp == 40);
    CHECK(B->can_carry_flag == 0);
    CHECK(B->melee_damage == 5);
    CHECK(!strcmp(B->description, "+2 Mauler II"));
}

/* ── 2. A single upgrade can set both can_carry_flag and melee_damage —
 *      the "BANANA" property, now lifted onto the upgrade. ──────────── */
static void test_combined_behaviors(void) {
    g_test = "combined_behaviors";
    const char *src =
        "creep BANANA\n"
        "upgrade BNN\n"
        "  cost            5\n"
        "  creep           BANANA\n"
        "  count           1\n"
        "  code            N\n"
        "  hp              30\n"
        "  can_carry_flag  1\n"
        "  melee_damage    3\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    CHECK(u->can_carry_flag == 1);
    CHECK(u->melee_damage == 3);
    CHECK(u->hp == 30);
}

/* ── 3. `creep` directive inside an upgrade resolves to a type index.
 *      Bad ids fail; forward references fail; missing arg fails. ────── */
static void test_creep_resolution(void) {
    g_test = "creep_resolution";
    /* Forward reference: the upgrade's `creep` directive requires the
     * creep type to be declared BEFORE the upgrade (catalog is built
     * top-down and lookup happens at parse time). */
    const char *forward =
        "upgrade U\n"
        "  cost 1\n"
        "  creep RUNNER\n"
        "creep RUNNER\n";
    CHECK(creep_config_load_from_string(forward) != 0);

    /* Unknown creep id is an error. */
    const char *unknown =
        "creep RUNNER\n"
        "upgrade U\n  cost 1\n  creep GHOST\n";
    CHECK(creep_config_load_from_string(unknown) != 0);

    /* `creep` directive with empty value is treated as starting a new
     * type section, which then errors on missing id. */
    const char *no_target =
        "creep RUNNER\n"
        "upgrade U\n  cost 1\n  creep\n";
    CHECK(creep_config_load_from_string(no_target) != 0);
}

/* ── 4. Descriptions still consume the rest of the line, with embedded
 *      spaces preserved. ──────────────────────────────────────────── */
static void test_description_multi_word(void) {
    g_test = "description_multi_word";
    const char *src =
        "creep R\n"
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
        "upgrade UG\n"
        "      code   G\n"
        "\thp\t44\n"
        "  # commented-out: hp 999\n"
        "  melee_damage 4\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    CHECK(u->code == 'G');
    CHECK(u->hp == 44);             /* not 999 — that line is a comment */
    CHECK(u->melee_damage == 4);
}

/* ── 6. Unspecified keys default to 0. An upgrade with no `creep`
 *      directive has creep_type = -1 (sentinel for "spawns nothing"),
 *      and set_flags is empty so the merge walk in game.c won't try to
 *      overlay anything for it. ─────────────────────────────────────── */
static void test_defaults(void) {
    g_test = "defaults";
    const char *src =
        "upgrade ZERO\n"
        "  cost 5\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    CHECK(u->cost == 5);
    CHECK(u->research_turns == 0);
    CHECK(u->creep_type == -1);
    CHECK(u->count == 0);
    CHECK(u->code == 0);
    CHECK(u->hp == 0);
    CHECK(u->can_carry_flag == 0);
    CHECK(u->melee_damage == 0);
    CHECK(u->spawn_order == 0);
    CHECK(u->requires == -1);
    CHECK(u->description[0] == 0);
    CHECK(u->set_flags == 0);
}

/* ── 6b. set_flags records exactly which profile fields the parser saw,
 *       so the merge layer can distinguish "not set" from "set to 0". ── */
static void test_set_flags(void) {
    g_test = "set_flags";
    const char *src =
        "creep R\n"
        "upgrade FULL\n"
        "  cost            1\n"
        "  creep           R\n"
        "  count           2\n"
        "  code            R\n"
        "  hp              0\n"           /* explicit zero — must still flip the HP bit */
        "  can_carry_flag  1\n"
        "  melee_damage    3\n"
        "  spawn_order     4\n"
        "upgrade PARTIAL\n"
        "  cost            1\n"
        "  creep           R\n"
        "  hp              50\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *full    = &creep_config_get()->upgrades[0];
    const CreepUpgradeConfig *partial = &creep_config_get()->upgrades[1];

    unsigned all = CREEP_UPG_SET_COUNT | CREEP_UPG_SET_CODE | CREEP_UPG_SET_HP |
                   CREEP_UPG_SET_CAN_CARRY_FLAG | CREEP_UPG_SET_MELEE_DAMAGE |
                   CREEP_UPG_SET_SPAWN_ORDER;
    CHECK(full->set_flags == all);
    CHECK(full->hp == 0);                 /* explicit-zero round trips */

    CHECK(partial->set_flags == CREEP_UPG_SET_HP);
    CHECK(partial->hp        == 50);
    CHECK(partial->count     == 0);       /* default; merge layer treats as inherit */
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

    /* Creep type sections have no body — any key under one is an error. */
    CHECK(creep_config_load_from_string(
              "creep X\n  code X\n") != 0);

    /* Non-integer where an int is expected. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost abc\n") != 0);

    /* `code` must be exactly one char. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  code BIG\n") != 0);

    /* Section directive with no id. */
    CHECK(creep_config_load_from_string("upgrade\n") != 0);
    CHECK(creep_config_load_from_string("creep\n") != 0);

    /* Duplicate upgrade id. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost 1\n"
              "upgrade X\n  cost 2\n") != 0);

    /* Duplicate creep id. */
    CHECK(creep_config_load_from_string(
              "creep X\n"
              "creep X\n") != 0);

    /* Key present but value missing. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost\n") != 0);
}

/* ── 7b. spawn_order is now an upgrade-level key. Parses as int and
 *       defaults to 0; non-integer rejects. ─────────────────────────── */
static void test_spawn_order(void) {
    g_test = "spawn_order";
    const char *src =
        "creep FAST\n"
        "creep SLOW\n"
        "creep UNSET\n"
        "upgrade UF\n  cost 1\n  creep FAST\n  spawn_order 1\n"
        "upgrade US\n  cost 1\n  creep SLOW\n  spawn_order 9\n"
        "upgrade UU\n  cost 1\n  creep UNSET\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepCatalog *c = creep_config_get();
    CHECK(c->upgrades[creep_config_lookup_upgrade("UF")].spawn_order == 1);
    CHECK(c->upgrades[creep_config_lookup_upgrade("US")].spawn_order == 9);
    CHECK(c->upgrades[creep_config_lookup_upgrade("UU")].spawn_order == 0);

    /* Non-integer value is rejected like every other int field. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost 1\n  spawn_order banana\n") != 0);
}

/* ── 7c. `requires` resolves to a previously-declared upgrade index. ─── */
static void test_requires(void) {
    g_test = "requires";

    /* Happy path: B requires A. */
    const char *ok =
        "creep R\n"
        "upgrade A\n  cost 1\n"
        "upgrade B\n  cost 1\n  requires A\n";
    CHECK(creep_config_load_from_string(ok) == 0);
    int a = creep_config_lookup_upgrade("A");
    int b = creep_config_lookup_upgrade("B");
    CHECK(creep_config_get()->upgrades[a].requires == -1);
    CHECK(creep_config_get()->upgrades[b].requires == a);

    /* Default: no `requires` directive → -1. */
    const char *bare =
        "upgrade ONLY\n  cost 1\n";
    CHECK(creep_config_load_from_string(bare) == 0);
    CHECK(creep_config_get()->upgrades[0].requires == -1);

    /* Forward reference: B requires C, but C is declared later. Lookup
     * happens at parse time (same rule as `creep` directive). */
    const char *forward =
        "upgrade B\n  cost 1\n  requires C\n"
        "upgrade C\n  cost 1\n";
    CHECK(creep_config_load_from_string(forward) != 0);

    /* Unknown id is an error. */
    const char *unknown =
        "upgrade A\n  cost 1\n  requires GHOST\n";
    CHECK(creep_config_load_from_string(unknown) != 0);

    /* Self-reference is an error — an upgrade can't gate itself. */
    const char *self =
        "upgrade A\n  cost 1\n  requires A\n";
    CHECK(creep_config_load_from_string(self) != 0);
}

/* ── 8. Declaration order is preserved as catalog index order, separately
 *      for types and upgrades. ────────────────────────────────────── */
static void test_declaration_order(void) {
    g_test = "declaration_order";
    const char *src =
        "creep ZULU\n"
        "creep ALPHA\n"
        "creep MIKE\n"
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
    test_creep_resolution();
    test_description_multi_word();
    test_whitespace_and_comments();
    test_defaults();
    test_set_flags();
    test_error_cases();
    test_spawn_order();
    test_requires();
    test_declaration_order();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
