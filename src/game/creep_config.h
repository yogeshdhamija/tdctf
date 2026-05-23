#ifndef CREEP_CONFIG_H
#define CREEP_CONFIG_H

/* Creep type & creep upgrade catalog loaded from data/creep_upgrades.cfg.
 * The default config is embedded into the binary at build time (see
 * Makefile + creep_config_data.h). Parallel to tower_config: entries are
 * declared in the cfg file and assigned runtime indices in declaration
 * order.
 *
 * Creep types are bare identifiers — CreepTypeConfig holds only an id.
 * All creep behavior (HP, code glyph, can_carry_flag, melee_damage,
 * spawn_order, count) lives on CreepUpgradeConfig.
 *
 * Merge semantic (resolved at sim-start in game.c): for each
 * (player, creep type) pair, the active profile is the result of
 * overlaying every completed upgrade targeting that type, in
 * declaration order — each upgrade contributes only the fields it
 * *explicitly set*. Two upgrades for the same type with disjoint fields
 * combine ("hp 25, melee_damage 5" + "hp 50" → "hp 50, melee_damage 5").
 * Two upgrades that set the same field: the later-declared wins for
 * that field. `set_flags` is the bitmask of which fields the parser
 * observed in the cfg, used to drive the overlay.
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
} CreepTypeConfig;

/* Bits in CreepUpgradeConfig.set_flags — one per mergable creep-profile
 * field. The parser sets the bit when it sees the cfg directive. The
 * merge walk in game.c reads these to know which fields to overlay. */
#define CREEP_UPG_SET_COUNT          (1u << 0)
#define CREEP_UPG_SET_CODE           (1u << 1)
#define CREEP_UPG_SET_HP             (1u << 2)
#define CREEP_UPG_SET_CAN_CARRY_FLAG (1u << 3)
#define CREEP_UPG_SET_MELEE_DAMAGE   (1u << 4)
#define CREEP_UPG_SET_SPAWN_ORDER    (1u << 5)

typedef struct {
    char id[CREEP_UPGRADE_ID_MAX];           /* config identifier, e.g. "RECRUIT" */
    int  cost;                               /* resources paid to start research */
    int  research_turns;                     /* turns of research before activation */
    int  creep_type;                         /* index into types[], or -1 if no creep directive */
    int  count;                              /* creeps spawned per turn once completed */
    char code;                               /* one-char glyph for the crowding badge */
    int  hp;                                 /* HP a spawned creep starts with */
    int  can_carry_flag;                     /* 0/1: picks up enemy flag on contact */
    int  melee_damage;                       /* damage per tick to any adjacent enemy tower (0 = no melee) */
    int  spawn_order;                        /* sort key for the per-turn spawn queue; lower spawns first */
    char description[CREEP_UPGRADE_DESC_MAX];/* sidebar label; may contain spaces */
    unsigned int set_flags;                  /* bitmask: which of the above were explicitly set in the cfg */
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
