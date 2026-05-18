#ifndef RENDER_H
#define RENDER_H

#include "../game/game.h"

typedef enum {
    BTN_NONE = 0,
    BTN_LOCK_IN,
    BTN_PLACE_GUNNER,
    BTN_PLACE_SLAMMER,
    BTN_PLACE_RESOURCE,
    BTN_UPGRADE_TOWER,
    BTN_DESTROY_TOWER,
    BTN_BUY_UPGRADE_0,
    BTN_BUY_UPGRADE_1,
    BTN_BUY_UPGRADE_2,
    BTN_BUY_UPGRADE_3,
    BTN_RESTART
} ButtonID;

#define CELL_SIZE 32
#define SIDEBAR_W 220

void render_frame(const GameState *state);
int  render_button_at(int px, int py); /* returns ButtonID or BTN_NONE */

#endif
