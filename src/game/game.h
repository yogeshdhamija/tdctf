#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_GRID_W 40
#define MAX_GRID_H 30
#define MAX_THINGS 400
#define MAX_CREEP_UPGRADES 8
#define MAX_BEAMS 64
#define SIM_TICKS_PER_TURN 300
#define SIM_FRAMES_PER_TICK 30
#define SIM_END_HOLD_FRAMES 90

typedef enum { PLAYER_RED = 0, PLAYER_BLUE = 1 } PlayerID;
typedef enum { PHASE_PLAN_RED, PHASE_PLAN_BLUE, PHASE_SIMULATE, PHASE_GAME_OVER } Phase;

typedef enum { THING_NONE = 0, THING_TOWER, THING_CREEP } ThingType;

/* Tower types are runtime ids assigned by tower_config at parse time, in the
 * order towers appear in data/towers.cfg. Use game_tower_id("NAME") to
 * resolve a config id to its index, and game_tower_count() for the live
 * count. -1 means "no tower / unset". */
typedef int TowerType;

typedef enum {
    CREEP_RETRIEVER,
    CREEP_SIEGE,
    CREEP_TYPE_COUNT
} CreepType;

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

typedef struct {
    int  id;
    int  cost;
    int  research_turns;
    int  turns_remaining;
    int  purchased;
    int  completed;
    int  add_retrievers;
    int  add_siege;
    char description[64];
} CreepUpgrade;

typedef struct {
    int          resources;
    int          income_per_turn;
    CreepUpgrade creep_upgrades[MAX_CREEP_UPGRADES];
    int          creep_upgrade_count;
    int          pending_place_x, pending_place_y; /* RED-only placement queued for conflict resolve */
    int          pending_place_type;               /* -1 if no pending placement */
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
    int    selected_x, selected_y;          /* -1 if none */
    int    placement_intent;                /* -1 or TowerType */
    int    sim_tick;
    int    sim_frame_accum;
    int    sim_end_hold;
    int    winner;                          /* -1 or PlayerID */
    char   status_msg[96];
    int    status_ttl;
} GameState;

/* Lifecycle */
void             game_init(void);                           /* loads the embedded data/towers.cfg */
void             game_init_with_tower_config(const char *cfg); /* test hook: load `cfg` instead */
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
PlayerID         game_planning_player(void);                /* meaningful only in planning */

/* Pathing primitive exposed for testing at the layer of abstraction of the
 * pathing logic itself (not the full sim). Per docs/game-design.md §10:
 * each creep takes the shortest unblocked path to the enemy flag's current
 * cell (until it touches the flag), then to its own receptacle. BFS expands
 * horizontal neighbours before vertical, so on a tie the first step is the
 * horizontal one. */
int              game_pathing_next_step(const GameState *gs,
                                        int creep_x, int creep_y,
                                        PlayerID owner, int visited_flag,
                                        int *out_x, int *out_y);

#endif
