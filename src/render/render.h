#ifndef RENDER_H
#define RENDER_H

#include "../game/game.h"

/* Fixed button IDs. Tower-placement and creep-upgrade buttons are NOT
 * enumerated here — they are assigned dynamically as
 * BTN_PLACE_TOWER_BASE + <tower index> and BTN_BUY_UPGRADE_BASE +
 * <upgrade index>, so each palette resizes with its catalog. */
typedef enum {
    BTN_NONE = 0,
    BTN_LOCK_IN,
    BTN_UPGRADE_TOWER,
    BTN_DESTROY_TOWER,
    BTN_RESTART,
    BTN_BUY_UPGRADE_BASE = 500,
    BTN_PLACE_TOWER_BASE = 1000
} ButtonID;

#define CELL_SIZE 32
#define SIDEBAR_W 220

void render_frame(const GameState *state);
int  render_button_at(int px, int py); /* returns ButtonID or BTN_NONE */

#endif
