#include "game/creep_config.h"
#include <stdio.h>
#include <string.h>

/* Parser tests at the creep_config layer. Sub-behaviors (per-key parsing,
 * comment stripping, section dispatch, error returns, multi-word
 * descriptions) are exercised here directly so the higher-layer game
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

/* ── 1. A canonical multi-upgrade file parses every field cleanly. ────── */
static void test_basic_parse(void) {
    g_test = "basic_parse";
    const char *src =
        "upgrade ALPHA\n"
        "  cost            30\n"
        "  research_turns  1\n"
        "  add_retrievers  1\n"
        "  description     +1 Retriever\n"
        "upgrade BETA\n"
        "  cost            70\n"
        "  research_turns  2\n"
        "  add_siege       2\n"
        "  description     +2 Siege II\n";
    CHECK(creep_config_load_from_string(src) == 0);
    CHECK(creep_config_get()->count == 2);

    int a = creep_config_lookup("ALPHA");
    int b = creep_config_lookup("BETA");
    CHECK(a == 0);
    CHECK(b == 1);

    const CreepUpgradeConfig *A = &creep_config_get()->upgrades[a];
    CHECK(A->cost == 30);
    CHECK(A->research_turns == 1);
    CHECK(A->add_retrievers == 1);
    CHECK(A->add_siege == 0);
    CHECK(!strcmp(A->description, "+1 Retriever"));

    const CreepUpgradeConfig *B = &creep_config_get()->upgrades[b];
    CHECK(B->cost == 70);
    CHECK(B->add_siege == 2);
    CHECK(!strcmp(B->description, "+2 Siege II"));
}

/* ── 2. Descriptions consume the rest of the line, including embedded
 *      spaces. Single-token values still tokenize cleanly. ──────────── */
static void test_description_multi_word(void) {
    g_test = "description_multi_word";
    const char *src =
        "upgrade FOO\n"
        "  cost 10\n"
        "  description  hello   world   from creeps  \n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    /* Leading whitespace skipped, trailing trimmed; interior preserved. */
    CHECK(!strcmp(u->description, "hello   world   from creeps"));
}

/* ── 3. Comments, blank lines, and indentation are all ignored. ───── */
static void test_whitespace_and_comments(void) {
    g_test = "whitespace_and_comments";
    const char *src =
        "# leading comment\n"
        "\n"
        "upgrade GAMMA     # inline comment\n"
        "      cost   33\n"
        "\tresearch_turns\t2\n"
        "  # commented-out: cost 999\n"
        "  add_siege 4\n";
    CHECK(creep_config_load_from_string(src) == 0);
    int g = creep_config_lookup("GAMMA");
    CHECK(g >= 0);
    const CreepUpgradeConfig *G = &creep_config_get()->upgrades[g];
    CHECK(G->cost == 33);          /* not 999 — that line is a comment */
    CHECK(G->research_turns == 2);
    CHECK(G->add_siege == 4);
}

/* ── 4. Unspecified keys default to 0. ────────────────────────────── */
static void test_defaults(void) {
    g_test = "defaults";
    const char *src =
        "upgrade ZERO\n"
        "  cost 5\n";
    CHECK(creep_config_load_from_string(src) == 0);
    const CreepUpgradeConfig *u = &creep_config_get()->upgrades[0];
    CHECK(u->cost == 5);
    CHECK(u->research_turns == 0);
    CHECK(u->add_retrievers == 0);
    CHECK(u->add_siege == 0);
    CHECK(u->description[0] == 0);
}

/* ── 5. Structural error cases reject loudly. ─────────────────────── */
static void test_error_cases(void) {
    g_test = "error_cases";
    /* Baseline: a minimal valid config. */
    CHECK(creep_config_load_from_string("upgrade X\n  cost 1\n") == 0);

    /* Key with no enclosing upgrade section. */
    CHECK(creep_config_load_from_string("cost 10\n") != 0);

    /* Unknown key inside an upgrade section. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  fnord 1\n") != 0);

    /* Non-integer where an int is expected. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost abc\n") != 0);

    /* `upgrade` directive with no id. */
    CHECK(creep_config_load_from_string("upgrade\n") != 0);

    /* Duplicate upgrade id. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost 1\n"
              "upgrade X\n  cost 2\n") != 0);

    /* Key present but value missing. */
    CHECK(creep_config_load_from_string(
              "upgrade X\n  cost\n") != 0);
}

/* ── 6. Declaration order is preserved as catalog index order. ────── */
static void test_declaration_order(void) {
    g_test = "declaration_order";
    const char *src =
        "upgrade ZULU\n  cost 1\n"
        "upgrade ALPHA\n  cost 1\n"
        "upgrade MIKE\n  cost 1\n";
    CHECK(creep_config_load_from_string(src) == 0);
    CHECK(creep_config_get()->count == 3);
    CHECK(creep_config_lookup("ZULU") == 0);
    CHECK(creep_config_lookup("ALPHA") == 1);
    CHECK(creep_config_lookup("MIKE") == 2);
}

int main(void) {
    test_basic_parse();
    test_description_multi_word();
    test_whitespace_and_comments();
    test_defaults();
    test_error_cases();
    test_declaration_order();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
