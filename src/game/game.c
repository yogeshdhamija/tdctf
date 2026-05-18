#include "game.h"
#include <string.h>

static GameState state;

void game_init(void) {
    memset(&state, 0, sizeof(state));
    state.grid_w = 30;
    state.grid_h = 20;
    state.turn = 1;
    state.phase = PHASE_PLAN_RED;

    for (int x = 0; x < state.grid_w; x++) {
        for (int y = 0; y < state.grid_h; y++) {
            state.grid[x][y].thing_id = -1;
            if (x < 10)      state.grid[x][y].zone = ZONE_RED;
            else if (x >= 20) state.grid[x][y].zone = ZONE_BLUE;
            else              state.grid[x][y].zone = ZONE_NEUTRAL;
        }
    }

    state.players[PLAYER_RED].resources = 100;
    state.players[PLAYER_RED].income_per_turn = 20;
    state.players[PLAYER_BLUE].resources = 100;
    state.players[PLAYER_BLUE].income_per_turn = 20;

    state.receptacle_x[PLAYER_RED] = 4;
    state.receptacle_y[PLAYER_RED] = 4;
    state.receptacle_x[PLAYER_BLUE] = 25;
    state.receptacle_y[PLAYER_BLUE] = 4;

    state.flags[PLAYER_RED].x = 4;
    state.flags[PLAYER_RED].y = 15;
    state.flags[PLAYER_RED].owner = PLAYER_RED;
    state.flags[PLAYER_RED].carried_by = -1;
    state.flags[PLAYER_RED].at_home = 1;

    state.flags[PLAYER_BLUE].x = 25;
    state.flags[PLAYER_BLUE].y = 15;
    state.flags[PLAYER_BLUE].owner = PLAYER_BLUE;
    state.flags[PLAYER_BLUE].carried_by = -1;
    state.flags[PLAYER_BLUE].at_home = 1;
}

const GameState *game_get_state(void) {
    return &state;
}
