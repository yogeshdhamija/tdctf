#include "creep_config.h"
#include "creep_config_data.h"   /* generated from data/creep_upgrades.cfg */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static CreepCatalog g_catalog;

const CreepCatalog *creep_config_get(void) { return &g_catalog; }

int creep_config_lookup_type(const char *id) {
    for (int i = 0; i < g_catalog.type_count; i++)
        if (!strcmp(g_catalog.types[i].id, id)) return i;
    return -1;
}

int creep_config_lookup_upgrade(const char *id) {
    for (int i = 0; i < g_catalog.upgrade_count; i++)
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

static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static int set_type_field(CreepTypeConfig *t, const char *key, const char *val) {
    if (!strcmp(key, "code")) {
        if (strlen(val) != 1) return -1;
        t->code = val[0];
        return 0;
    }
    if (!strcmp(key, "hp"))             return parse_int(val, &t->hp);
    if (!strcmp(key, "can_carry_flag")) return parse_int(val, &t->can_carry_flag);
    if (!strcmp(key, "melee_damage"))   return parse_int(val, &t->melee_damage);
    if (!strcmp(key, "spawn_order"))    return parse_int(val, &t->spawn_order);
    return -1;
}

/* `spawn <CREEP_ID> <N>` — value carries two whitespace-separated tokens.
 * Resolves CREEP_ID to a type index, requires N to be an integer. */
static int set_upgrade_spawn(CreepUpgradeConfig *u, const char *val) {
    char tmp[CREEP_TYPE_ID_MAX + 32];
    size_t n = strlen(val);
    if (n >= sizeof(tmp)) return -1;
    memcpy(tmp, val, n + 1);

    char *s = tmp;
    while (*s && isspace((unsigned char)*s)) s++;
    char *creep_id = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (!*s) return -1;             /* missing count token */
    *s = 0; s++;
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return -1;

    int idx = creep_config_lookup_type(creep_id);
    if (idx < 0) return -1;
    u->spawn_type = idx;
    return parse_int(s, &u->spawn_count);
}

static int set_upgrade_field(CreepUpgradeConfig *u, const char *key, const char *val) {
    if (!strcmp(key, "cost"))           return parse_int(val, &u->cost);
    if (!strcmp(key, "research_turns")) return parse_int(val, &u->research_turns);
    if (!strcmp(key, "spawn"))          return set_upgrade_spawn(u, val);
    if (!strcmp(key, "description")) {
        strncpy(u->description, val, CREEP_UPGRADE_DESC_MAX - 1);
        u->description[CREEP_UPGRADE_DESC_MAX - 1] = 0;
        return 0;
    }
    return -1;
}

typedef enum { SECTION_NONE, SECTION_TYPE, SECTION_UPGRADE } SectionKind;

int creep_config_load_from_string(const char *src) {
    memset(&g_catalog, 0, sizeof(g_catalog));
    /* Mark every potential upgrade slot as "no spawn" by default so an
     * upgrade with no `spawn` directive cleanly resolves to "spawn nothing". */
    for (int i = 0; i < CREEP_UPGRADE_MAX_COUNT; i++) g_catalog.upgrades[i].spawn_type = -1;

    SectionKind kind = SECTION_NONE;
    int  current_idx = -1;
    char line[256];
    const char *p = src;

    while (*p) {
        size_t n = 0;
        while (*p && *p != '\n' && n < sizeof(line) - 1) line[n++] = *p++;
        line[n] = 0;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        /* First token is the key; rest of the line (trimmed) is the value.
         * For `spawn` the value is itself two tokens, but that's handled
         * downstream in set_upgrade_spawn. */
        char *s = line;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) continue;
        char *key = s;
        while (*s && !isspace((unsigned char)*s)) s++;
        if (*s) { *s = 0; s++; }
        while (*s && isspace((unsigned char)*s)) s++;
        char *val = s;
        rtrim(val);

        if (!strcmp(key, "creep")) {
            if (!*val) return -1;
            if (creep_config_lookup_type(val) >= 0) return -1;
            if (g_catalog.type_count >= CREEP_TYPE_MAX_COUNT) return -1;
            CreepTypeConfig *t = &g_catalog.types[g_catalog.type_count];
            memset(t, 0, sizeof(*t));
            strncpy(t->id, val, CREEP_TYPE_ID_MAX - 1);
            current_idx = g_catalog.type_count++;
            kind = SECTION_TYPE;
            continue;
        }
        if (!strcmp(key, "upgrade")) {
            if (!*val) return -1;
            if (creep_config_lookup_upgrade(val) >= 0) return -1;
            if (g_catalog.upgrade_count >= CREEP_UPGRADE_MAX_COUNT) return -1;
            CreepUpgradeConfig *u = &g_catalog.upgrades[g_catalog.upgrade_count];
            memset(u, 0, sizeof(*u));
            u->spawn_type = -1;
            strncpy(u->id, val, CREEP_UPGRADE_ID_MAX - 1);
            current_idx = g_catalog.upgrade_count++;
            kind = SECTION_UPGRADE;
            continue;
        }

        if (kind == SECTION_NONE || !*val) return -1;
        int rc = (kind == SECTION_TYPE)
                 ? set_type_field(&g_catalog.types[current_idx], key, val)
                 : set_upgrade_field(&g_catalog.upgrades[current_idx], key, val);
        if (rc != 0) return -1;
    }

    return 0;
}

int creep_config_load_default(void) {
    return creep_config_load_from_string(CREEP_UPGRADE_CONFIG_DEFAULT);
}
