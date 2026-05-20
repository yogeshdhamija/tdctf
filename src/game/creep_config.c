#include "creep_config.h"
#include "creep_config_data.h"   /* generated from data/creep_upgrades.cfg */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static CreepUpgradeCatalog g_catalog;

const CreepUpgradeCatalog *creep_config_get(void) { return &g_catalog; }

int creep_config_lookup(const char *id) {
    for (int i = 0; i < g_catalog.count; i++)
        if (!strcmp(g_catalog.upgrades[i].id, id)) return i;
    return -1;
}

static int parse_int(const char *s, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != 0) return -1;
    *out = (int)v;
    return 0;
}

/* Trim trailing whitespace from `s` in place. */
static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static int set_field(CreepUpgradeConfig *u, const char *key, const char *val) {
    if (!strcmp(key, "cost"))           return parse_int(val, &u->cost);
    if (!strcmp(key, "research_turns")) return parse_int(val, &u->research_turns);
    if (!strcmp(key, "add_retrievers")) return parse_int(val, &u->add_retrievers);
    if (!strcmp(key, "add_siege"))      return parse_int(val, &u->add_siege);
    if (!strcmp(key, "description")) {
        strncpy(u->description, val, CREEP_UPGRADE_DESC_MAX - 1);
        u->description[CREEP_UPGRADE_DESC_MAX - 1] = 0;
        return 0;
    }
    return -1;
}

int creep_config_load_from_string(const char *src) {
    memset(&g_catalog, 0, sizeof(g_catalog));

    int  current = -1;
    char line[256];
    const char *p = src;

    while (*p) {
        /* Copy one line. Overflowing lines still advance past '\n'. */
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
        line[n] = 0;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        /* First token (key); rest of the line is the value. This lets
         * `description` contain spaces while still tokenizing cleanly for
         * single-word values like `cost 30`. */
        char *s = line;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) continue;
        char *key = s;
        while (*s && !isspace((unsigned char)*s)) s++;
        if (*s) { *s = 0; s++; }
        while (*s && isspace((unsigned char)*s)) s++;
        char *val = s;
        rtrim(val);

        if (!strcmp(key, "upgrade")) {
            if (!*val) return -1;
            if (creep_config_lookup(val) >= 0) return -1; /* duplicate */
            if (g_catalog.count >= CREEP_UPGRADE_MAX_COUNT) return -1;
            CreepUpgradeConfig *u = &g_catalog.upgrades[g_catalog.count];
            memset(u, 0, sizeof(*u));
            strncpy(u->id, val, CREEP_UPGRADE_ID_MAX - 1);
            current = g_catalog.count++;
            continue;
        }

        if (current < 0) return -1;     /* key with no enclosing section */
        if (!*val) return -1;
        if (set_field(&g_catalog.upgrades[current], key, val) != 0) return -1;
    }

    return 0;
}

int creep_config_load_default(void) {
    return creep_config_load_from_string(CREEP_UPGRADE_CONFIG_DEFAULT);
}
