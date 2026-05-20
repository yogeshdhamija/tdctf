#ifndef CREEP_CONFIG_H
#define CREEP_CONFIG_H

/* Creep upgrade catalog loaded from data/creep_upgrades.cfg. The default
 * config is embedded into the binary at build time (see Makefile +
 * creep_config_data.h). Same shape as tower_config: upgrades are declared
 * in the cfg file and assigned runtime ids in declaration order.
 *
 * Each player's runtime state (purchased / completed / turns_remaining) is
 * stored separately on the Player struct in game.h, indexed parallel to
 * the catalog. The catalog itself is global / static. */

#define CREEP_UPGRADE_MAX_COUNT 8
#define CREEP_UPGRADE_ID_MAX    24
#define CREEP_UPGRADE_DESC_MAX  64

typedef struct {
    char id[CREEP_UPGRADE_ID_MAX];    /* config identifier, e.g. "RETRIEVER_1" */
    int  cost;                        /* resources paid to start research */
    int  research_turns;              /* turns of research before activation */
    int  add_retrievers;              /* retrievers spawned per turn once completed */
    int  add_siege;                   /* siege creeps spawned per turn once completed */
    char description[CREEP_UPGRADE_DESC_MAX];  /* sidebar label; may contain spaces */
} CreepUpgradeConfig;

typedef struct {
    int                count;
    CreepUpgradeConfig upgrades[CREEP_UPGRADE_MAX_COUNT];
} CreepUpgradeCatalog;

const CreepUpgradeCatalog *creep_config_get(void);
int  creep_config_load_default(void);
int  creep_config_load_from_string(const char *src);

/* Index of the upgrade with the given config id, or -1 if not found. */
int  creep_config_lookup(const char *id);

#endif
