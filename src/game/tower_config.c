#include "tower_config.h"
#include "tower_config_data.h"   /* generated from data/towers.cfg */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static TowerCatalog g_catalog;

const TowerCatalog *tower_config_get(void) { return &g_catalog; }

int tower_config_lookup(const char *id) {
    for (int i = 0; i < g_catalog.count; i++)
        if (!strcmp(g_catalog.towers[i].id, id)) return i;
    return -1;
}

static int parse_int(const char *s, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != 0) return -1;
    *out = (int)v;
    return 0;
}

static int set_tower_field(TowerConfig *t, const char *key, const char *val) {
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

static int set_level_field(TowerLevelStats *L, const char *key, const char *val) {
    if (!strcmp(key, "cost"))        return parse_int(val, &L->cost);
    if (!strcmp(key, "hp"))          return parse_int(val, &L->hp);
    if (!strcmp(key, "build_turns")) return parse_int(val, &L->build_turns);
    if (!strcmp(key, "dmg"))         return parse_int(val, &L->dmg);
    if (!strcmp(key, "range"))       return parse_int(val, &L->range);
    if (!strcmp(key, "aoe"))         return parse_int(val, &L->aoe);
    if (!strcmp(key, "slow"))        return parse_int(val, &L->slow);
    if (!strcmp(key, "cooldown"))    return parse_int(val, &L->cooldown);
    if (!strcmp(key, "income"))      return parse_int(val, &L->income);
    if (!strcmp(key, "crit_chance")) return parse_int(val, &L->crit_chance);
    if (!strcmp(key, "crit_dmg"))    return parse_int(val, &L->crit_dmg);
    return -1;
}

int tower_config_load_from_string(const char *src) {
    memset(&g_catalog, 0, sizeof(g_catalog));

    int  current_tower = -1;
    int  current_level = -1;     /* -1 = tower section, >=0 = level index */
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

        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

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
            if (tower_config_lookup(toks[1]) >= 0) return -1; /* duplicate */
            if (g_catalog.count >= TOWER_MAX_COUNT) return -1;
            TowerConfig *t = &g_catalog.towers[g_catalog.count];
            memset(t, 0, sizeof(*t));
            strncpy(t->id, toks[1], TOWER_ID_MAX - 1);
            current_tower = g_catalog.count++;
            current_level = -1;
            continue;
        }
        if (!strcmp(toks[0], "level")) {
            if (nt < 3) return -1;
            int idx = tower_config_lookup(toks[1]);
            if (idx < 0) return -1;       /* level before tower */
            int n_decl;
            if (parse_int(toks[2], &n_decl) != 0) return -1;
            TowerConfig *t = &g_catalog.towers[idx];
            if (n_decl != t->level_count + 1) return -1;   /* must be sequential */
            if (t->level_count >= TOWER_MAX_LEVELS) return -1;
            current_tower = idx;
            current_level = t->level_count++;
            continue;
        }

        if (current_tower < 0 || nt < 2) return -1;
        TowerConfig *t = &g_catalog.towers[current_tower];
        int rc = (current_level < 0)
                 ? set_tower_field(t, toks[0], toks[1])
                 : set_level_field(&t->level[current_level], toks[0], toks[1]);
        if (rc != 0) return -1;
    }

    /* Every tower must have at least one level — placement reads level[0]. */
    for (int i = 0; i < g_catalog.count; i++)
        if (g_catalog.towers[i].level_count < 1) return -1;

    return 0;
}

int tower_config_load_default(void) {
    return tower_config_load_from_string(TOWER_CONFIG_DEFAULT);
}
