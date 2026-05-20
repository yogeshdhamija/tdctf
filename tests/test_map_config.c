#include "game/map_config.h"
#include <stdio.h>
#include <string.h>

/* Parser tests at the map_config layer. Sub-behaviors (per-symbol cell
 * classification, comment stripping, error returns, landmark uniqueness)
 * are exercised here directly so the higher-layer game tests don't have
 * to. */

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

/* A tiny but complete map. Every required landmark appears exactly once. */
#define TINY_MAP                                                            \
    "grid\n"                                                                \
    "RRRR..BBBB\n"                                                          \
    "R[RR..B]BB\n"                                                          \
    "Rr..1.2bBB\n"                                                          \
    "RRRR..BBBB\n"

/* ── 1. A canonical map parses every cell and landmark cleanly. ──────── */
static void test_basic_parse(void) {
    g_test = "basic_parse";
    CHECK(map_config_load_from_string(TINY_MAP) == 0);
    const MapConfig *m = map_config_get();
    CHECK(m->width == 10);
    CHECK(m->height == 4);

    /* Zone sampling: corners + landmark cells. */
    CHECK(m->zones[0][0] == MAP_ZONE_RED);
    CHECK(m->zones[4][0] == MAP_ZONE_NEUTRAL);
    CHECK(m->zones[6][0] == MAP_ZONE_BLUE);
    CHECK(m->zones[1][1] == MAP_ZONE_RED);   /* '[' on a red cell */
    CHECK(m->zones[7][1] == MAP_ZONE_BLUE);  /* ']' on a blue cell */
    CHECK(m->zones[1][2] == MAP_ZONE_RED);   /* 'r' on a red cell */
    CHECK(m->zones[7][2] == MAP_ZONE_BLUE);  /* 'b' on a blue cell */
    CHECK(m->zones[4][2] == MAP_ZONE_NEUTRAL); /* '1' = neutral */
    CHECK(m->zones[6][2] == MAP_ZONE_NEUTRAL); /* '2' = neutral */

    CHECK(m->red_recep_x  == 1 && m->red_recep_y  == 1);
    CHECK(m->blue_recep_x == 7 && m->blue_recep_y == 1);
    CHECK(m->red_flag_x   == 1 && m->red_flag_y   == 2);
    CHECK(m->blue_flag_x  == 7 && m->blue_flag_y  == 2);
    CHECK(m->red_spawn_x  == 4 && m->red_spawn_y  == 2);
    CHECK(m->blue_spawn_x == 6 && m->blue_spawn_y == 2);
}

/* ── 2. Debris cells parse and survive in zones[][]. ─────────────────── */
static void test_debris(void) {
    g_test = "debris";
    const char *src =
        "grid\n"
        "[r12]bX..\n";
    CHECK(map_config_load_from_string(src) == 0);
    const MapConfig *m = map_config_get();
    CHECK(m->zones[6][0] == MAP_ZONE_DEBRIS);
    CHECK(m->zones[7][0] == MAP_ZONE_NEUTRAL);
    CHECK(m->width == 9);
    CHECK(m->height == 1);
}

/* ── 3. Blank lines + comments before/inside the grid are ignored. ───── */
static void test_whitespace_and_comments(void) {
    g_test = "whitespace_and_comments";
    const char *src =
        "# leading comment\n"
        "\n"
        "grid    # the grid begins now\n"
        "[r12]b....\n"
        "\n"
        "..........\n"
        "  # commented row\n"
        "..........\n";
    CHECK(map_config_load_from_string(src) == 0);
    const MapConfig *m = map_config_get();
    CHECK(m->width == 10);
    CHECK(m->height == 3);
}

/* ── 4. Missing any one of the six landmarks is an error. ────────────── */
static void test_missing_landmark(void) {
    g_test = "missing_landmark";
    /* Drop the blue flag. */
    const char *src =
        "grid\n"
        "RRRR..BBBB\n"
        "R[RR..B]BB\n"
        "Rr..1.2.BB\n";
    CHECK(map_config_load_from_string(src) != 0);
}

/* ── 5. A duplicate landmark is rejected. ────────────────────────────── */
static void test_duplicate_landmark(void) {
    g_test = "duplicate_landmark";
    const char *src =
        "grid\n"
        "RRRR..BBBB\n"
        "R[RR..B]BB\n"
        "Rr..1.2bBB\n"
        "Rr..R.BBBB\n";   /* second 'r' */
    CHECK(map_config_load_from_string(src) != 0);
}

/* ── 6. Inconsistent row width is an error. ──────────────────────────── */
static void test_inconsistent_width(void) {
    g_test = "inconsistent_width";
    const char *src =
        "grid\n"
        "RRRR..BBBB\n"
        "R[RR..B]B\n"          /* one cell short */
        "Rr..1.2bBB\n";
    CHECK(map_config_load_from_string(src) != 0);
}

/* ── 7. Unknown cell symbol is an error. ─────────────────────────────── */
static void test_unknown_symbol(void) {
    g_test = "unknown_symbol";
    const char *src =
        "grid\n"
        "RRRR?.BBBB\n"           /* '?' is not a defined symbol */
        "R[RR..B]BB\n"
        "Rr..1.2bBB\n";
    CHECK(map_config_load_from_string(src) != 0);
}

/* ── 8. Missing `grid` keyword leaves the parser with no rows. ───────── */
static void test_no_grid_keyword(void) {
    g_test = "no_grid_keyword";
    /* No `grid`, but a stray non-blank, non-comment line — should fail. */
    CHECK(map_config_load_from_string("hello\n") != 0);
    /* Truly empty input — no grid means no map. */
    CHECK(map_config_load_from_string("") != 0);
    /* Just `grid` with no rows underneath — still no map. */
    CHECK(map_config_load_from_string("grid\n") != 0);
}

int main(void) {
    test_basic_parse();
    test_debris();
    test_whitespace_and_comments();
    test_missing_landmark();
    test_duplicate_landmark();
    test_inconsistent_width();
    test_unknown_symbol();
    test_no_grid_keyword();
    printf("%d assertions, %d failures\n", g_assertions, g_fail);
    return g_fail ? 1 : 0;
}
