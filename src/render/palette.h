#ifndef PALETTE_H
#define PALETTE_H

/* Centralized 0xRRGGBB color palette for the render layer. Group by
 * purpose, name by purpose — pick the constant whose name describes
 * what the pixel represents, not what shade it is. */

/* === Canvas / sidebar chrome === */
#define CANVAS_BG                  0x111111
#define SIDEBAR_BG                 0x1C1C1C
#define SIDEBAR_BORDER             0x444444

/* === Grid === */
#define ZONE_RED_BG                0x1A0808
#define ZONE_BLUE_BG               0x08081A
#define ZONE_DEBRIS_BG             0x222222
#define ZONE_NEUTRAL_BG            0x141414
#define GRID_LINE                  0x303030

/* === Per-player entity colors ===
 * LIVE       = entity currently in the viewer's vision.
 * FOG_OF_WAR = entity drawn from the viewer's fog memory — also reused
 *              as the dim shade for "heavy" melee creeps and for corpse
 *              markers (stylistic darker tone, not literal fog).        */
#define RED_LIVE                   0xCC4444
#define RED_FOG_OF_WAR             0x661818
#define BLUE_LIVE                  0x4477CC
#define BLUE_FOG_OF_WAR            0x182455

/* === Tower HP bar === */
#define HP_BAR_BG                  0x440000
#define HP_BAR_FILL                0x44CC44

/* === Tower attack beam === */
#define TOWER_BEAM                 0xFFEE88

/* === Creep slow-effect halo === */
#define CREEP_SLOW_HALO            0x88CCFF

/* === Selection highlight === */
#define SELECTION_HIGHLIGHT        0xFFFFFF

/* === Per-cell crowding badge === */
#define CROWDING_BADGE_BG          0x000000

/* === Placement-intent banner === */
#define PLACEMENT_BANNER_BG        0x222244
#define PLACEMENT_BANNER_TEXT      0xFFEE88

/* === Button states === */
#define BUTTON_ACTIVE_FILL         0x445588
#define BUTTON_ENABLED_FILL        0x2A2A2A
#define BUTTON_DISABLED_FILL       0x1A1A1A
#define BUTTON_ENABLED_BORDER      0x888888
#define BUTTON_DISABLED_BORDER     0x444444
#define BUTTON_ENABLED_TEXT        0xEEEEEE
#define BUTTON_DISABLED_TEXT       0x666666

/* === Sidebar text === */
#define TEXT_PRIMARY               0xEEEEEE  /* TURN heading, SEL line */
#define TEXT_SECONDARY             0xCCCCCC  /* default body text, default phase label */
#define TEXT_SECTION_HEADER        0xBBBBBB  /* "PLACE TOWER", "CREEP UPGRADES" */
#define TEXT_MUTED                 0x999999  /* selected tower HP line */
#define TEXT_FAINT                 0x888888  /* frame stats overlay */

/* === Phase indicator label === */
#define PHASE_LABEL_HIGHLIGHT      0xFFCC44  /* CHOOSE VIEW / SIMULATING */
#define PHASE_LABEL_GAME_OVER      0xFFFFFF

/* === Current-player sidebar block highlight === */
#define CURRENT_PLAYER_HIGHLIGHT   0x2A2A40

/* === Creep upgrade card states === */
#define UPGRADE_READY_BG           0x183018
#define UPGRADE_READY_TEXT         0x66CC66
#define UPGRADE_PURCHASED_BG       0x252535
#define UPGRADE_PURCHASED_TEXT     0xAAAACC

/* === Status banner === */
#define STATUS_MSG_BG              0x402810
#define STATUS_MSG_TEXT            0xFFCC66

#endif
