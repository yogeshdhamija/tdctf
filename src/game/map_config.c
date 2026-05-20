#include "map_config.h"
#include "map_config_data.h"   /* generated from data/map.cfg */
#include <ctype.h>
#include <string.h>

static MapConfig g_config;

const MapConfig *map_config_get(void) { return &g_config; }

/* Parse one grid cell character into (zone, landmark slot). Returns 0 on
 * success and -1 on unknown symbol. `*lm_slot` is set to a landmark index
 * 0..5 (red_spawn / blue_spawn / red_recep / blue_recep / red_flag /
 * blue_flag) or -1 if the cell is plain zone-only. */
static int classify(char c, MapZone *zone, int *lm_slot) {
    *lm_slot = -1;
    switch (c) {
        case '.': *zone = MAP_ZONE_NEUTRAL; return 0;
        case 'R': *zone = MAP_ZONE_RED;     return 0;
        case 'B': *zone = MAP_ZONE_BLUE;    return 0;
        case 'X': *zone = MAP_ZONE_DEBRIS;  return 0;
        case '1': *zone = MAP_ZONE_NEUTRAL; *lm_slot = 0; return 0;
        case '2': *zone = MAP_ZONE_NEUTRAL; *lm_slot = 1; return 0;
        case '[': *zone = MAP_ZONE_RED;     *lm_slot = 2; return 0;
        case ']': *zone = MAP_ZONE_BLUE;    *lm_slot = 3; return 0;
        case 'r': *zone = MAP_ZONE_RED;     *lm_slot = 4; return 0;
        case 'b': *zone = MAP_ZONE_BLUE;    *lm_slot = 5; return 0;
        default:  return -1;
    }
}

int map_config_load_from_string(const char *src) {
    memset(&g_config, 0, sizeof(g_config));
    /* Sentinel -1 in every landmark field — both axes — so we can detect a
     * missing landmark at end-of-parse and a duplicate landmark mid-parse. */
    int *lm_xs[6] = {
        &g_config.red_spawn_x,  &g_config.blue_spawn_x,
        &g_config.red_recep_x,  &g_config.blue_recep_x,
        &g_config.red_flag_x,   &g_config.blue_flag_x
    };
    int *lm_ys[6] = {
        &g_config.red_spawn_y,  &g_config.blue_spawn_y,
        &g_config.red_recep_y,  &g_config.blue_recep_y,
        &g_config.red_flag_y,   &g_config.blue_flag_y
    };
    for (int i = 0; i < 6; i++) { *lm_xs[i] = -1; *lm_ys[i] = -1; }

    int  in_grid = 0;
    int  y       = 0;
    char line[MAP_MAX_W + 64];
    const char *p = src;

    while (*p) {
        /* Copy one line into `line`, mirroring the read loop in
         * tower_config.c so behavior stays consistent across configs. */
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
        line[n] = 0;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        /* `#` introduces a comment. The grid alphabet never contains `#`
         * (debris uses `X`), so stripping it is safe even inside the grid. */
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        if (!in_grid) {
            char *s = line;
            while (*s && isspace((unsigned char)*s)) s++;
            if (!*s) continue;
            size_t len = strlen(s);
            while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = 0;
            if (!strcmp(s, "grid")) { in_grid = 1; continue; }
            return -1;
        }

        /* In-grid: trim trailing whitespace only — leading whitespace would
         * shift every cell, which is almost certainly a user mistake. */
        size_t len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1])) line[--len] = 0;
        if (len == 0) continue;

        if (y == 0) {
            if ((int)len > MAP_MAX_W) return -1;
            g_config.width = (int)len;
        } else if ((int)len != g_config.width) {
            return -1;
        }
        if (y >= MAP_MAX_H) return -1;

        for (int x = 0; x < g_config.width; x++) {
            MapZone z;
            int slot;
            if (classify(line[x], &z, &slot) != 0) return -1;
            g_config.zones[x][y] = (unsigned char)z;
            if (slot >= 0) {
                if (*lm_xs[slot] >= 0) return -1;   /* duplicate landmark */
                *lm_xs[slot] = x;
                *lm_ys[slot] = y;
            }
        }
        y++;
    }

    if (!in_grid || y == 0) return -1;
    g_config.height = y;

    for (int i = 0; i < 6; i++)
        if (*lm_xs[i] < 0) return -1;               /* missing landmark */

    return 0;
}

int map_config_load_default(void) {
    return map_config_load_from_string(MAP_CONFIG_DEFAULT);
}
