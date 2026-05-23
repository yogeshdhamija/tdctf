#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_GRID_W 40
#define MAX_GRID_H 30
#define MAX_THINGS 400
/* Sizes — match CREEP_UPGRADE_MAX_COUNT / CREEP_TYPE_MAX_COUNT in
 * creep_config.h. Kept as local defines so game.h doesn't need to pull
 * in the catalog header (mirrors how MAX_THINGS sizes the things array
 * without dragging tower_config.h into game.h). */
#define MAX_CREEP_UPGRADES 16
#define MAX_CREEP_TYPES    8
#define MAX_SPAWN_QUEUE 64
#define MAX_BEAMS 64
#define SIM_TICKS_PER_TURN 100
#define SIM_FRAMES_PER_TICK 8
#define SIM_END_HOLD_FRAMES 90

typedef enum { PLAYER_RED = 0, PLAYER_BLUE = 1 } PlayerID;
typedef enum { PHASE_PLAN_RED, PHASE_PLAN_BLUE, PHASE_SIMULATE, PHASE_GAME_OVER } Phase;

typedef enum { THING_NONE = 0, THING_TOWER, THING_CREEP } ThingType;

/* Tower types are runtime ids assigned by tower_config at parse time, in the
 * order towers appear in data/towers.cfg. Use game_tower_id("NAME") to
 * resolve a config id to its index, and game_tower_count() for the live
 * count. -1 means "no tower / unset". */
typedef int TowerType;

/* Creep types are runtime ids assigned by creep_config at parse time, in
 * the order types appear in data/creep_upgrades.cfg. Use
 * game_creep_type_id("NAME") to resolve a config id to its index. -1
 * means "no creep type / unset". */
typedef int CreepType;

typedef enum { ZONE_NEUTRAL, ZONE_RED, ZONE_BLUE, ZONE_DEBRIS } ZoneType;

typedef struct {
    ThingType tag;
    PlayerID  owner;
    int       x, y;
    int       hp, max_hp;
    int       alive;
    int       last_target_x, last_target_y; /* for tower beam visualization */
    int       beam_ttl;                     /* ticks remaining on beam */
    union {
        struct {
            TowerType type;
            int       level;
            int       build_turns;
            int       cooldown;
        } tower;
        struct {
            CreepType type;
            int       has_flag;
            int       visited_flag;
            int       slow_ticks;
        } creep;
    };
} Thing;

typedef struct {
    ZoneType zone;
    int      thing_id;
} Cell;

/* Per-player runtime state for one creep upgrade. The static spec
 * (cost, research_turns, creep_type, count, code, hp, can_carry_flag,
 * melee_damage, spawn_order, description, set_flags) lives in the
 * catalog at data/creep_upgrades.cfg, accessed via game_creep_upgrade_*
 * accessors. The array index into Player.creep_upgrades matches the
 * catalog upgrade index. */
typedef struct {
    int turns_remaining;
    int purchased;
    int completed;
} CreepUpgrade;

/* Merged creep profile for one (player, creep type). Built fresh at
 * start_simulation() by walking the player's completed upgrades in
 * catalog order and overlaying each upgrade's *set* fields. `active`
 * goes to 1 the first time any upgrade overlays anything for this
 * type — `count == 0` is then a legitimate "stat-buff only" upgrade,
 * not "no profile here". */
typedef struct {
    int  active;
    int  count;
    char code;
    int  hp;
    int  can_carry_flag;
    int  melee_damage;
    int  spawn_order;
} ActiveCreepProfile;

typedef struct {
    int                resources;
    int                income_per_turn;
    CreepUpgrade       creep_upgrades[MAX_CREEP_UPGRADES];
    int                creep_upgrade_count;
    int                pending_place_x, pending_place_y; /* RED-only placement queued for conflict resolve */
    int                pending_place_type;               /* -1 if no pending placement */
    /* Merged creep profiles, indexed by CreepType. Recomputed at every
     * start_simulation() from the player's completed-upgrade overlay. */
    ActiveCreepProfile active_creeps[MAX_CREEP_TYPES];
    /* Spawn queue: a flat list of CreepType indices. One entry is drained
     * at the start of each sim tick into a spawn at spawn_x/spawn_y; the
     * spawn reads its profile from active_creeps[type]. Built fresh in
     * start_simulation() — for each type with an active merged profile,
     * push `count` copies and then sort by spawn_order. Reset on every
     * sim. */
    CreepType          spawn_queue[MAX_SPAWN_QUEUE];
    int                spawn_queue_count;
    int                spawn_queue_pos;
} Player;

typedef struct {
    int      x, y;
    PlayerID owner;
    int      carried_by;
    int      at_home;
} Flag;

typedef struct {
    int x, y;
} Point;

typedef struct {
    Cell   grid[MAX_GRID_W][MAX_GRID_H];
    int    grid_w, grid_h;
    Thing  things[MAX_THINGS];
    int    thing_count;
    Player players[2];
    Flag   flags[2];
    Phase  phase;
    int    turn;
    int    spawn_x[2], spawn_y[2];
    int    receptacle_x[2], receptacle_y[2];

    /* MVP additions */
    int      selected_x, selected_y;          /* -1 if none */
    int      placement_intent;                /* -1 or TowerType */
    int      sim_tick;
    int      sim_frame_accum;
    int      sim_end_hold;
    int      winner;                          /* -1 or PlayerID */
    char     status_msg[96];
    int      status_ttl;
    uint32_t rng_state;                       /* xorshift32 PRNG; snapshotted */
} GameState;

/* Lifecycle */
void             game_init(void);                           /* loads embedded data/towers.cfg + data/creep_upgrades.cfg */
void             game_init_with_tower_config(const char *cfg); /* test hook: load `cfg` instead of the default towers; creep upgrades use the default */
void             game_init_with_creep_config(const char *cfg); /* test hook: load `cfg` instead of the default creep upgrades; towers use the default */
void             game_init_with_configs(const char *tower_cfg, const char *creep_cfg); /* test hook: pin both catalogs from fixtures */
void             game_init_with_configs_and_map(const char *tower_cfg, const char *creep_cfg, const char *map_cfg); /* test hook: pin both catalogs AND the map from fixtures */
void             game_frame(void);                          /* advance one frame (~60Hz) */
const GameState *game_get_state(void);

/* Input — only valid in planning phases */
void             game_grid_click(int gx, int gy);
void             game_set_placement(int tower_type);        /* TowerType or -1 */
void             game_upgrade_selected(void);
void             game_destroy_selected(void);
void             game_buy_creep_upgrade(int idx);
void             game_lock_in(void);

/* Helpers for render layer */
int              game_tower_count(void);                    /* live tower count from catalog */
int              game_tower_id(const char *name);           /* catalog id -> index, or -1 */
int              game_tower_cost(TowerType t);              /* placement cost (level 1) */
int              game_tower_upgrade_cost(TowerType t, int from_level); /* cost of from_level -> from_level+1; 0 if at max */
int              game_tower_build_turns(TowerType t);
int              game_tower_upgrade_turns(TowerType t, int from_level);
int              game_tower_max_level(TowerType t);         /* number of levels defined */
const char      *game_tower_name(TowerType t);
char             game_tower_code(TowerType t);

/* Creep type catalog accessors. CreepType is an index into the catalog
 * at data/creep_upgrades.cfg. Types are bare identifiers — all behavior
 * (code, hp, can_carry_flag, melee_damage, spawn_order, count) is
 * defined by the upgrades that target the type and merged per-player at
 * sim start. */
int              game_creep_type_count(void);
int              game_creep_type_id(const char *name);     /* -1 if not found */

/* Live merged creep profile for a (player, creep type) pair. Reads
 * Player.active_creeps[t] — what was overlaid from completed upgrades at
 * the most recent start_simulation. game_creep_is_active returns 0 when
 * no completed upgrade has touched this type for this player; the
 * per-field accessors then return 0 / '\0'. */
int              game_creep_is_active(PlayerID owner, CreepType t);
int              game_creep_active_count(PlayerID owner, CreepType t);
char             game_creep_active_code(PlayerID owner, CreepType t);
int              game_creep_active_hp(PlayerID owner, CreepType t);
int              game_creep_active_can_carry_flag(PlayerID owner, CreepType t);
int              game_creep_active_melee_damage(PlayerID owner, CreepType t);
int              game_creep_active_spawn_order(PlayerID owner, CreepType t);

/* Creep upgrade catalog accessors. Per-player dynamic state (purchased /
 * completed / turns_remaining) lives on Player.creep_upgrades[idx]. */
int              game_creep_upgrade_total(void);            /* count of upgrades in catalog */
int              game_creep_upgrade_cost(int idx);
int              game_creep_upgrade_research_turns(int idx);
const char      *game_creep_upgrade_description(int idx);
int              game_creep_upgrade_creep_type(int idx);    /* target CreepType, or -1 */
int              game_creep_upgrade_requires(int idx);       /* prerequisite upgrade index, or -1 */

PlayerID         game_planning_player(void);                /* meaningful only in planning */

/* Pathing primitive exposed for testing at the layer of abstraction of the
 * pathing logic itself (not the full sim). Per docs/game-design.md §10:
 * each creep takes the shortest unblocked path to the enemy flag's current
 * cell (until it touches the flag), then to its own receptacle. When
 * multiple shortest paths exist, the first step is chosen uniformly at
 * random among them ("pathing wobble"), consuming one xorshift32 roll from
 * gs->rng_state. With only one shortest first step (or none), no RNG is
 * consumed. gs is non-const because of that side effect. */
int              game_pathing_next_step(GameState *gs,
                                        int creep_x, int creep_y,
                                        PlayerID owner, int visited_flag,
                                        int *out_x, int *out_y);

/* Snapshot save/resume.
 *
 * The encoded string is a URL-safe, plain-text representation of enough
 * game state to resume play: turn, phase, per-player resources/income, all
 * purchased creep upgrades (referenced by config ID), live towers
 * (referenced by config ID), and both flags' positions. It deliberately
 * omits anything derivable from the configs (tower stats, creep spawn
 * counts) and anything transient (creeps mid-sim, sim_tick, beam/cooldown,
 * UI selection state).
 *
 * Loading silently drops references that don't resolve against the current
 * catalogs (renamed/removed tower or upgrade IDs) or positions that no
 * longer fit the current map (out of bounds, in debris). This is the
 * "configs may have changed in a non-conflicting way" property — the
 * snapshot keeps working when the cfgs are edited, as long as the IDs
 * still exist and the map still has the cells.
 *
 * Encoding format (single line, no whitespace):
 *   v1~T<turn>~P<phase>~R<res>:<inc>~r<upgs>~B<res>:<inc>~b<upgs>
 *      ~W<towers>~F<rx>:<ry>:<rhome>:<bx>:<by>:<bhome>~N<rng>
 * where:
 *   <phase>  one of R / B / S / O   (PLAN_RED / PLAN_BLUE / SIMULATE / GAME_OVER)
 *   <upgs>   comma-separated  <id>:<purchased>:<completed>:<turns_remaining>
 *   <towers> comma-separated  <R|B>:<id>:<x>:<y>:<level>:<hp>:<build_turns>
 *   <rng>    decimal uint32 PRNG state — preserved so resumed sims reproduce
 *            the same crit/random rolls as the original timeline. Omitted in
 *            older snapshots; missing N leaves the fresh-init seed in place.
 */
int              game_snapshot_encode(char *out, int out_size);
int              game_snapshot_load(const char *src);

#endif
