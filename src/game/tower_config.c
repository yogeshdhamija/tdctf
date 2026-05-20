#include "tower_config.h"
#include "tower_config_data.h"   /* generated from data/towers.cfg */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static TowerCatalog g_catalog;

const TowerCatalog *tower_config_get(void) { return &g_catalog; }

static int tower_id_from_name(const char *n) {
    if (!strcmp(n, "BLOCKER"))  return TOWER_BLOCKER;
    if (!strcmp(n, "GUNNER"))   return TOWER_GUNNER;
    if (!strcmp(n, "SLAMMER"))  return TOWER_SLAMMER;
    if (!strcmp(n, "RESOURCE")) return TOWER_RESOURCE;
    return -1;
}

static int parse_int(const char *s, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != 0) return -1;
    *out = (int)v;
    return 0;
}

static int set_field(TowerConfig *t, const char *key, const char *val) {
    /* Per-level fields parse as `levelN.<sub>` with N in [1, TOWER_MAX_LEVELS]. */
    if (!strncmp(key, "level", 5)
        && key[5] >= '1' && key[5] <= ('0' + TOWER_MAX_LEVELS)
        && key[6] == '.') {
        TowerLevelStats *L = &t->level[key[5] - '1'];
        const char *sub = key + 7;
        if (!strcmp(sub, "dmg"))      return parse_int(val, &L->dmg);
        if (!strcmp(sub, "range"))    return parse_int(val, &L->range);
        if (!strcmp(sub, "aoe"))      return parse_int(val, &L->aoe);
        if (!strcmp(sub, "slow"))     return parse_int(val, &L->slow);
        if (!strcmp(sub, "cooldown")) return parse_int(val, &L->cooldown);
        if (!strcmp(sub, "income"))   return parse_int(val, &L->income);
        return -1;
    }
    if (!strcmp(key, "cost"))             return parse_int(val, &t->cost);
    if (!strcmp(key, "hp"))               return parse_int(val, &t->hp);
    if (!strcmp(key, "build_turns"))      return parse_int(val, &t->build_turns);
    if (!strcmp(key, "upgrade_cost"))     return parse_int(val, &t->upgrade_cost);
    if (!strcmp(key, "upgrade_build"))    return parse_int(val, &t->upgrade_build);
    if (!strcmp(key, "upgrade_hp_bonus")) return parse_int(val, &t->upgrade_hp_bonus);
    if (!strcmp(key, "code")) {
        if (strlen(val) != 1) return -1;
        t->code = val[0];
        return 0;
    }
    if (!strcmp(key, "name")) {
        strncpy(t->name, val, TOWER_NAME_MAX - 1);
        t->name[TOWER_NAME_MAX - 1] = 0;
        return 0;
    }
    return -1;
}

int tower_config_load_from_string(const char *src) {
    memset(&g_catalog, 0, sizeof(g_catalog));
    int current = -1;
    char line[256];
    const char *p = src;

    while (*p) {
        /* Copy one line into `line`, trimming on '\n' or buffer-full. If the
         * line overflows the buffer we still advance `p` past the '\n' so the
         * parser doesn't re-enter mid-line on the next pass. */
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
        line[n] = 0;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        /* Strip '#' comments. */
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        /* Tokenise on whitespace. */
        char *toks[4];
        int nt = 0;
        char *s = line;
        while (*s && nt < 4) {
            while (*s && isspace((unsigned char)*s)) s++;
            if (!*s) break;
            toks[nt++] = s;
            while (*s && !isspace((unsigned char)*s)) s++;
            if (*s) { *s = 0; s++; }
        }
        if (nt == 0) continue;

        if (!strcmp(toks[0], "tower")) {
            if (nt < 2) return -1;
            current = tower_id_from_name(toks[1]);
            if (current < 0) return -1;
            continue;
        }
        if (current < 0 || nt < 2) return -1;
        if (set_field(&g_catalog.towers[current], toks[0], toks[1]) != 0) return -1;
    }
    return 0;
}

int tower_config_load_default(void) {
    return tower_config_load_from_string(TOWER_CONFIG_DEFAULT);
}
