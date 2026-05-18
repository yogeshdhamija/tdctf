#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_GRID_W 40
#define MAX_GRID_H 30
#define MAX_THINGS 400
#define MAX_CREEP_UPGRADES 8

typedef enum { PLAYER_RED = 0, PLAYER_BLUE = 1 } PlayerID;
typedef enum { PHASE_PLAN_RED, PHASE_PLAN_BLUE, PHASE_SIMULATE, PHASE_GAME_OVER } Phase;

typedef enum { THING_NONE = 0, THING_TOWER, THING_CREEP } ThingType;

typedef enum {
    TOWER_GUNNER,
    TOWER_SLAMMER,
    TOWER_RESOURCE
} TowerType;

typedef enum {
    CREEP_RETRIEVER,
    CREEP_SIEGE
} CreepType;

typedef enum { ZONE_NEUTRAL, ZONE_RED, ZONE_BLUE } ZoneType;

typedef struct {
    ThingType tag;
    PlayerID  owner;
    int       x, y;
    int       hp, max_hp;
    int       alive;
    union {
        struct {
            TowerType type;
            int       level;
            int       build_turns;
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
    int thing_id;
} Cell;

typedef struct {
    int   id;
    int   cost;
    int   research_turns;
    int   turns_remaining;
    int   purchased;
    int   completed;
    char  description[64];
} CreepUpgrade;

typedef struct {
    int           resources;
    int           income_per_turn;
    CreepUpgrade  creep_upgrades[MAX_CREEP_UPGRADES];
    int           creep_upgrade_count;
} Player;

typedef struct {
    int      x, y;
    PlayerID owner;
    int      carried_by;
    int      at_home;
} Flag;

typedef struct {
    Cell    grid[MAX_GRID_W][MAX_GRID_H];
    int     grid_w, grid_h;
    Thing   things[MAX_THINGS];
    int     thing_count;
    Player  players[2];
    Flag    flags[2];
    Phase   phase;
    int     turn;
    int     receptacle_x[2], receptacle_y[2];
    int     path_x[2][64], path_y[2][64];
    int     path_len[2];
} GameState;

void             game_init(void);
const GameState *game_get_state(void);

#endif
