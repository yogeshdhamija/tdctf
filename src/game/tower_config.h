#ifndef TOWER_CONFIG_H
#define TOWER_CONFIG_H

/* Tower stats loaded from data/towers.cfg. The default config is embedded
 * into the binary at build time (see Makefile + tower_config_data.h). Both
 * the WASM build and native tests see exactly the same bytes.
 *
 * Tower types are NOT hardcoded. Towers are declared in the config and
 * assigned runtime ids in declaration order. The catalog's `count` field
 * is the live tower count; tower_config_lookup() resolves a config id
 * (e.g. "GUNNER") to its index.
 *
 * Levels are independent: every level redefines cost, hp, build_turns,
 * and all combat/income stats. There is no "delta from previous level"
 * semantics — each level is a complete spec. */

#define TOWER_MAX_COUNT  16
#define TOWER_MAX_LEVELS 8
#define TOWER_NAME_MAX   16
#define TOWER_ID_MAX     16

typedef struct {
    int cost;          /* paid to ENTER this level: placement cost for L1,
                        * upgrade cost from L(N-1) for L(N>1). */
    int hp;            /* absolute max HP at this level. */
    int build_turns;   /* construction time when entering this level. */
    int dmg;
    int range;
    int aoe;
    int slow;
    int cooldown;
    int income;        /* per-turn resource generation while at this level. */
    int crit_chance;   /* percent (0-100) chance of dealing `crit_dmg` instead
                        * of `dmg`. Rolled once per attack — AoE attacks apply
                        * the rolled damage uniformly to all hit targets. */
    int crit_dmg;      /* damage substituted for `dmg` when the crit roll
                        * succeeds. Ignored when crit_chance is 0. */
} TowerLevelStats;

typedef struct {
    char            id[TOWER_ID_MAX];        /* config identifier, e.g. "GUNNER" */
    char            name[TOWER_NAME_MAX];    /* display name */
    char            code;                    /* one-char glyph drawn on the grid */
    int             level_count;             /* number of defined levels (>= 1) */
    TowerLevelStats level[TOWER_MAX_LEVELS];
} TowerConfig;

typedef struct {
    int         count;                       /* number of towers defined */
    TowerConfig towers[TOWER_MAX_COUNT];
} TowerCatalog;

const TowerCatalog *tower_config_get(void);
int  tower_config_load_default(void);
int  tower_config_load_from_string(const char *src);

/* Returns the index of the tower with the given config id, or -1 if not
 * found. Useful for tests that want to refer to a specific tower by name. */
int  tower_config_lookup(const char *id);

#endif
