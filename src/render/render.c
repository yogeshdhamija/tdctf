#include "render.h"
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

    /* Spawn-direction arrows: at each spawn cell, an arrow pointing the
     * first BFS step toward the enemy flag — i.e. where this player's
     * creeps will head when they appear. Updates live as towers reshape
     * the shortest path. */
    for (int p = 0; p < 2; p++) {
        int sxc = gs->spawn_x[p], syc = gs->spawn_y[p];
        int nx, ny;
        if (!game_pathing_next_step(gs, sxc, syc, (PlayerID)p, 0, &nx, &ny)) continue;
        int dx = nx - sxc, dy = ny - syc;
        if (dx == 0 && dy == 0) continue;
        uint32_t col = player_color((PlayerID)p);
        int cx = sxc * CELL_SIZE + CELL_SIZE / 2;
        int cy = syc * CELL_SIZE + CELL_SIZE / 2;
        int tail_x = cx - dx * 11, tail_y = cy - dy * 11;
        int tip_x  = cx + dx * 11, tip_y  = cy + dy * 11;
        int base_x = cx + dx * 2,  base_y = cy + dy * 2;
        int perp_x = -dy * 5,      perp_y =  dx * 5;
        plat_draw_line(tail_x, tail_y, base_x, base_y, col);
        plat_draw_triangle(tip_x, tip_y,
                           base_x + perp_x, base_y + perp_y,
                           base_x - perp_x, base_y - perp_y, col);
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

    /* Creeps */
    static int creep_cnt[MAX_GRID_W][MAX_GRID_H][2][CREEP_TYPE_COUNT];
    memset(creep_cnt, 0, sizeof(creep_cnt));
    for (int i = 0; i < gs->thing_count; i++) {
        const Thing *t = &gs->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        creep_cnt[t->x][t->y][t->owner][t->creep.type]++;
        uint32_t col = (t->creep.type == CREEP_SIEGE)
                       ? player_color_dim(t->owner) : player_color(t->owner);
        int cx = t->x * CELL_SIZE + CELL_SIZE/2;
        int cy = t->y * CELL_SIZE + CELL_SIZE/2;
        int r  = (t->creep.type == CREEP_SIEGE) ? 7 : 5;
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
     * or the type mix. Draw a compact per-owner label like "2R", "3S", or
     * "2R1S" anchored to the top of the cell in the owner's color, with a
     * dark backdrop so it stays readable over the creep circle. */
    for (int x = 0; x < gs->grid_w; x++) {
        for (int y = 0; y < gs->grid_h; y++) {
            int total = creep_cnt[x][y][0][CREEP_RETRIEVER] + creep_cnt[x][y][0][CREEP_SIEGE]
                      + creep_cnt[x][y][1][CREEP_RETRIEVER] + creep_cnt[x][y][1][CREEP_SIEGE];
            if (total < 2) continue;
            int by = y * CELL_SIZE + 1;
            for (int p = 0; p < 2; p++) {
                int rc = creep_cnt[x][y][p][CREEP_RETRIEVER];
                int sc = creep_cnt[x][y][p][CREEP_SIEGE];
                if (rc + sc == 0) continue;
                if      (rc > 0 && sc > 0) snprintf(buf, sizeof(buf), "%dR%dS", rc, sc);
                else if (rc > 0)           snprintf(buf, sizeof(buf), "%dR", rc);
                else                       snprintf(buf, sizeof(buf), "%dS", sc);
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
                 game_tower_name((TowerType)gs->placement_intent));
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
            plat_fill_rect(sx + 4, line - 3, SIDEBAR_W - 8, 44, 0x2A2A40);
        snprintf(buf, sizeof(buf), "%s  $%d  +%d",
                 p == PLAYER_RED ? "RED" : "BLUE",
                 gs->players[p].resources, gs->players[p].income_per_turn);
        plat_draw_text(sx + 10, line, buf, player_color((PlayerID)p));
        line += 18;
        int rcount = 0, scount = 0;
        for (int i = 0; i < gs->players[p].creep_upgrade_count; i++) {
            const CreepUpgrade *u = &gs->players[p].creep_upgrades[i];
            if (!u->completed) continue;
            rcount += u->add_retrievers;
            scount += u->add_siege;
        }
        snprintf(buf, sizeof(buf), "  spawn %dR %dS", rcount, scount);
        plat_draw_text(sx + 10, line, buf, 0x888888);
        line += 22;
    }

    line += 4;

    if (is_plan) {
        PlayerID p = game_planning_player();

        plat_draw_text(sx + 10, line, "PLACE TOWER", 0xBBBBBB);
        line += 18;
        for (int i = 0; i < TOWER_TYPE_COUNT; i++) {
            snprintf(buf, sizeof(buf), "[%c] %-8s $%d",
                     game_tower_code((TowerType)i),
                     game_tower_name((TowerType)i),
                     game_tower_cost((TowerType)i));
            int active  = (gs->placement_intent == i);
            int enabled = gs->players[p].resources >= game_tower_cost((TowerType)i);
            draw_button(BTN_PLACE_BLOCKER + i, sx + 10, line,
                        SIDEBAR_W - 20, 22, buf, active, enabled);
            line += 26;
        }
        line += 4;

        plat_draw_text(sx + 10, line, "CREEP UPGRADES", 0xBBBBBB);
        line += 18;
        for (int i = 0; i < gs->players[p].creep_upgrade_count; i++) {
            const CreepUpgrade *u = &gs->players[p].creep_upgrades[i];
            if (u->completed) {
                snprintf(buf, sizeof(buf), "%s  READY", u->description);
                plat_fill_rect(sx + 10, line, SIDEBAR_W - 20, 20, 0x183018);
                plat_draw_text(sx + 14, line + 3, buf, 0x66CC66);
            } else if (u->purchased) {
                snprintf(buf, sizeof(buf), "%s  %dt", u->description, u->turns_remaining);
                plat_fill_rect(sx + 10, line, SIDEBAR_W - 20, 20, 0x252535);
                plat_draw_text(sx + 14, line + 3, buf, 0xAAAACC);
            } else {
                snprintf(buf, sizeof(buf), "%s $%d", u->description, u->cost);
                int enabled = gs->players[p].resources >= u->cost;
                draw_button(BTN_BUY_UPGRADE_0 + i, sx + 10, line,
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
                snprintf(buf, sizeof(buf), "  HP %d/%d  %s",
                         t->hp, t->max_hp,
                         t->tower.build_turns > 0 ? "BUILDING" : "READY");
                plat_draw_text(sx + 10, line, buf, 0x999999);
                line += 22;
                if (t->owner == p) {
                    int up_cost = game_tower_cost(t->tower.type);
                    char ulbl[32];
                    snprintf(ulbl, sizeof(ulbl), "Upg $%d", up_cost);
                    int bw = (SIDEBAR_W - 30) / 2;
                    int up_enabled = (t->tower.level < 2) && (t->tower.build_turns == 0)
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
        plat_draw_text(sx + 10, gh - 20, gs->status_msg, 0xFFCC66);
    }

    /* Frame stats overlay (bottom-left of grid) */
    FrameStats fs;
    plat_get_frame_stats(&fs);
    snprintf(buf, sizeof(buf), "FPS:%.0f  MaxLag:%.0fms", fs.fps, fs.max_lag_ms);
    plat_draw_text(4, gh - 16, buf, 0x888888);
}
