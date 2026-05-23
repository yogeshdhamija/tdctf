#include "render.h"
#include "../game/creep_config.h"   /* CREEP_TYPE_MAX_COUNT for crowding-badge sizing */
#include "../platform/platform.h"
#include <stdio.h>
#include <string.h>

typedef struct { int id, x, y, w, h; } BtnRect;
static BtnRect g_btns[64];
static int     g_btn_count;

static void btn_push(int id, int x, int y, int w, int h) {
    if (g_btn_count >= 64) return;
    g_btns[g_btn_count].id = id;
    g_btns[g_btn_count].x  = x;
    g_btns[g_btn_count].y  = y;
    g_btns[g_btn_count].w  = w;
    g_btns[g_btn_count].h  = h;
    g_btn_count++;
}

int render_button_at(int px, int py) {
    for (int i = 0; i < g_btn_count; i++) {
        BtnRect *r = &g_btns[i];
        if (px >= r->x && px < r->x + r->w && py >= r->y && py < r->y + r->h)
            return r->id;
    }
    return BTN_NONE;
}

static uint32_t zone_color(ZoneType z) {
    switch (z) {
        case ZONE_RED:    return 0x1A0808;
        case ZONE_BLUE:   return 0x08081A;
        case ZONE_DEBRIS: return 0x222222;
        default:          return 0x141414;
    }
}

static uint32_t player_color(PlayerID p)     { return p == PLAYER_RED ? 0xCC4444 : 0x4477CC; }
static uint32_t player_color_dim(PlayerID p) { return p == PLAYER_RED ? 0x661818 : 0x182455; }

static void draw_button(int id, int x, int y, int w, int h,
                        const char *label, int active, int enabled) {
    uint32_t fill   = enabled ? (active ? 0x445588 : 0x2A2A2A) : 0x1A1A1A;
    uint32_t border = enabled ? 0x888888 : 0x444444;
    uint32_t txt    = enabled ? 0xEEEEEE : 0x666666;
    plat_fill_rect(x, y, w, h, fill);
    plat_draw_rect(x, y, w, h, border);
    plat_draw_text(x + 6, y + (h - 14) / 2 + 1, label, txt);
    btn_push(id, x, y, w, h);
}

void render_frame(const GameState *gs) {
    g_btn_count = 0;
    int gw = gs->grid_w * CELL_SIZE;
    int gh = gs->grid_h * CELL_SIZE;
    char buf[96];

    plat_clear(0x111111);

    /* Grid zones */
    for (int x = 0; x < gs->grid_w; x++)
        for (int y = 0; y < gs->grid_h; y++)
            plat_fill_rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE,
                           zone_color(gs->grid[x][y].zone));

    /* Grid lines */
    for (int x = 0; x <= gs->grid_w; x++)
        plat_draw_line(x * CELL_SIZE, 0, x * CELL_SIZE, gh, 0x303030);
    for (int y = 0; y <= gs->grid_h; y++)
        plat_draw_line(0, y * CELL_SIZE, gw, y * CELL_SIZE, 0x303030);

    /* Receptacles */
    for (int p = 0; p < 2; p++) {
        int rx = gs->receptacle_x[p] * CELL_SIZE;
        int ry = gs->receptacle_y[p] * CELL_SIZE;
        plat_draw_rect(rx + 2, ry + 2, CELL_SIZE - 4, CELL_SIZE - 4, player_color((PlayerID)p));
        plat_draw_rect(rx + 5, ry + 5, CELL_SIZE - 10, CELL_SIZE - 10, player_color((PlayerID)p));
    }

    /* Spawn markers: nested circles in the player's color — non-directional,
     * visually paired with the nested-rectangle receptacle marker. */
    for (int p = 0; p < 2; p++) {
        int cx = gs->spawn_x[p] * CELL_SIZE + CELL_SIZE / 2;
        int cy = gs->spawn_y[p] * CELL_SIZE + CELL_SIZE / 2;
        uint32_t col = player_color((PlayerID)p);
        plat_draw_circle(cx, cy, CELL_SIZE / 2 - 2, col);
        plat_draw_circle(cx, cy, CELL_SIZE / 2 - 5, col);
    }

    /* Flags (only if not carried) */
    for (int i = 0; i < 2; i++) {
        const Flag *f = &gs->flags[i];
        if (f->carried_by != -1) continue;
        uint32_t col = player_color(f->owner);
        int fx = f->x * CELL_SIZE + 6;
        int fy = f->y * CELL_SIZE + 4;
        plat_draw_line(fx, fy, fx, fy + 24, col);
        plat_draw_triangle(fx, fy, fx + 16, fy + 6, fx, fy + 12, col);
    }

    /* Towers */
    for (int i = 0; i < gs->thing_count; i++) {
        const Thing *t = &gs->things[i];
        if (t->tag != THING_TOWER || !t->alive) continue;
        char code = game_tower_code(t->tower.type);
        uint32_t col = (t->tower.build_turns > 0) ? player_color_dim(t->owner) : player_color(t->owner);
        if (t->tower.build_turns > 0)
            snprintf(buf, sizeof(buf), "%c%d*", code, t->tower.level);
        else
            snprintf(buf, sizeof(buf), "%c%d", code, t->tower.level);
        plat_draw_text(t->x * CELL_SIZE + 7, t->y * CELL_SIZE + 8, buf, col);
        if (t->hp < t->max_hp) {
            int barw = (CELL_SIZE - 6) * t->hp / (t->max_hp > 0 ? t->max_hp : 1);
            plat_fill_rect(t->x * CELL_SIZE + 3, t->y * CELL_SIZE + CELL_SIZE - 4,
                           CELL_SIZE - 6, 2, 0x440000);
            plat_fill_rect(t->x * CELL_SIZE + 3, t->y * CELL_SIZE + CELL_SIZE - 4,
                           barw, 2, 0x44CC44);
        }
    }

    /* Tower attack beams */
    for (int i = 0; i < gs->thing_count; i++) {
        const Thing *t = &gs->things[i];
        if (t->tag != THING_TOWER || !t->alive) continue;
        if (t->beam_ttl <= 0) continue;
        int x1 = t->x * CELL_SIZE + CELL_SIZE/2;
        int y1 = t->y * CELL_SIZE + CELL_SIZE/2;
        int x2 = t->last_target_x * CELL_SIZE + CELL_SIZE/2;
        int y2 = t->last_target_y * CELL_SIZE + CELL_SIZE/2;
        plat_draw_line(x1, y1, x2, y2, 0xFFEE88);
    }

    /* Creeps. Visual differentiation: any creep type with melee_damage > 0
     * is rendered "heavy" (larger circle, dim color) — a generic stand-in
     * for "fighter" creeps; everything else renders "light". This keeps
     * arbitrary new creep types from the config readable without per-type
     * art. */
    static int creep_cnt[MAX_GRID_W][MAX_GRID_H][2][CREEP_TYPE_MAX_COUNT];
    memset(creep_cnt, 0, sizeof(creep_cnt));
    for (int i = 0; i < gs->thing_count; i++) {
        const Thing *t = &gs->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        creep_cnt[t->x][t->y][t->owner][t->creep.type]++;
        int heavy = game_creep_active_melee_damage(t->owner, t->creep.type) > 0;
        uint32_t col = heavy ? player_color_dim(t->owner) : player_color(t->owner);
        int cx = t->x * CELL_SIZE + CELL_SIZE/2;
        int cy = t->y * CELL_SIZE + CELL_SIZE/2;
        int r  = heavy ? 7 : 5;
        plat_fill_circle(cx, cy, r, col);
        if (t->creep.slow_ticks > 0)
            plat_draw_circle(cx, cy, r + 2, 0x88CCFF);
        if (t->creep.has_flag) {
            PlayerID flag_owner = (t->owner == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
            uint32_t fcol = player_color(flag_owner);
            plat_draw_line(cx - 7, cy - 12, cx - 7, cy + 2, fcol);
            plat_draw_triangle(cx - 7, cy - 12, cx + 3, cy - 9, cx - 7, cy - 6, fcol);
        }
    }

    /* Per-cell crowding badge: when more than one creep shares a cell the
     * individual circles fully overlap and the player can't see the count
     * or the type mix. Draw a compact per-owner label like "2R", "2R1S",
     * or "3B" using each creep type's `code` glyph from the catalog. */
    int type_count = game_creep_type_count();
    for (int x = 0; x < gs->grid_w; x++) {
        for (int y = 0; y < gs->grid_h; y++) {
            int total = 0;
            for (int p = 0; p < 2; p++)
                for (int ct = 0; ct < type_count; ct++)
                    total += creep_cnt[x][y][p][ct];
            if (total < 2) continue;
            int by = y * CELL_SIZE + 1;
            for (int p = 0; p < 2; p++) {
                int per_player = 0;
                for (int ct = 0; ct < type_count; ct++) per_player += creep_cnt[x][y][p][ct];
                if (per_player == 0) continue;
                buf[0] = 0;
                size_t used = 0;
                for (int ct = 0; ct < type_count; ct++) {
                    int n = creep_cnt[x][y][p][ct];
                    if (n == 0) continue;
                    /* Live creeps of (player, type) must have a merged
                     * active profile — they couldn't have spawned
                     * otherwise — so the code lookup always resolves. */
                    char code = game_creep_active_code((PlayerID)p, ct);
                    int w = snprintf(buf + used, sizeof(buf) - used,
                                     "%d%c", n, code);
                    if (w < 0 || (size_t)w >= sizeof(buf) - used) break;
                    used += w;
                }
                int label_w = (int)strlen(buf) * 8 + 2;
                plat_fill_rect(x * CELL_SIZE + 1, by, label_w, 12, 0x000000);
                plat_draw_text(x * CELL_SIZE + 2, by, buf, player_color((PlayerID)p));
                by += 13;
            }
        }
    }

    /* Selection highlight */
    if (gs->selected_x >= 0 && gs->selected_y >= 0) {
        plat_draw_rect(gs->selected_x * CELL_SIZE + 1, gs->selected_y * CELL_SIZE + 1,
                       CELL_SIZE - 2, CELL_SIZE - 2, 0xFFFFFF);
    }

    /* Placement-intent hint banner */
    if (gs->placement_intent >= 0 && (gs->phase == PHASE_PLAN_RED || gs->phase == PHASE_PLAN_BLUE)) {
        plat_fill_rect(0, 0, gw, 18, 0x222244);
        snprintf(buf, sizeof(buf), "Click grid to place %s  (click button again to cancel)",
                 game_tower_name(gs->placement_intent));
        plat_draw_text(6, 2, buf, 0xFFEE88);
    }

    /* ── Sidebar ── */
    int sx = gw;
    plat_fill_rect(sx, 0, SIDEBAR_W, gh, 0x1C1C1C);
    plat_draw_line(sx, 0, sx, gh, 0x444444);
    int line = 12;

    snprintf(buf, sizeof(buf), "TURN %d", gs->turn);
    plat_draw_text(sx + 10, line, buf, 0xEEEEEE);
    line += 20;

    const char *phase_str = "—";
    uint32_t phase_col = 0xCCCCCC;
    if (gs->phase == PHASE_PLAN_RED)  { phase_str = "PLAN: RED";  phase_col = player_color(PLAYER_RED); }
    if (gs->phase == PHASE_PLAN_BLUE) { phase_str = "PLAN: BLUE"; phase_col = player_color(PLAYER_BLUE); }
    if (gs->phase == PHASE_SIMULATE)  { phase_str = "SIMULATING"; phase_col = 0xFFCC44; }
    if (gs->phase == PHASE_GAME_OVER) { phase_str = "GAME OVER";  phase_col = 0xFFFFFF; }
    plat_draw_text(sx + 10, line, phase_str, phase_col);
    line += 24;

    int is_plan = (gs->phase == PHASE_PLAN_RED || gs->phase == PHASE_PLAN_BLUE);
    if (is_plan) {
        draw_button(BTN_LOCK_IN, sx + 10, line, SIDEBAR_W - 20, 24, "Lock In", 0, 1);
        line += 32;
    }

    /* Player blocks */
    for (int p = 0; p < 2; p++) {
        int current = (gs->phase == PHASE_PLAN_RED && p == PLAYER_RED) ||
                      (gs->phase == PHASE_PLAN_BLUE && p == PLAYER_BLUE);
        if (current)
            plat_fill_rect(sx + 4, line - 5, SIDEBAR_W - 8, 22, 0x2A2A40);
        snprintf(buf, sizeof(buf), "%s  $%d  +%d",
                 p == PLAYER_RED ? "RED" : "BLUE",
                 gs->players[p].resources, gs->players[p].income_per_turn);
        plat_draw_text(sx + 10, line, buf, player_color((PlayerID)p));
        line += 22;
    }

    line += 4;

    if (is_plan) {
        PlayerID p = game_planning_player();

        plat_draw_text(sx + 10, line, "PLACE TOWER", 0xBBBBBB);
        line += 18;
        for (int i = 0; i < game_tower_count(); i++) {
            int cost    = game_tower_cost(i);
            int build_turns = game_tower_build_turns(i);
            int active  = (gs->placement_intent == i);
            int enabled = gs->players[p].resources >= cost;
            snprintf(buf, sizeof(buf), "[%c] %-8s $%d %dt",
                     game_tower_code(i), game_tower_name(i), cost, build_turns);
            draw_button(BTN_PLACE_TOWER_BASE + i, sx + 10, line,
                        SIDEBAR_W - 20, 22, buf, active, enabled);
            line += 26;
        }
        line += 4;

        plat_draw_text(sx + 10, line, "CREEP UPGRADES", 0xBBBBBB);
        line += 18;
        for (int i = 0; i < gs->players[p].creep_upgrade_count; i++) {
            const CreepUpgrade *u = &gs->players[p].creep_upgrades[i];
            const char *desc = game_creep_upgrade_description(i);
            if (u->completed) {
                snprintf(buf, sizeof(buf), "%s  READY", desc);
                plat_fill_rect(sx + 10, line, SIDEBAR_W - 20, 20, 0x183018);
                plat_draw_text(sx + 14, line + 3, buf, 0x66CC66);
            } else if (u->purchased) {
                snprintf(buf, sizeof(buf), "%s  %dt", desc, u->turns_remaining);
                plat_fill_rect(sx + 10, line, SIDEBAR_W - 20, 20, 0x252535);
                plat_draw_text(sx + 14, line + 3, buf, 0xAAAACC);
            } else {
                int cost  = game_creep_upgrade_cost(i);
                int turns = game_creep_upgrade_research_turns(i);
                int req   = game_creep_upgrade_requires(i);
                int locked = req >= 0 && !gs->players[p].creep_upgrades[req].completed;
                if (locked) {
                    snprintf(buf, sizeof(buf), "%s (locked)", desc);
                } else {
                    snprintf(buf, sizeof(buf), "%s $%d %dt", desc, cost, turns);
                }
                int enabled = !locked && gs->players[p].resources >= cost;
                draw_button(BTN_BUY_UPGRADE_BASE + i, sx + 10, line,
                            SIDEBAR_W - 20, 20, buf, 0, enabled);
            }
            line += 24;
        }
        line += 4;

        if (gs->selected_x >= 0 && gs->selected_y >= 0) {
            int id = gs->grid[gs->selected_x][gs->selected_y].thing_id;
            if (id >= 0 && gs->things[id].tag == THING_TOWER) {
                const Thing *t = &gs->things[id];
                snprintf(buf, sizeof(buf), "SEL %c%d @ %d,%d",
                         game_tower_code(t->tower.type), t->tower.level,
                         t->x, t->y);
                plat_draw_text(sx + 10, line, buf, 0xEEEEEE);
                line += 18;
                if (t->tower.build_turns > 0)
                    snprintf(buf, sizeof(buf), "  HP %d/%d  BUILDING %dt",
                             t->hp, t->max_hp, t->tower.build_turns);
                else
                    snprintf(buf, sizeof(buf), "  HP %d/%d  READY",
                             t->hp, t->max_hp);
                plat_draw_text(sx + 10, line, buf, 0x999999);
                line += 22;
                if (t->owner == p) {
                    int max_lvl = game_tower_max_level(t->tower.type);
                    int up_cost = game_tower_upgrade_cost(t->tower.type, t->tower.level);
                    int up_build_turns = game_tower_upgrade_turns(t->tower.type, t->tower.level);
                    char ulbl[32];
                    snprintf(ulbl, sizeof(ulbl), "Upg $%d %dt", up_cost, up_build_turns);
                    int bw = (SIDEBAR_W - 30) / 2;
                    int up_enabled = (t->tower.level < max_lvl) && (t->tower.build_turns == 0)
                                     && (gs->players[p].resources >= up_cost);
                    draw_button(BTN_UPGRADE_TOWER, sx + 10, line, bw, 22, ulbl, 0, up_enabled);
                    draw_button(BTN_DESTROY_TOWER, sx + 20 + bw, line, bw, 22, "Destroy", 0, 1);
                    line += 26;
                }
            }
        }
    } else if (gs->phase == PHASE_SIMULATE) {
        snprintf(buf, sizeof(buf), "Tick %d / %d", gs->sim_tick, SIM_TICKS_PER_TURN);
        plat_draw_text(sx + 10, line, buf, 0xCCCCCC);
    } else if (gs->phase == PHASE_GAME_OVER) {
        snprintf(buf, sizeof(buf), "%s WINS!",
                 gs->winner == PLAYER_RED ? "RED" : "BLUE");
        plat_draw_text(sx + 10, line, buf,
                       gs->winner == PLAYER_RED ? player_color(PLAYER_RED) : player_color(PLAYER_BLUE));
        line += 26;
        draw_button(BTN_RESTART, sx + 10, line, SIDEBAR_W - 20, 24, "Restart", 0, 1);
    }

    if (gs->status_ttl > 0 && gs->status_msg[0]) {
        plat_fill_rect(sx + 4, gh - 22, SIDEBAR_W - 8, 18, 0x402810);
        plat_draw_text(sx + 10, gh - 50, gs->status_msg, 0xFFCC66);
    }

    /* Frame stats overlay */
    FrameStats fs;
    plat_get_frame_stats(&fs);
    snprintf(buf, sizeof(buf), "FPS:%.0f  MaxLag:%.0fms", fs.fps, fs.max_lag_ms);
    plat_draw_text(gw + 10, gh - 16, buf, 0x888888);
}
