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

static int set_upgrade_creep_target(CreepUpgradeConfig *u, const char *val) {
    int idx = creep_config_lookup_type(val);
    if (idx < 0) return -1;
    u->creep_type = idx;
    return 0;
}

/* `requires` resolves at parse time, so the prerequisite must be declared
 * before this upgrade — same forward-reference rule as the `creep`
 * directive. An upgrade can't require itself. */
static int set_upgrade_requires(CreepUpgradeConfig *u, const char *val,
                                int self_idx) {
    int idx = creep_config_lookup_upgrade(val);
    if (idx < 0 || idx == self_idx) return -1;
    u->requires = idx;
    return 0;
}

static int set_upgrade_field(CreepUpgradeConfig *u, int self_idx,
                             const char *key, const char *val) {
    if (!strcmp(key, "cost"))           return parse_int(val, &u->cost);
    if (!strcmp(key, "research_turns")) return parse_int(val, &u->research_turns);
    if (!strcmp(key, "creep"))          return set_upgrade_creep_target(u, val);
    if (!strcmp(key, "requires"))       return set_upgrade_requires(u, val, self_idx);
    if (!strcmp(key, "count")) {
        int rc = parse_int(val, &u->count);
        if (rc == 0) u->set_flags |= CREEP_UPG_SET_COUNT;
        return rc;
    }
    if (!strcmp(key, "code")) {
        if (strlen(val) != 1) return -1;
        u->code = val[0];
        u->set_flags |= CREEP_UPG_SET_CODE;
        return 0;
    }
    if (!strcmp(key, "hp")) {
        int rc = parse_int(val, &u->hp);
        if (rc == 0) u->set_flags |= CREEP_UPG_SET_HP;
        return rc;
    }
    if (!strcmp(key, "can_carry_flag")) {
        int rc = parse_int(val, &u->can_carry_flag);
        if (rc == 0) u->set_flags |= CREEP_UPG_SET_CAN_CARRY_FLAG;
        return rc;
    }
    if (!strcmp(key, "melee_damage")) {
        int rc = parse_int(val, &u->melee_damage);
        if (rc == 0) u->set_flags |= CREEP_UPG_SET_MELEE_DAMAGE;
        return rc;
    }
    if (!strcmp(key, "spawn_order")) {
        int rc = parse_int(val, &u->spawn_order);
        if (rc == 0) u->set_flags |= CREEP_UPG_SET_SPAWN_ORDER;
        return rc;
    }
    if (!strcmp(key, "vision")) {
        int rc = parse_int(val, &u->vision);
        if (rc == 0) u->set_flags |= CREEP_UPG_SET_VISION;
        return rc;
    }
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
    /* Default creep_type / requires to -1 (sentinels for "no target" /
     * "no prerequisite"), so an upgrade without an explicit `creep` /
     * `requires` directive cleanly resolves. */
    for (int i = 0; i < CREEP_UPGRADE_MAX_COUNT; i++) {
        g_catalog.upgrades[i].creep_type = -1;
        g_catalog.upgrades[i].requires   = -1;
    }

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
            if (creep_config_lookup_upgrade(val) >= 0) return -1;
            if (g_catalog.upgrade_count >= CREEP_UPGRADE_MAX_COUNT) return -1;
            CreepUpgradeConfig *u = &g_catalog.upgrades[g_catalog.upgrade_count];
            memset(u, 0, sizeof(*u));
            u->creep_type = -1;
            u->requires   = -1;
            strncpy(u->id, val, CREEP_UPGRADE_ID_MAX - 1);
            current_idx = g_catalog.upgrade_count++;
            kind = SECTION_UPGRADE;
            continue;
        }
        /* `creep` is overloaded: at top level (or after a type section) it
         * declares a new type; inside an upgrade body it sets the target
         * creep type for that upgrade. */
        if (!strcmp(key, "creep") && kind != SECTION_UPGRADE) {
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

        /* All other body keys are upgrade-only. A creep type's section is
         * just its declaration line — there are no inner keys for it. */
        if (kind != SECTION_UPGRADE || !*val) return -1;
        if (set_upgrade_field(&g_catalog.upgrades[current_idx], current_idx, key, val) != 0) return -1;
    }

    return 0;
}

int creep_config_load_default(void) {
    return creep_config_load_from_string(CREEP_UPGRADE_CONFIG_DEFAULT);
}
