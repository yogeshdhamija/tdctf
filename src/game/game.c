#include "game.h"
#include "tower_config.h"
#include "creep_config.h"
#include "map_config.h"
#include <string.h>
#include <stdio.h>

static GameState s;

/* Deterministic xorshift32 PRNG. Seeded to a fixed value by game_init_state
 * so identical play sequences produce identical sim outcomes, and the state
 * lives in GameState.rng_state so the snapshot round-trips it — resuming
 * from a mid-game snapshot reproduces the same future rolls. Consumers:
 * tower crit rolls and the pathing wobble (random tiebreak among equal
 * shortest paths). State is taken by pointer so callers — including the
 * pathing code, which threads &gs->rng_state through — can use it without
 * the function reaching back into the file-static GameState. Note the zero
 * seed is a fixed point; seed to non-zero for actual randomness. */
static uint32_t rng_next(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Tower stats live in data/towers.cfg, embedded as TOWER_CONFIG_DEFAULT and
 * parsed into a TowerCatalog at game_init(). All tower lookups go through
 * the catalog — there is no hardcoded tower data in this file. */
static const TowerConfig *spec(TowerType t) {
    return &tower_config_get()->towers[t];
}

int  game_tower_count(void)         { return tower_config_get()->count; }
int  game_tower_id(const char *n)   { return tower_config_lookup(n); }
int  game_tower_max_level(TowerType t) { return spec(t)->level_count; }

/* Placement cost = cost to enter level 1. */
int  game_tower_cost(TowerType t)   { return spec(t)->level[0].cost; }

/* Upgrade cost = cost to enter (from_level + 1). Returns 0 if already at
 * the top, so callers can use the value as "is upgrading possible" too. */
int  game_tower_upgrade_cost(TowerType t, int from_level) {
    const TowerConfig *cfg = spec(t);
    if (from_level < 1 || from_level >= cfg->level_count) return 0;
    return cfg->level[from_level].cost;
}

int game_tower_build_turns(TowerType t) {
    return spec(t)->level[0].build_turns;
}
int game_tower_upgrade_turns(TowerType t, int from_level) {
    const TowerConfig *cfg = spec(t);
    if (from_level < 1 || from_level >= cfg->level_count) return 0;
    return cfg->level[from_level].build_turns;
}

const char *game_tower_name(TowerType t) { return spec(t)->name; }
char        game_tower_code(TowerType t) { return spec(t)->code; }

/* Creep catalog lookups. The catalog is global; per-player runtime state
 * (purchased / completed / turns_remaining) is on Player.creep_upgrades
 * indexed parallel to the catalog's upgrades array. */
static const CreepTypeConfig    *ct_spec(CreepType t) {
    return &creep_config_get()->types[t];
}
static const CreepUpgradeConfig *cu_spec(int idx) {
    return &creep_config_get()->upgrades[idx];
}
int  game_creep_type_count(void)             { return creep_config_get()->type_count; }
int  game_creep_type_id(const char *name)    { return creep_config_lookup_type(name); }
char game_creep_type_code(CreepType t)       { return ct_spec(t)->code; }
int  game_creep_type_can_carry_flag(CreepType t) { return ct_spec(t)->can_carry_flag; }
int  game_creep_type_melee_damage(CreepType t)   { return ct_spec(t)->melee_damage; }
int  game_creep_type_spawn_order(CreepType t)    { return ct_spec(t)->spawn_order; }

int  game_creep_upgrade_count(void)               { return creep_config_get()->upgrade_count; }
int  game_creep_upgrade_cost(int idx)             { return cu_spec(idx)->cost; }
int  game_creep_upgrade_research_turns(int idx)   { return cu_spec(idx)->research_turns; }
const char *game_creep_upgrade_description(int idx) { return cu_spec(idx)->description; }

PlayerID game_planning_player(void) {
    return (s.phase == PHASE_PLAN_BLUE) ? PLAYER_BLUE : PLAYER_RED;
}

const GameState *game_get_state(void) { return &s; }

/* ── Helpers ────────────────────────────────────────────── */

static int abs_i(int x) { return x < 0 ? -x : x; }

static int in_bounds(int x, int y) {
    return x >= 0 && y >= 0 && x < s.grid_w && y < s.grid_h;
}

static int walkable_in(const GameState *gs, int x, int y) {
    if (x < 0 || y < 0 || x >= gs->grid_w || y >= gs->grid_h) return 0;
    if (gs->grid[x][y].zone == ZONE_DEBRIS) return 0;
    if (gs->grid[x][y].thing_id != -1) return 0;
    return 1;
}

static int tower_at(int x, int y) {
    if (!in_bounds(x, y)) return -1;
    int id = s.grid[x][y].thing_id;
    if (id < 0) return -1;
    if (s.things[id].tag != THING_TOWER) return -1;
    return id;
}

static int alloc_thing(void) {
    for (int i = 0; i < s.thing_count; i++)
        if (s.things[i].tag == THING_NONE) return i;
    if (s.thing_count >= MAX_THINGS) return -1;
    return s.thing_count++;
}

static void set_status(const char *msg) {
    strncpy(s.status_msg, msg, sizeof(s.status_msg) - 1);
    s.status_msg[sizeof(s.status_msg) - 1] = 0;
    s.status_ttl = 180;
}

/* ── BFS pathfinding ────────────────────────────────────── */

/* Creep pathing per docs/game-design.md §10. Each creep walks the shortest
 * unblocked path spawn → enemy flag → own receptacle. Phase 1 (visited_flag
 * == 0): the goal is the enemy flag's current cell. Phase 2 (visited_flag
 * == 1, set as soon as the creep steps onto the flag's current cell): the
 * goal is the creep's own receptacle.
 *
 * BFS runs goal→source, populating a distance field. The source's distance
 * is the length of the shortest path; any walkable neighbour of the source
 * whose distance is exactly one less is the start of *a* shortest path.
 * When multiple such neighbours exist (a tie), we pick uniformly at random
 * via gs->rng_state — this is the "pathing wobble" from game-design.md §10,
 * adding an element of chance to which path a creep takes. The pick is
 * deterministic given the RNG state and so survives snapshots. */
static int     bfs_dist[MAX_GRID_W][MAX_GRID_H];   /* stored as distance + 1 so 0 means unvisited */
static int     bfs_qx[MAX_GRID_W * MAX_GRID_H];
static int     bfs_qy[MAX_GRID_W * MAX_GRID_H];

/* gs is not const: when step_x is set and multiple shortest-path neighbours
 * tie, we consume one rng_next from gs->rng_state. With step_x == NULL
 * (paths_valid uses this) we only answer reachability and never touch the
 * RNG. */
static int bfs_to_goal(GameState *gs, int sx, int sy, int gx, int gy,
                       int *step_x, int *step_y) {
    if (sx < 0 || sy < 0 || sx >= gs->grid_w || sy >= gs->grid_h) return 0;
    if (sx == gx && sy == gy) {
        if (step_x) { *step_x = sx; *step_y = sy; }
        return 1;
    }
    int W = gs->grid_w, H = gs->grid_h;
    for (int x = 0; x < W; x++)
        for (int y = 0; y < H; y++)
            bfs_dist[x][y] = 0;
    int qh = 0, qt = 0;
    bfs_qx[qt] = gx; bfs_qy[qt] = gy; qt++;
    bfs_dist[gx][gy] = 1;          /* distance 0, +1 sentinel */
    static const int DX[4] = { 1, -1, 0,  0 };
    static const int DY[4] = { 0,  0, 1, -1 };
    while (qh < qt) {
        int cx = bfs_qx[qh], cy = bfs_qy[qh]; qh++;
        /* Stop expanding once the source has been popped — all source
         * neighbours at distance source_d-1 are already discovered by FIFO
         * layering, which is everything the tie-pick below needs. */
        if (cx == sx && cy == sy) break;
        for (int i = 0; i < 4; i++) {
            int nx = cx + DX[i], ny = cy + DY[i];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (bfs_dist[nx][ny]) continue;
            if (!walkable_in(gs, nx, ny)) continue;
            bfs_dist[nx][ny] = bfs_dist[cx][cy] + 1;
            bfs_qx[qt] = nx; bfs_qy[qt] = ny; qt++;
        }
    }
    if (!bfs_dist[sx][sy]) return 0;
    if (!step_x) return 1;
    int target_d = bfs_dist[sx][sy] - 1;
    int cand_x[4], cand_y[4], n = 0;
    for (int i = 0; i < 4; i++) {
        int nx = sx + DX[i], ny = sy + DY[i];
        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
        if (bfs_dist[nx][ny] != target_d) continue;
        cand_x[n] = nx; cand_y[n] = ny; n++;
    }
    if (n == 0) return 0;
    int pick = (n == 1) ? 0 : (int)(rng_next(&gs->rng_state) % (uint32_t)n);
    *step_x = cand_x[pick]; *step_y = cand_y[pick];
    return 1;
}

int game_pathing_next_step(GameState *gs, int creep_x, int creep_y,
                           PlayerID owner, int visited_flag,
                           int *out_x, int *out_y) {
    int gx, gy;
    PlayerID enemy = (owner == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
    const Flag *f = &gs->flags[enemy];
    /* Carried-flag exemption: a non-carrier creep in phase 1 does NOT chase
     * a teammate carrier. The flag's current cell is a goal only while it's
     * at home or dropped. If it's being carried, the creep skips ahead and
     * heads to the receptacle alongside the carrier — but without locking
     * in visited_flag, so a subsequent carrier death (dropping the flag)
     * reverts the goal to the new flag location. */
    if (!visited_flag && f->carried_by == -1) {
        gx = f->x; gy = f->y;
    } else {
        gx = gs->receptacle_x[owner]; gy = gs->receptacle_y[owner];
    }
    return bfs_to_goal(gs, creep_x, creep_y, gx, gy, out_x, out_y);
}

/* Placement is legal only if every creep's journey remains possible:
 * spawn → enemy flag → own receptacle for both players. Either half being
 * severed would strand creeps. */
static int paths_valid(void) {
    for (int p = 0; p < 2; p++) {
        PlayerID enemy = (p == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
        if (!bfs_to_goal(&s, s.spawn_x[p], s.spawn_y[p],
                         s.flags[enemy].x, s.flags[enemy].y, NULL, NULL)) return 0;
        if (!bfs_to_goal(&s, s.flags[enemy].x, s.flags[enemy].y,
                         s.receptacle_x[p], s.receptacle_y[p], NULL, NULL)) return 0;
    }
    return 1;
}

/* ── Creep upgrade init ─────────────────────────────────── */

/* Per-player runtime state mirrors the catalog 1:1: index i holds the
 * dynamic state for catalog upgrade i. Static spec (cost, research_turns,
 * description, ...) is read directly from the catalog at use time. */
static void init_creep_upgrades(Player *p) {
    int n = creep_config_get()->upgrade_count;
    if (n > MAX_CREEP_UPGRADES) n = MAX_CREEP_UPGRADES;
    p->creep_upgrade_count = n;
    for (int i = 0; i < n; i++) memset(&p->creep_upgrades[i], 0, sizeof(p->creep_upgrades[i]));
}

/* ── Init ───────────────────────────────────────────────── */

/* Reset all non-catalog state. Split out so callers can pin the catalog
 * first (via tower_config_load_*) and then call this to reset everything
 * else without re-loading the default config. */
/* Map zones come from map_config.h (which doesn't depend on game.h). The
 * MapZone enum order matches ZoneType so we can cast — but enforce the
 * mapping explicitly here so a future drift can't go silently wrong. */
static ZoneType to_zone_type(unsigned char mz) {
    switch (mz) {
        case MAP_ZONE_RED:    return ZONE_RED;
        case MAP_ZONE_BLUE:   return ZONE_BLUE;
        case MAP_ZONE_DEBRIS: return ZONE_DEBRIS;
        default:              return ZONE_NEUTRAL;
    }
}

static void game_init_state(void) {
    const MapConfig *m = map_config_get();
    memset(&s, 0, sizeof(s));
    s.rng_state = 0x9E3779B9u; /* fixed seed → deterministic sim across runs */
    s.grid_w = m->width;
    s.grid_h = m->height;
    s.turn = 1;
    s.phase = PHASE_PLAN_RED;
    s.selected_x = -1; s.selected_y = -1;
    s.placement_intent = -1;
    s.winner = -1;

    for (int x = 0; x < s.grid_w; x++) {
        for (int y = 0; y < s.grid_h; y++) {
            s.grid[x][y].thing_id = -1;
            s.grid[x][y].zone     = to_zone_type(m->zones[x][y]);
        }
    }

    s.players[PLAYER_RED].resources       = 100;
    s.players[PLAYER_RED].income_per_turn = 0;
    s.players[PLAYER_BLUE].resources       = 100;
    s.players[PLAYER_BLUE].income_per_turn = 0;
    init_creep_upgrades(&s.players[PLAYER_RED]);
    init_creep_upgrades(&s.players[PLAYER_BLUE]);

    s.spawn_x[PLAYER_RED]       = m->red_spawn_x;  s.spawn_y[PLAYER_RED]       = m->red_spawn_y;
    s.spawn_x[PLAYER_BLUE]      = m->blue_spawn_x; s.spawn_y[PLAYER_BLUE]      = m->blue_spawn_y;
    s.receptacle_x[PLAYER_RED]  = m->red_recep_x;  s.receptacle_y[PLAYER_RED]  = m->red_recep_y;
    s.receptacle_x[PLAYER_BLUE] = m->blue_recep_x; s.receptacle_y[PLAYER_BLUE] = m->blue_recep_y;

    s.flags[PLAYER_RED].x = m->red_flag_x; s.flags[PLAYER_RED].y = m->red_flag_y;
    s.flags[PLAYER_RED].owner      = PLAYER_RED;
    s.flags[PLAYER_RED].carried_by = -1;
    s.flags[PLAYER_RED].at_home    = 1;

    s.flags[PLAYER_BLUE].x = m->blue_flag_x; s.flags[PLAYER_BLUE].y = m->blue_flag_y;
    s.flags[PLAYER_BLUE].owner      = PLAYER_BLUE;
    s.flags[PLAYER_BLUE].carried_by = -1;
    s.flags[PLAYER_BLUE].at_home    = 1;
}

void game_init(void) {
    tower_config_load_default();
    creep_config_load_default();
    map_config_load_default();
    game_init_state();
}

void game_init_with_tower_config(const char *cfg) {
    tower_config_load_from_string(cfg);
    creep_config_load_default();
    map_config_load_default();
    game_init_state();
}

void game_init_with_creep_config(const char *cfg) {
    tower_config_load_default();
    creep_config_load_from_string(cfg);
    map_config_load_default();
    game_init_state();
}

void game_init_with_configs(const char *tower_cfg, const char *creep_cfg) {
    tower_config_load_from_string(tower_cfg);
    creep_config_load_from_string(creep_cfg);
    map_config_load_default();
    game_init_state();
}

void game_init_with_configs_and_map(const char *tower_cfg, const char *creep_cfg, const char *map_cfg) {
    tower_config_load_from_string(tower_cfg);
    creep_config_load_from_string(creep_cfg);
    map_config_load_from_string(map_cfg);
    game_init_state();
}

/* ── Planning actions ───────────────────────────────────── */

static int planning(void) {
    return s.phase == PHASE_PLAN_RED || s.phase == PHASE_PLAN_BLUE;
}

static int placement_valid(int gx, int gy, PlayerID p) {
    if (!in_bounds(gx, gy)) return 0;
    if (s.grid[gx][gy].thing_id != -1) return 0;
    if (s.grid[gx][gy].zone == ZONE_DEBRIS) return 0;
    ZoneType z = s.grid[gx][gy].zone;
    if (p == PLAYER_RED  && z == ZONE_BLUE) return 0;
    if (p == PLAYER_BLUE && z == ZONE_RED)  return 0;
    for (int i = 0; i < 2; i++)
        if (gx == s.receptacle_x[i] && gy == s.receptacle_y[i]) return 0;
    for (int i = 0; i < 2; i++)
        if (s.flags[i].at_home && gx == s.flags[i].x && gy == s.flags[i].y) return 0;

    s.grid[gx][gy].thing_id = -2; /* sentinel: counts as blocking */
    int ok = paths_valid();
    s.grid[gx][gy].thing_id = -1;
    return ok;
}

static void try_place(int gx, int gy, TowerType type) {
    PlayerID p = game_planning_player();
    const TowerLevelStats *l1 = &spec(type)->level[0];
    if (s.players[p].resources < l1->cost) { set_status("Not enough resources"); return; }
    if (!placement_valid(gx, gy, p))       { set_status("Invalid placement");    return; }
    int id = alloc_thing();
    if (id < 0) return;
    Thing *t = &s.things[id];
    memset(t, 0, sizeof(*t));
    t->tag    = THING_TOWER;
    t->owner  = p;
    t->x      = gx; t->y = gy;
    t->hp     = l1->hp;
    t->max_hp = l1->hp;
    t->alive  = 1;
    t->tower.type        = type;
    t->tower.level       = 1;
    t->tower.build_turns = l1->build_turns;
    s.grid[gx][gy].thing_id = id;
    s.players[p].resources -= l1->cost;
    s.placement_intent = -1;
    s.selected_x = gx; s.selected_y = gy;
}

void game_grid_click(int gx, int gy) {
    if (!planning()) return;
    if (!in_bounds(gx, gy)) return;
    if (s.placement_intent >= 0) {
        try_place(gx, gy, (TowerType)s.placement_intent);
    } else {
        s.selected_x = gx; s.selected_y = gy;
    }
}

void game_set_placement(int type) {
    if (!planning()) return;
    if (type < 0 || type >= tower_config_get()->count) { s.placement_intent = -1; return; }
    if (s.placement_intent == type) {
        s.placement_intent = -1;
    } else {
        s.placement_intent = type;
        s.selected_x = -1; s.selected_y = -1;
    }
}

void game_upgrade_selected(void) {
    if (!planning() || s.selected_x < 0) return;
    int id = tower_at(s.selected_x, s.selected_y);
    if (id < 0) { set_status("No tower selected"); return; }
    Thing *t = &s.things[id];
    PlayerID p = game_planning_player();
    const TowerConfig *cfg = spec(t->tower.type);
    if (t->owner != p)                            { set_status("Not your tower");      return; }
    if (t->tower.level >= cfg->level_count)       { set_status("Max level");           return; }
    if (t->tower.build_turns > 0)                 { set_status("Still building");      return; }
    /* Upgrading from level N → N+1: read the next level's stats. The array
     * is 0-indexed, so level N+1 lives at cfg->level[t->tower.level]. */
    const TowerLevelStats *next = &cfg->level[t->tower.level];
    if (s.players[p].resources < next->cost)      { set_status("Not enough resources"); return; }
    s.players[p].resources -= next->cost;
    t->tower.level++;
    t->tower.build_turns = next->build_turns;
    t->max_hp = next->hp;
    t->hp = t->max_hp;
}

void game_destroy_selected(void) {
    if (!planning() || s.selected_x < 0) return;
    int id = tower_at(s.selected_x, s.selected_y);
    if (id < 0) return;
    Thing *t = &s.things[id];
    PlayerID p = game_planning_player();
    if (t->owner != p) { set_status("Not your tower"); return; }
    s.grid[t->x][t->y].thing_id = -1;
    t->alive = 0;
    t->tag = THING_NONE;
}

void game_buy_creep_upgrade(int idx) {
    if (!planning()) return;
    PlayerID p = game_planning_player();
    if (idx < 0 || idx >= s.players[p].creep_upgrade_count) return;
    CreepUpgrade *u = &s.players[p].creep_upgrades[idx];
    const CreepUpgradeConfig *cfg = cu_spec(idx);
    if (u->purchased)                       { set_status("Already purchased");   return; }
    if (s.players[p].resources < cfg->cost) { set_status("Not enough resources"); return; }
    s.players[p].resources -= cfg->cost;
    u->purchased = 1;
    u->turns_remaining = cfg->research_turns;
    /* research_turns 0 means "complete immediately, spawn this turn's sim" —
     * the analog of tower build_turns 0. Without this, the end-of-sim
     * decrement loop never sets `completed`, since it only fires when
     * turns_remaining > 0, and the upgrade would be silently stuck forever. */
    if (u->turns_remaining == 0) u->completed = 1;
}

/* ── Simulation ─────────────────────────────────────────── */

static void spawn_creep(PlayerID p, CreepType ct) {
    int id = alloc_thing();
    if (id < 0) return;
    const CreepTypeConfig *cspec = ct_spec(ct);
    Thing *t = &s.things[id];
    memset(t, 0, sizeof(*t));
    t->tag    = THING_CREEP;
    t->owner  = p;
    t->x      = s.spawn_x[p];
    t->y      = s.spawn_y[p];
    t->hp     = cspec->hp;
    t->max_hp = t->hp;
    t->alive  = 1;
    t->creep.type = ct;
}

/* Insertion sort the player's spawn queue by each creep type's spawn_order
 * (ascending). Stable: equal orders keep declaration/insertion order, which
 * is the order upgrades sit in the catalog. */
static void sort_spawn_queue(Player *pl) {
    for (int i = 1; i < pl->spawn_queue_count; i++) {
        CreepType v  = pl->spawn_queue[i];
        int       so = ct_spec(v)->spawn_order;
        int       j  = i - 1;
        while (j >= 0 && ct_spec(pl->spawn_queue[j])->spawn_order > so) {
            pl->spawn_queue[j + 1] = pl->spawn_queue[j];
            j--;
        }
        pl->spawn_queue[j + 1] = v;
    }
}

static void start_simulation(void) {
    s.phase = PHASE_SIMULATE;
    s.sim_tick = 0;
    s.sim_frame_accum = 0;
    s.sim_end_hold = 0;
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag == THING_TOWER) { t->beam_ttl = 0; t->tower.cooldown = 0; }
    }
    /* Build the per-player spawn queue: for every completed upgrade, push
     * spawn_count copies of its spawn_type. Then sort by creep-type
     * spawn_order so creeps appear in the configured order, one per tick.
     * Upgrades without a `spawn` directive (spawn_type < 0) contribute
     * nothing. If the catalog ever holds more spawns than MAX_SPAWN_QUEUE
     * the overflow is silently dropped — bumping the cap is the fix. */
    for (int p = 0; p < 2; p++) {
        Player *pl = &s.players[p];
        pl->spawn_queue_count = 0;
        pl->spawn_queue_pos   = 0;
        for (int i = 0; i < pl->creep_upgrade_count; i++) {
            const CreepUpgrade       *u   = &pl->creep_upgrades[i];
            const CreepUpgradeConfig *cfg = cu_spec(i);
            if (!u->completed) continue;
            if (cfg->spawn_type < 0 || cfg->spawn_count <= 0) continue;
            for (int n = cfg->spawn_count; n > 0; n--) {
                if (pl->spawn_queue_count >= MAX_SPAWN_QUEUE) break;
                pl->spawn_queue[pl->spawn_queue_count++] = cfg->spawn_type;
            }
        }
        sort_spawn_queue(pl);
    }
}

/* Pop the next queued creep (if any) for the player and spawn it at the
 * spawn cell. Called once per player at the start of every sim tick — that
 * gives the wave a one-cell stride along the path. */
static void drain_spawn_queue(PlayerID p) {
    Player *pl = &s.players[p];
    if (pl->spawn_queue_pos >= pl->spawn_queue_count) return;
    spawn_creep(p, pl->spawn_queue[pl->spawn_queue_pos++]);
}

static void sim_one_tick(void) {
    /* Spawn the next queued creep per player, if any. This runs before
     * movement so a freshly-spawned creep takes its first step on the same
     * tick — preserving the "first creep is at step N after N ticks" timing
     * the older all-at-once spawning model implied. */
    drain_spawn_queue(PLAYER_RED);
    drain_spawn_queue(PLAYER_BLUE);

    /* Move creeps. Per docs/game-design.md §10: each creep takes the
     * shortest unblocked path to the enemy flag's current cell, then once
     * it touches the flag (visited_flag flips) to its own receptacle.
     * has_flag is not a pathing input — it only affects whether the flag's
     * position is dragged along after the move. */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->creep.slow_ticks > 0) { t->creep.slow_ticks--; continue; }
        int nx = t->x, ny = t->y;
        if (!game_pathing_next_step(&s, t->x, t->y, t->owner,
                                    t->creep.visited_flag, &nx, &ny)) continue;
        t->x = nx; t->y = ny;
        PlayerID enemy = (t->owner == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
        if (!t->creep.visited_flag &&
            t->x == s.flags[enemy].x && t->y == s.flags[enemy].y) {
            t->creep.visited_flag = 1;
        }
        if (t->creep.has_flag) {
            for (int k = 0; k < 2; k++)
                if (s.flags[k].carried_by == i) {
                    s.flags[k].x = t->x; s.flags[k].y = t->y;
                }
        }
    }

    /* Pick up flags. Only creeps whose type has can_carry_flag set can
     * scoop up an enemy flag — both behaviors are independent, so a creep
     * with can_carry_flag=1 AND melee_damage>0 (e.g. BANANA) participates
     * here AND in the tower-damage loop below. */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (!ct_spec(t->creep.type)->can_carry_flag) continue;
        if (t->creep.has_flag) continue;
        for (int fi = 0; fi < 2; fi++) {
            Flag *f = &s.flags[fi];
            if (f->carried_by != -1) continue;
            if (t->x == f->x && t->y == f->y && t->owner != f->owner) {
                f->carried_by = i;
                f->at_home    = 0;
                t->creep.has_flag = 1;
                break;
            }
        }
    }

    /* Win check */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (!t->creep.has_flag) continue;
        if (t->x == s.receptacle_x[t->owner] && t->y == s.receptacle_y[t->owner]) {
            s.winner = t->owner;
            s.phase  = PHASE_GAME_OVER;
            return;
        }
    }

    /* Tower attacks */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_TOWER || !t->alive) continue;
        if (t->beam_ttl > 0) t->beam_ttl--;
        if (t->tower.build_turns > 0) continue;
        const TowerLevelStats *atk = &spec(t->tower.type)->level[t->tower.level - 1];
        if (atk->range == 0) continue;
        if (t->tower.cooldown > 0) { t->tower.cooldown--; continue; }
        /* Among enemy creeps within range of this tower, target the one
         * closest (Manhattan) to the tower owner's own flag — i.e. the
         * creep most about to grab it (or, after pickup, the carrier
         * itself, since a carried flag tracks the carrier's cell). */
        int fx = s.flags[t->owner].x, fy = s.flags[t->owner].y;
        int best = -1, best_dist = 999;
        for (int j = 0; j < s.thing_count; j++) {
            Thing *c = &s.things[j];
            if (c->tag != THING_CREEP || !c->alive) continue;
            if (c->owner == t->owner) continue;
            int dt = abs_i(c->x - t->x) + abs_i(c->y - t->y);
            if (dt > atk->range) continue;
            int df = abs_i(c->x - fx) + abs_i(c->y - fy);
            if (df < best_dist) { best_dist = df; best = j; }
        }
        if (best < 0) continue;
        Thing *tgt = &s.things[best];
        t->last_target_x = tgt->x; t->last_target_y = tgt->y;
        t->beam_ttl = 2;
        t->tower.cooldown = atk->cooldown;
        /* One crit roll per attack. AoE then applies the same `dmg` to every
         * cell in the splash — the design is "crit replaces dmg", not "each
         * target rolls independently". */
        int dmg = atk->dmg;
        if (atk->crit_chance > 0 && (rng_next(&s.rng_state) % 100u) < (uint32_t)atk->crit_chance)
            dmg = atk->crit_dmg;
        if (atk->aoe > 0) {
            for (int j = 0; j < s.thing_count; j++) {
                Thing *c = &s.things[j];
                if (c->tag != THING_CREEP || !c->alive) continue;
                if (c->owner == t->owner) continue;
                if (abs_i(c->x - tgt->x) <= atk->aoe &&
                    abs_i(c->y - tgt->y) <= atk->aoe) {
                    c->hp -= dmg;
                    if (atk->slow > 0) c->creep.slow_ticks = atk->slow;
                }
            }
        } else {
            tgt->hp -= dmg;
        }
    }

    /* Creep melee: any creep type with melee_damage > 0 deals that damage
     * to the first adjacent enemy tower it finds, once per tick. */
    static const int DX[4] = { 1, -1, 0,  0 };
    static const int DY[4] = { 0,  0, 1, -1 };
    for (int i = 0; i < s.thing_count; i++) {
        Thing *c = &s.things[i];
        if (c->tag != THING_CREEP || !c->alive) continue;
        int dmg = ct_spec(c->creep.type)->melee_damage;
        if (dmg <= 0) continue;
        for (int d = 0; d < 4; d++) {
            int nx = c->x + DX[d], ny = c->y + DY[d];
            int tid = tower_at(nx, ny);
            if (tid < 0) continue;
            Thing *target = &s.things[tid];
            if (target->owner == c->owner) continue;
            target->hp -= dmg;
            break;
        }
    }

    /* Resolve deaths */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (!t->alive) continue;
        if (t->hp > 0) continue;
        if (t->tag == THING_CREEP && t->creep.has_flag) {
            for (int fi = 0; fi < 2; fi++)
                if (s.flags[fi].carried_by == i) {
                    s.flags[fi].carried_by = -1;
                    s.flags[fi].x = t->x;
                    s.flags[fi].y = t->y;
                }
        }
        if (t->tag == THING_TOWER) s.grid[t->x][t->y].thing_id = -1;
        t->alive = 0;
        t->tag   = THING_NONE;
    }

    s.sim_tick++;
}

static void end_simulation(void) {
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_CREEP) continue;
        if (t->creep.has_flag) {
            for (int fi = 0; fi < 2; fi++)
                if (s.flags[fi].carried_by == i) {
                    s.flags[fi].carried_by = -1;
                    s.flags[fi].x = t->x;
                    s.flags[fi].y = t->y;
                }
        }
        t->alive = 0;
        t->tag   = THING_NONE;
    }
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_TOWER || !t->alive) continue;
        if (t->tower.build_turns > 0) t->tower.build_turns--;
        t->beam_ttl = 0;
    }
    for (int p = 0; p < 2; p++) {
        int inc = 0;
        for (int i = 0; i < s.thing_count; i++) {
            Thing *t = &s.things[i];
            if (t->tag != THING_TOWER || !t->alive) continue;
            if ((int)t->owner != p) continue;
            if (t->tower.build_turns > 0) continue;
            inc += spec(t->tower.type)->level[t->tower.level - 1].income;
        }
        s.players[p].income_per_turn = inc;
        s.players[p].resources += inc;
    }
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < s.players[p].creep_upgrade_count; i++) {
            CreepUpgrade *u = &s.players[p].creep_upgrades[i];
            if (u->purchased && !u->completed && u->turns_remaining > 0) {
                u->turns_remaining--;
                if (u->turns_remaining == 0) u->completed = 1;
            }
        }
    }
    s.turn++;
    s.phase = PHASE_PLAN_RED;
    s.selected_x = -1; s.selected_y = -1;
    s.placement_intent = -1;
}

void game_lock_in(void) {
    if (s.phase == PHASE_PLAN_RED)  { s.placement_intent = -1; s.selected_x = -1; s.selected_y = -1; s.phase = PHASE_PLAN_BLUE; }
    else if (s.phase == PHASE_PLAN_BLUE) { s.placement_intent = -1; s.selected_x = -1; s.selected_y = -1; start_simulation(); }
}

static int any_creeps_alive(void) {
    for (int i = 0; i < s.thing_count; i++)
        if (s.things[i].tag == THING_CREEP && s.things[i].alive) return 1;
    return 0;
}

void game_frame(void) {
    if (s.status_ttl > 0) s.status_ttl--;
    if (s.phase != PHASE_SIMULATE) return;
    if (s.sim_end_hold > 0) {
        s.sim_end_hold--;
        if (s.sim_end_hold == 0) end_simulation();
        return;
    }
    s.sim_frame_accum++;
    if (s.sim_frame_accum < SIM_FRAMES_PER_TICK) return;
    s.sim_frame_accum = 0;
    sim_one_tick();
    if (s.phase == PHASE_GAME_OVER) return;
    if (s.sim_tick >= SIM_TICKS_PER_TURN || !any_creeps_alive())
        s.sim_end_hold = 3 * SIM_FRAMES_PER_TICK;
}

/* ── Snapshot encode / decode ───────────────────────────── */

/* See header for the format. Designed to round-trip GameState through a
 * URL query param. Tower / upgrade references use string IDs from the
 * catalogs (so config edits that don't rename/remove anything keep working);
 * tower stats and creep spawn counts are intentionally omitted because they
 * come straight from the current catalogs at load time. */

static char phase_to_char(Phase p) {
    switch (p) {
        case PHASE_PLAN_RED:  return 'R';
        case PHASE_PLAN_BLUE: return 'B';
        case PHASE_SIMULATE:  return 'S';
        case PHASE_GAME_OVER: return 'O';
    }
    return 'R';
}

static Phase phase_from_char(char c) {
    switch (c) {
        case 'B': return PHASE_PLAN_BLUE;
        case 'S': return PHASE_SIMULATE;
        case 'O': return PHASE_GAME_OVER;
        default:  return PHASE_PLAN_RED;
    }
}

/* APPEND macro: write fmt+args to (p..end). Returns -1 from the enclosing
 * function on overflow. Used by game_snapshot_encode below to keep the
 * "bounded write or bail" pattern from cluttering each snprintf site. */
#define APPEND(...) do {                                                  \
    int _n = snprintf(p, (size_t)(end - p), __VA_ARGS__);                 \
    if (_n < 0 || _n >= end - p) return -1;                               \
    p += _n;                                                              \
} while (0)

int game_snapshot_encode(char *out, int out_size) {
    if (!out || out_size <= 0) return -1;
    char *p = out;
    char *end = out + out_size;

    APPEND("v1~T%d~P%c", s.turn, phase_to_char(s.phase));

    for (int pi = 0; pi < 2; pi++) {
        APPEND("~%c%d:%d~%c",
               pi == PLAYER_RED ? 'R' : 'B',
               s.players[pi].resources,
               s.players[pi].income_per_turn,
               pi == PLAYER_RED ? 'r' : 'b');
        int first = 1;
        for (int i = 0; i < s.players[pi].creep_upgrade_count; i++) {
            const CreepUpgrade *u = &s.players[pi].creep_upgrades[i];
            if (!u->purchased) continue;   /* default state: omit to save bytes */
            const char *id = creep_config_get()->upgrades[i].id;
            APPEND("%s%s:%d:%d:%d",
                   first ? "" : ",", id,
                   u->purchased, u->completed, u->turns_remaining);
            first = 0;
        }
    }

    APPEND("~W");
    int first = 1;
    for (int i = 0; i < s.thing_count; i++) {
        const Thing *t = &s.things[i];
        if (t->tag != THING_TOWER || !t->alive) continue;
        const char *id = tower_config_get()->towers[t->tower.type].id;
        APPEND("%s%c:%s:%d:%d:%d:%d:%d",
               first ? "" : ",",
               t->owner == PLAYER_RED ? 'R' : 'B',
               id, t->x, t->y, t->tower.level, t->hp, t->tower.build_turns);
        first = 0;
    }

    APPEND("~F%d:%d:%d:%d:%d:%d",
           s.flags[PLAYER_RED].x, s.flags[PLAYER_RED].y, s.flags[PLAYER_RED].at_home,
           s.flags[PLAYER_BLUE].x, s.flags[PLAYER_BLUE].y, s.flags[PLAYER_BLUE].at_home);

    /* Encode as signed: read_int parses into `int`, which overflows for
     * uint32 values > INT_MAX. The signed reinterpretation round-trips the
     * bit pattern exactly via the (uint32_t) cast on load. */
    APPEND("~N%d", (int)(int32_t)s.rng_state);

    return (int)(p - out);
}

#undef APPEND

static const char *read_int(const char *src, int *out) {
    int neg = 0;
    if (*src == '-') { neg = 1; src++; }
    int v = 0;
    while (*src >= '0' && *src <= '9') { v = v * 10 + (*src - '0'); src++; }
    *out = neg ? -v : v;
    return src;
}

static const char *read_id(const char *src, char *buf, int buf_size) {
    int i = 0;
    while (*src && *src != ':' && *src != ',' && *src != '~') {
        if (i < buf_size - 1) buf[i++] = *src;
        src++;
    }
    buf[i] = 0;
    return src;
}

/* Skip to the next record (after ',') or end of section ('~'). */
static const char *skip_record(const char *src) {
    while (*src && *src != ',' && *src != '~') src++;
    if (*src == ',') src++;
    return src;
}

static const char *parse_upgrades(const char *src, PlayerID owner) {
    while (*src && *src != '~') {
        char id[CREEP_UPGRADE_ID_MAX];
        src = read_id(src, id, sizeof(id));
        if (*src != ':') { src = skip_record(src); continue; }
        src++;
        int purchased, completed, turns;
        src = read_int(src, &purchased); if (*src == ':') src++;
        src = read_int(src, &completed); if (*src == ':') src++;
        src = read_int(src, &turns);
        int idx = creep_config_lookup_upgrade(id);
        if (idx >= 0 && idx < s.players[owner].creep_upgrade_count) {
            CreepUpgrade *u = &s.players[owner].creep_upgrades[idx];
            u->purchased       = purchased;
            u->completed       = completed;
            u->turns_remaining = turns;
        }
        if (*src == ',') src++;
    }
    return src;
}

static void place_snapshot_tower(PlayerID owner, const char *id,
                                 int x, int y, int level, int hp, int bt) {
    TowerType type = tower_config_lookup(id);
    if (type < 0) return;                                /* unknown id from older cfg */
    if (x < 0 || y < 0 || x >= s.grid_w || y >= s.grid_h) return;
    if (s.grid[x][y].zone == ZONE_DEBRIS) return;        /* map shrunk into us */
    if (s.grid[x][y].thing_id != -1) return;             /* slot taken */
    int max_lvl = spec(type)->level_count;
    if (level < 1) level = 1;
    if (level > max_lvl) level = max_lvl;
    if (bt < 0) bt = 0;
    int idx = alloc_thing();
    if (idx < 0) return;
    Thing *t = &s.things[idx];
    memset(t, 0, sizeof(*t));
    t->tag         = THING_TOWER;
    t->owner       = owner;
    t->x = x; t->y = y;
    t->max_hp      = spec(type)->level[level - 1].hp;
    t->hp          = (hp > 0 && hp <= t->max_hp) ? hp : t->max_hp;
    t->alive       = 1;
    t->tower.type  = type;
    t->tower.level = level;
    t->tower.build_turns = bt;
    s.grid[x][y].thing_id = idx;
}

static const char *parse_towers(const char *src) {
    while (*src && *src != '~') {
        char owner_c = *src;
        if (owner_c != 'R' && owner_c != 'B') { src = skip_record(src); continue; }
        src++;
        if (*src != ':') { src = skip_record(src); continue; }
        src++;
        char id[TOWER_ID_MAX];
        src = read_id(src, id, sizeof(id));
        if (*src != ':') { src = skip_record(src); continue; }
        src++;
        int x, y, level, hp, bt;
        src = read_int(src, &x);      if (*src == ':') src++;
        src = read_int(src, &y);      if (*src == ':') src++;
        src = read_int(src, &level);  if (*src == ':') src++;
        src = read_int(src, &hp);     if (*src == ':') src++;
        src = read_int(src, &bt);
        place_snapshot_tower(owner_c == 'R' ? PLAYER_RED : PLAYER_BLUE,
                             id, x, y, level, hp, bt);
        if (*src == ',') src++;
    }
    return src;
}

static const char *parse_flags(const char *src) {
    int v[6] = {0};
    for (int i = 0; i < 6; i++) {
        src = read_int(src, &v[i]);
        if (*src == ':') src++;
    }
    int coords[2][2] = { { v[0], v[1] }, { v[3], v[4] } };
    int homes[2]    = { v[2], v[5] };
    for (int p = 0; p < 2; p++) {
        int x = coords[p][0], y = coords[p][1];
        if (x < 0 || y < 0 || x >= s.grid_w || y >= s.grid_h) continue;
        s.flags[p].x          = x;
        s.flags[p].y          = y;
        s.flags[p].at_home    = homes[p] ? 1 : 0;
        s.flags[p].carried_by = -1;
    }
    return src;
}

int game_snapshot_load(const char *src) {
    if (!src) return -1;
    /* Reset everything via the existing zero-state init, which seeds defaults
     * from the *current* catalogs (towers, creep upgrades, map). The
     * snapshot then overlays whatever it wants on top of that fresh state. */
    game_init_state();

    if (src[0] != 'v' || src[1] != '1') return -1;
    src += 2;

    while (*src == '~') {
        src++;
        char tag = *src++;
        if (!tag) return -1;
        switch (tag) {
            case 'T': {
                int v; src = read_int(src, &v);
                if (v < 1) v = 1;
                s.turn = v;
                break;
            }
            case 'P': {
                s.phase = phase_from_char(*src);
                if (*src) src++;
                break;
            }
            case 'R': case 'B': {
                PlayerID owner = (tag == 'R') ? PLAYER_RED : PLAYER_BLUE;
                int res, inc;
                src = read_int(src, &res); if (*src == ':') src++;
                src = read_int(src, &inc);
                s.players[owner].resources       = res;
                s.players[owner].income_per_turn = inc;
                break;
            }
            case 'r': src = parse_upgrades(src, PLAYER_RED);  break;
            case 'b': src = parse_upgrades(src, PLAYER_BLUE); break;
            case 'W': src = parse_towers(src);                break;
            case 'F': src = parse_flags(src);                 break;
            case 'N': {
                int v; src = read_int(src, &v);
                s.rng_state = (uint32_t)v;
                break;
            }
            default:
                /* Unknown section: skip to next ~ so future format
                 * extensions don't break old clients. */
                while (*src && *src != '~') src++;
                break;
        }
    }

    /* If the snapshot says we're in SIMULATE, the recorded state is the
     * "just after start_simulation" point — towers reset, creeps about to
     * spawn. We didn't save creeps (they're deterministic), so re-derive
     * them here. start_simulation also zeroes sim_tick / beam_ttl / cooldown
     * which matches the snapshot's implicit "fresh sim" semantics. */
    if (s.phase == PHASE_SIMULATE) start_simulation();

    return 0;
}
