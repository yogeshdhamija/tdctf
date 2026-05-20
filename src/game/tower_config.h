#ifndef TOWER_CONFIG_H
#define TOWER_CONFIG_H

#include "game.h"

/* Tower stats loaded from data/towers.cfg. The default config is embedded
 * into the binary at build time (see Makefile + tower_config_data.h). Both
 * the WASM build and native tests see exactly the same bytes. */

#define TOWER_MAX_LEVELS 2
#define TOWER_NAME_MAX   16

typedef struct {
    int dmg;
    int range;
    int aoe;
    int slow;
    int cooldown;
    int income;
} TowerLevelStats;

typedef struct {
    int             cost;
    int             hp;
    int             build_turns;
    char            code;
    char            name[TOWER_NAME_MAX];
    int             upgrade_cost;
    int             upgrade_build;
    int             upgrade_hp_bonus;
    TowerLevelStats level[TOWER_MAX_LEVELS];
} TowerConfig;

typedef struct {
    TowerConfig towers[TOWER_TYPE_COUNT];
} TowerCatalog;

/* Returns the live catalog. Always non-NULL; values are whatever the most
 * recent successful tower_config_load_* call produced (or zeros before
 * any load). */
const TowerCatalog *tower_config_get(void);

/* Load the catalog from the embedded default config. Returns 0 on success.
 * Called by game_init(); tests can call it directly. */
int tower_config_load_default(void);

/* Load the catalog from an arbitrary string. Useful for tests that exercise
 * the parser. Returns 0 on success, nonzero on parse error. On failure the
 * catalog is left in an unspecified state — callers should reload defaults
 * or treat the catalog as invalid. */
int tower_config_load_from_string(const char *src);

#endif
