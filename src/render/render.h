#ifndef RENDER_H
#define RENDER_H

#include "../game/game.h"

/* Fixed button IDs. Tower-placement buttons are NOT enumerated here — they
 * are assigned dynamically as BTN_PLACE_TOWER_BASE + <tower index>, so the
 * palette resizes with the tower catalog. */
typedef enum {
    BTN_NONE = 0,
    BTN_LOCK_IN,
    BTN_UPGRADE_TOWER,
    BTN_DESTROY_TOWER,
    BTN_BUY_UPGRADE_0,
    BTN_BUY_UPGRADE_1,
    BTN_BUY_UPGRADE_2,
    BTN_BUY_UPGRADE_3,
    BTN_RESTART,
    BTN_PLACE_TOWER_BASE = 1000
} ButtonID;

#define CELL_SIZE 32
#define SIDEBAR_W 220

void render_frame(const GameState *state);
int  render_button_at(int px, int py); /* returns ButtonID or BTN_NONE */

#endif
