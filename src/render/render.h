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
    BTN_START_SIM_AS_RED,
    BTN_START_SIM_AS_BLUE,
    BTN_CONTINUE_TO_NEXT_TURN,
    BTN_BUY_UPGRADE_BASE = 500,
    BTN_PLACE_TOWER_BASE = 1000
} ButtonID;

#define CELL_SIZE 32
#define SIDEBAR_W 220
/* Reserved row above the grid for the placement-intent banner. The grid is
 * shifted down by this amount so the banner never overlaps the first row. */
#define BANNER_H 20
/* Floor on the canvas (and sidebar) height so the sidebar's full button
 * stack — tower palette, creep upgrades, selected-tower controls — never
 * gets cut off when the map has few rows. Sized for the worst-case PLAN
 * layout with the shipped catalogs (~660 px of content) plus headroom
 * for the status-message overlay and frame-stats line at the bottom. */
#define SIDEBAR_MIN_H 720

void render_frame(const GameState *state);
int  render_button_at(int px, int py); /* returns ButtonID or BTN_NONE */

#endif
