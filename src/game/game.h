#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_GRID_W 40
#define MAX_GRID_H 30
#define MAX_THINGS 400
#define MAX_CREEP_UPGRADES 8
#define MAX_BEAMS 64
#define MAX_PATH 64
#define SIM_TICKS_PER_TURN 300
#define SIM_FRAMES_PER_TICK 30
#define SIM_END_HOLD_FRAMES 90

typedef enum { PLAYER_RED = 0, PLAYER_BLUE = 1 } PlayerID;
typedef enum { PHASE_PLAN_RED, PHASE_PLAN_BLUE, PHASE_SIMULATE, PHASE_GAME_OVER } Phase;

typedef enum { THING_NONE = 0, THING_TOWER, THING_CREEP } ThingType;

typedef enum {
    TOWER_BLOCKER,
    TOWER_GUNNER,
    TOWER_SLAMMER,
    TOWER_RESOURCE,
    TOWER_TYPE_COUNT
} TowerType;

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
            int       path_progress;
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
    int    path_x[2][MAX_PATH], path_y[2][MAX_PATH];
    int    path_len[2];

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
void             game_init(void);
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
int              game_tower_cost(TowerType t);
const char      *game_tower_name(TowerType t);
char             game_tower_code(TowerType t);
PlayerID         game_planning_player(void);                /* meaningful only in planning */

/* Pathing primitives exposed for testing at the layer of abstraction of the
 * pathing logic itself (not the full sim). game_build_path lays out the line
 * spawn → enemy_flag → receptacle in gs->path_x/y[p]. game_pathing_next_step
 * applies the unified creep movement rule (docs/game-design.md §10): one step
 * toward the closest cell on the line that hasn't been visited yet, with the
 * enemy flag's current cell added as a goal when it's on the ground.
 * Excludes the creep's own cell from the goal set so the step always moves. */
void             game_build_path(GameState *gs, PlayerID p);
int              game_pathing_next_step(const GameState *gs,
                                        int creep_x, int creep_y,
                                        PlayerID owner, int path_progress,
                                        int *out_x, int *out_y);

#endif
