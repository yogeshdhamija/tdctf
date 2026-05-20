#ifndef MAP_CONFIG_H
#define MAP_CONFIG_H

/* Map definition loaded from data/map.cfg. The default config is embedded
 * into the binary at build time (see Makefile + map_config_data.h). Both the
 * WASM build and native tests see exactly the same bytes.
 *
 * The map is a dot-matrix grid: each character of each row corresponds to
 * one cell. Every cell carries a zone (red, blue, neutral, debris), and a
 * cell may additionally be one of six unique landmarks: red/blue spawn,
 * receptacle, flag. The landmark's zone is fixed by symbol (see map.cfg).
 *
 * Map config defines geometry and starting positions; it does not depend on
 * game.h. The game layer maps these zones onto its own ZoneType enum and
 * copies landmark coordinates into GameState at init. */

#define MAP_MAX_W 40
#define MAP_MAX_H 30

typedef enum {
    MAP_ZONE_NEUTRAL = 0,
    MAP_ZONE_RED     = 1,
    MAP_ZONE_BLUE    = 2,
    MAP_ZONE_DEBRIS  = 3
} MapZone;

typedef struct {
    int           width;
    int           height;
    unsigned char zones[MAP_MAX_W][MAP_MAX_H];  /* MapZone per cell */
    int           red_spawn_x,  red_spawn_y;
    int           blue_spawn_x, blue_spawn_y;
    int           red_recep_x,  red_recep_y;
    int           blue_recep_x, blue_recep_y;
    int           red_flag_x,   red_flag_y;
    int           blue_flag_x,  blue_flag_y;
} MapConfig;

const MapConfig *map_config_get(void);
int  map_config_load_default(void);
int  map_config_load_from_string(const char *src);

#endif
