#include "game.h"
#include <string.h>
#include <stdio.h>

static GameState s;

/* ── Tower data ─────────────────────────────────────────── */

typedef struct {
    int  cost;
    int  hp;
    int  build_turns;
    char code;
    const char *name;
} TowerSpec;

static const TowerSpec TOWERS[TOWER_TYPE_COUNT] = {
    [TOWER_BLOCKER]   = { 20, 100, 1, 'B', "Blocker"   },
    [TOWER_GUNNER]   = { 30, 50, 0, 'G', "Gunner"   },
    [TOWER_SLAMMER]  = { 50, 50, 2, 'S', "Slammer"  },
    [TOWER_RESOURCE] = { 80, 30, 3, 'R', "Resource" },
};

typedef struct {
    int dmg;
    int range;
    int aoe_radius;
    int slow;
    int cooldown;
} TowerAttack;

static TowerAttack tower_attack(TowerType t, int level) {
    TowerAttack a = {0};
    if (t == TOWER_GUNNER) {
        a.dmg      = (level == 1) ? 10 : 15;
        a.range    = (level == 1) ? 3  : 4;
        a.cooldown = 2;
    } else if (t == TOWER_SLAMMER) {
        a.dmg        = (level == 1) ? 5 : 8;
        a.range      = 3;
        a.aoe_radius = (level == 1) ? 1 : 2;
        a.slow       = 2;
        a.cooldown   = 3;
    }
    return a;
}

int         game_tower_cost(TowerType t) { return TOWERS[t].cost; }
const char *game_tower_name(TowerType t) { return TOWERS[t].name; }
char        game_tower_code(TowerType t) { return TOWERS[t].code; }

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

/* Creep pathing per docs/game-design.md §10. Conceptually the goal "line" for
 * a creep is spawn → enemy_flag → receptacle, with the flag's current cell
 * (when on the ground) inserted between the two halves. Each tick the creep
 * takes the shortest unblocked path to the closest cell on that line it hasn't
 * yet visited. Same rule for every creep type and whether or not the creep
 * carries the flag — flag pickup is a separate side-effect of arriving on the
 * flag's cell, not a pathing branch.
 *
 * Implementation: callers fill bfs_goal[][] with the set of acceptable target
 * cells, then bfs_step_in finds the shortest path from (sx,sy) to any of
 * them and returns the first step. */
static int     bfs_parent[MAX_GRID_W][MAX_GRID_H];
static int     bfs_qx[MAX_GRID_W * MAX_GRID_H];
static int     bfs_qy[MAX_GRID_W * MAX_GRID_H];
static uint8_t bfs_goal[MAX_GRID_W][MAX_GRID_H];

static void bfs_clear_goals_in(const GameState *gs) {
    for (int x = 0; x < gs->grid_w; x++)
        for (int y = 0; y < gs->grid_h; y++)
            bfs_goal[x][y] = 0;
}

static int bfs_step_in(const GameState *gs, int sx, int sy,
                       int *step_x, int *step_y) {
    if (sx < 0 || sy < 0 || sx >= gs->grid_w || sy >= gs->grid_h) return 0;
    if (bfs_goal[sx][sy]) {
        if (step_x) { *step_x = sx; *step_y = sy; }
        return 1;
    }
    int W = gs->grid_w, H = gs->grid_h;
    for (int x = 0; x < W; x++)
        for (int y = 0; y < H; y++)
            bfs_parent[x][y] = 0;
    int qh = 0, qt = 0;
    bfs_qx[qt] = sx; bfs_qy[qt] = sy; qt++;
    bfs_parent[sx][sy] = sy * W + sx + 1;
    static const int DX[4] = { 1, -1, 0,  0 };
    static const int DY[4] = { 0,  0, 1, -1 };
    while (qh < qt) {
        int cx = bfs_qx[qh], cy = bfs_qy[qh]; qh++;
        for (int i = 0; i < 4; i++) {
            int nx = cx + DX[i], ny = cy + DY[i];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            if (bfs_parent[nx][ny]) continue;
            if (!walkable_in(gs, nx, ny)) continue;
            bfs_parent[nx][ny] = cy * W + cx + 1;
            if (bfs_goal[nx][ny]) {
                int rx = nx, ry = ny;
                while (1) {
                    int p  = bfs_parent[rx][ry] - 1;
                    int px = p % W, py = p / W;
                    if (px == sx && py == sy) {
                        if (step_x) { *step_x = rx; *step_y = ry; }
                        return 1;
                    }
                    rx = px; ry = py;
                }
            }
            bfs_qx[qt] = nx; bfs_qy[qt] = ny; qt++;
        }
    }
    return 0;
}

void game_build_path(GameState *gs, PlayerID p) {
    PlayerID enemy = (p == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
    int x = gs->spawn_x[p], y = gs->spawn_y[p];
    int fx = gs->flags[enemy].x, fy = gs->flags[enemy].y;
    int rx = gs->receptacle_x[p], ry = gs->receptacle_y[p];
    int n = 0;
    gs->path_x[p][n] = x; gs->path_y[p][n] = y; n++;
    while (x != fx && n < MAX_PATH) { x += (fx > x) ? 1 : -1; gs->path_x[p][n] = x; gs->path_y[p][n] = y; n++; }
    while (y != fy && n < MAX_PATH) { y += (fy > y) ? 1 : -1; gs->path_x[p][n] = x; gs->path_y[p][n] = y; n++; }
    while (x != rx && n < MAX_PATH) { x += (rx > x) ? 1 : -1; gs->path_x[p][n] = x; gs->path_y[p][n] = y; n++; }
    while (y != ry && n < MAX_PATH) { y += (ry > y) ? 1 : -1; gs->path_x[p][n] = x; gs->path_y[p][n] = y; n++; }
    gs->path_len[p] = n;
}

int game_pathing_next_step(const GameState *gs, int creep_x, int creep_y,
                           PlayerID owner, int path_progress,
                           int *out_x, int *out_y) {
    bfs_clear_goals_in(gs);
    for (int i = path_progress + 1; i < gs->path_len[owner]; i++)
        bfs_goal[gs->path_x[owner][i]][gs->path_y[owner][i]] = 1;
    PlayerID enemy = (owner == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
    const Flag *f = &gs->flags[enemy];
    if (f->carried_by == -1)
        bfs_goal[f->x][f->y] = 1;
    /* The creep's own cell is never a goal — otherwise the BFS short-circuits
     * with "no movement" when the creep is currently sitting on an unvisited
     * line cell (e.g. at the flag, before the path_progress has been advanced
     * past it). */
    if (in_bounds(creep_x, creep_y)) bfs_goal[creep_x][creep_y] = 0;
    return bfs_step_in(gs, creep_x, creep_y, out_x, out_y);
}

/* Placement is legal only if both halves of every player's loop remain
 * walkable: spawn → enemy flag → own receptacle. Either half being severed
 * would leave creeps unable to complete their objective. */
static int paths_valid(void) {
    for (int p = 0; p < 2; p++) {
        PlayerID enemy = (p == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
        bfs_clear_goals_in(&s);
        bfs_goal[s.flags[enemy].x][s.flags[enemy].y] = 1;
        if (!bfs_step_in(&s, s.spawn_x[p], s.spawn_y[p], NULL, NULL)) return 0;
        bfs_clear_goals_in(&s);
        bfs_goal[s.receptacle_x[p]][s.receptacle_y[p]] = 1;
        if (!bfs_step_in(&s, s.flags[enemy].x, s.flags[enemy].y, NULL, NULL)) return 0;
    }
    return 1;
}

/* ── Creep upgrade init ─────────────────────────────────── */

static void init_creep_upgrades(Player *p) {
    p->creep_upgrade_count = 4;
    CreepUpgrade *u;

    u = &p->creep_upgrades[0];
    memset(u, 0, sizeof(*u));
    u->id = 0; u->cost = 30; u->research_turns = 1; u->add_retrievers = 1;
    strcpy(u->description, "+1 Retriever");

    u = &p->creep_upgrades[1];
    memset(u, 0, sizeof(*u));
    u->id = 1; u->cost = 40; u->research_turns = 1; u->add_siege = 2;
    strcpy(u->description, "+2 Siege");

    u = &p->creep_upgrades[2];
    memset(u, 0, sizeof(*u));
    u->id = 2; u->cost = 60; u->research_turns = 2; u->add_retrievers = 2;
    strcpy(u->description, "+2 Retrievers");

    u = &p->creep_upgrades[3];
    memset(u, 0, sizeof(*u));
    u->id = 3; u->cost = 70; u->research_turns = 2; u->add_siege = 2;
    strcpy(u->description, "+2 Siege II");
}

/* ── Init ───────────────────────────────────────────────── */

void game_init(void) {
    memset(&s, 0, sizeof(s));
    s.grid_w = 30;
    s.grid_h = 20;
    s.turn = 1;
    s.phase = PHASE_PLAN_RED;
    s.selected_x = -1; s.selected_y = -1;
    s.placement_intent = -1;
    s.winner = -1;

    for (int x = 0; x < s.grid_w; x++) {
        for (int y = 0; y < s.grid_h; y++) {
            s.grid[x][y].thing_id = -1;
            if      (x < 10)  s.grid[x][y].zone = ZONE_RED;
            else if (x >= 20) s.grid[x][y].zone = ZONE_BLUE;
            else              s.grid[x][y].zone = ZONE_NEUTRAL;
        }
    }

    s.players[PLAYER_RED].resources       = 100;
    s.players[PLAYER_RED].income_per_turn = 0;
    s.players[PLAYER_BLUE].resources       = 100;
    s.players[PLAYER_BLUE].income_per_turn = 20;
    init_creep_upgrades(&s.players[PLAYER_RED]);
    init_creep_upgrades(&s.players[PLAYER_BLUE]);

    s.spawn_x[PLAYER_RED]      = 0;  s.spawn_y[PLAYER_RED]      = 9;
    s.spawn_x[PLAYER_BLUE]     = 29; s.spawn_y[PLAYER_BLUE]     = 11;
    s.receptacle_x[PLAYER_RED]  = 4;  s.receptacle_y[PLAYER_RED]  = 4;
    s.receptacle_x[PLAYER_BLUE] = 25; s.receptacle_y[PLAYER_BLUE] = 15;

    s.flags[PLAYER_RED].x = 4;  s.flags[PLAYER_RED].y = 15;
    s.flags[PLAYER_RED].owner      = PLAYER_RED;
    s.flags[PLAYER_RED].carried_by = -1;
    s.flags[PLAYER_RED].at_home    = 1;

    s.flags[PLAYER_BLUE].x = 25; s.flags[PLAYER_BLUE].y = 4;
    s.flags[PLAYER_BLUE].owner      = PLAYER_BLUE;
    s.flags[PLAYER_BLUE].carried_by = -1;
    s.flags[PLAYER_BLUE].at_home    = 1;

    /* Demarcated path: cell-by-cell L-shaped loop spawn → enemy flag →
     * own receptacle. Visible to both players and serves as the goal set
     * for creep pathing per docs/game-design.md §10. */
    game_build_path(&s, PLAYER_RED);
    game_build_path(&s, PLAYER_BLUE);
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
    int cost = TOWERS[type].cost;
    if (s.players[p].resources < cost) { set_status("Not enough resources"); return; }
    if (!placement_valid(gx, gy, p))   { set_status("Invalid placement");    return; }
    int id = alloc_thing();
    if (id < 0) return;
    Thing *t = &s.things[id];
    memset(t, 0, sizeof(*t));
    t->tag    = THING_TOWER;
    t->owner  = p;
    t->x      = gx; t->y = gy;
    t->hp     = TOWERS[type].hp;
    t->max_hp = TOWERS[type].hp;
    t->alive  = 1;
    t->tower.type        = type;
    t->tower.level       = 1;
    t->tower.build_turns = TOWERS[type].build_turns;
    s.grid[gx][gy].thing_id = id;
    s.players[p].resources -= cost;
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
    if (type < 0 || type >= TOWER_TYPE_COUNT) { s.placement_intent = -1; return; }
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
    if (t->owner != p)              { set_status("Not your tower");      return; }
    if (t->tower.level >= 2)        { set_status("Max level");           return; }
    if (t->tower.build_turns > 0)   { set_status("Still building");      return; }
    int cost = TOWERS[t->tower.type].cost;
    if (s.players[p].resources < cost) { set_status("Not enough resources"); return; }
    s.players[p].resources -= cost;
    t->tower.level = 2;
    t->tower.build_turns = 1;
    t->max_hp += 20;
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
    if (u->purchased)                  { set_status("Already purchased"); return; }
    if (s.players[p].resources < u->cost) { set_status("Not enough resources"); return; }
    s.players[p].resources -= u->cost;
    u->purchased = 1;
    u->turns_remaining = u->research_turns;
}

/* ── Simulation ─────────────────────────────────────────── */

static int count_spawns(PlayerID p, CreepType ct) {
    int n = 0;
    if (ct == CREEP_RETRIEVER) n = 0; /* base */
    for (int i = 0; i < s.players[p].creep_upgrade_count; i++) {
        CreepUpgrade *u = &s.players[p].creep_upgrades[i];
        if (!u->completed) continue;
        if (ct == CREEP_RETRIEVER) n += u->add_retrievers;
        else if (ct == CREEP_SIEGE) n += u->add_siege;
    }
    return n;
}

static void spawn_creep(PlayerID p, CreepType ct) {
    int id = alloc_thing();
    if (id < 0) return;
    Thing *t = &s.things[id];
    memset(t, 0, sizeof(*t));
    t->tag    = THING_CREEP;
    t->owner  = p;
    t->x      = s.spawn_x[p];
    t->y      = s.spawn_y[p];
    t->hp     = (ct == CREEP_SIEGE) ? 40 : 20;
    t->max_hp = t->hp;
    t->alive  = 1;
    t->creep.type = ct;
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
    for (int p = 0; p < 2; p++) {
        for (int n = count_spawns((PlayerID)p, CREEP_RETRIEVER); n > 0; n--)
            spawn_creep((PlayerID)p, CREEP_RETRIEVER);
        for (int n = count_spawns((PlayerID)p, CREEP_SIEGE); n > 0; n--)
            spawn_creep((PlayerID)p, CREEP_SIEGE);
    }
}

static void sim_one_tick(void) {
    /* Move creeps. Unified per docs/game-design.md §10: every creep takes one
     * step toward the closest unvisited cell on its line (spawn → enemy flag →
     * receptacle), with the flag's current cell added as a goal when on the
     * ground. has_flag is not a pathing input — it only affects whether the
     * flag's position is dragged along after the move. */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->creep.slow_ticks > 0) { t->creep.slow_ticks--; continue; }
        int nx = t->x, ny = t->y;
        if (!game_pathing_next_step(&s, t->x, t->y, t->owner,
                                    t->creep.path_progress, &nx, &ny)) continue;
        t->x = nx; t->y = ny;
        /* path_progress = smallest line index > current progress whose cell
         * coords match the creep's new cell. Smallest (rather than largest)
         * makes traversal robust if the line ever revisits a coord — the
         * progress walks forward one position at a time, not jumping ahead. */
        PlayerID p = t->owner;
        for (int k = t->creep.path_progress + 1; k < s.path_len[p]; k++) {
            if (s.path_x[p][k] == t->x && s.path_y[p][k] == t->y) {
                t->creep.path_progress = k;
                break;
            }
        }
        if (t->creep.has_flag) {
            for (int k = 0; k < 2; k++)
                if (s.flags[k].carried_by == i) {
                    s.flags[k].x = t->x; s.flags[k].y = t->y;
                }
        }
    }

    /* Pick up flags */
    for (int i = 0; i < s.thing_count; i++) {
        Thing *t = &s.things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        if (t->creep.type != CREEP_RETRIEVER) continue;
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
        if (t->tower.type == TOWER_RESOURCE) continue;
        if (t->tower.cooldown > 0) { t->tower.cooldown--; continue; }
        TowerAttack atk = tower_attack(t->tower.type, t->tower.level);
        int best = -1, best_dist = 999;
        for (int j = 0; j < s.thing_count; j++) {
            Thing *c = &s.things[j];
            if (c->tag != THING_CREEP || !c->alive) continue;
            if (c->owner == t->owner) continue;
            int d = abs_i(c->x - t->x) + abs_i(c->y - t->y);
            if (d > atk.range) continue;
            if (d < best_dist) { best_dist = d; best = j; }
        }
        if (best < 0) continue;
        Thing *tgt = &s.things[best];
        t->last_target_x = tgt->x; t->last_target_y = tgt->y;
        t->beam_ttl = 2;
        t->tower.cooldown = atk.cooldown;
        if (atk.aoe_radius > 0) {
            for (int j = 0; j < s.thing_count; j++) {
                Thing *c = &s.things[j];
                if (c->tag != THING_CREEP || !c->alive) continue;
                if (c->owner == t->owner) continue;
                if (abs_i(c->x - tgt->x) <= atk.aoe_radius &&
                    abs_i(c->y - tgt->y) <= atk.aoe_radius) {
                    c->hp -= atk.dmg;
                    if (atk.slow > 0) c->creep.slow_ticks = atk.slow;
                }
            }
        } else {
            tgt->hp -= atk.dmg;
        }
    }

    /* Siege creep melee */
    static const int DX[4] = { 1, -1, 0,  0 };
    static const int DY[4] = { 0,  0, 1, -1 };
    for (int i = 0; i < s.thing_count; i++) {
        Thing *c = &s.things[i];
        if (c->tag != THING_CREEP || !c->alive) continue;
        if (c->creep.type != CREEP_SIEGE) continue;
        for (int d = 0; d < 4; d++) {
            int nx = c->x + DX[d], ny = c->y + DY[d];
            int tid = tower_at(nx, ny);
            if (tid < 0) continue;
            Thing *target = &s.things[tid];
            if (target->owner == c->owner) continue;
            target->hp -= 5;
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
        int inc = 20;
        for (int i = 0; i < s.thing_count; i++) {
            Thing *t = &s.things[i];
            if (t->tag != THING_TOWER || !t->alive) continue;
            if ((int)t->owner != p) continue;
            if (t->tower.type != TOWER_RESOURCE) continue;
            if (t->tower.build_turns > 0) continue;
            inc += (t->tower.level == 1) ? 10 : 20;
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
