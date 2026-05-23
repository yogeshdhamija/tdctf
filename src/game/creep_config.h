#ifndef CREEP_CONFIG_H
#define CREEP_CONFIG_H

/* Creep type & creep upgrade catalog loaded from data/creep_upgrades.cfg.
 * The default config is embedded into the binary at build time (see
 * Makefile + creep_config_data.h). Parallel to tower_config: entries are
 * declared in the cfg file and assigned runtime indices in declaration
 * order.
 *
 * Creep types describe a creep's *behavior*: HP, whether it picks up
 * flags, how much damage it does to adjacent enemy towers each tick, and
 * its display code. A single creep type can combine any subset of these
 * behaviors — a "BANANA" that both carries flags AND damages towers is a
 * normal config, not a special case.
 *
 * Upgrades reference creep types through the `spawn` directive: after
 * research completes, the upgrade spawns `spawn_count` creeps of type
 * `spawn_type` at the player's spawn cell each turn. `spawn_count == 0`
 * means "no creeps from this upgrade" (e.g. for future stat buffs).
 *
 * Each player's runtime state (purchased / completed / turns_remaining)
 * is stored separately on the Player struct in game.h, indexed parallel
 * to the upgrades array. */

#define CREEP_TYPE_MAX_COUNT    8
#define CREEP_TYPE_ID_MAX       24
#define CREEP_UPGRADE_MAX_COUNT 8
#define CREEP_UPGRADE_ID_MAX    24
#define CREEP_UPGRADE_DESC_MAX  64

typedef struct {
    char id[CREEP_TYPE_ID_MAX];       /* config identifier, e.g. "RETRIEVER" */
    char code;                        /* one-char glyph for the crowding badge */
    int  hp;                          /* HP at spawn */
    int  can_carry_flag;              /* 0/1: picks up enemy flag on contact */
    int  melee_damage;                /* damage per tick to any adjacent enemy tower (0 = no melee) */
    int  spawn_order;                 /* sort key for the per-turn spawn queue; lower spawns first, ties keep declaration order */
} CreepTypeConfig;

typedef struct {
    char id[CREEP_UPGRADE_ID_MAX];           /* config identifier, e.g. "BANANA_UPG" */
    int  cost;                               /* resources paid to start research */
    int  research_turns;                     /* turns of research before activation */
    int  spawn_type;                         /* index into types[], or -1 if no spawn directive */
    int  spawn_count;                        /* creeps spawned per turn once completed */
    char description[CREEP_UPGRADE_DESC_MAX];/* sidebar label; may contain spaces */
} CreepUpgradeConfig;

typedef struct {
    int                type_count;
    CreepTypeConfig    types[CREEP_TYPE_MAX_COUNT];
    int                upgrade_count;
    CreepUpgradeConfig upgrades[CREEP_UPGRADE_MAX_COUNT];
} CreepCatalog;

const CreepCatalog *creep_config_get(void);
int  creep_config_load_default(void);
int  creep_config_load_from_string(const char *src);

/* Index resolvers; -1 if not found. */
int  creep_config_lookup_type(const char *id);
int  creep_config_lookup_upgrade(const char *id);

#endif
