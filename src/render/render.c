#include "render.h"
#include "palette.h"
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

static uint32_t zone_color(ZoneType z, int live) {
    switch (z) {
        case ZONE_RED:    return live ? ZONE_RED_BG_LIVE     : ZONE_RED_BG_FOG_OF_WAR;
        case ZONE_BLUE:   return live ? ZONE_BLUE_BG_LIVE    : ZONE_BLUE_BG_FOG_OF_WAR;
        case ZONE_DEBRIS: return live ? ZONE_DEBRIS_BG_LIVE  : ZONE_DEBRIS_BG_FOG_OF_WAR;
        default:          return live ? ZONE_NEUTRAL_BG_LIVE : ZONE_NEUTRAL_BG_FOG_OF_WAR;
    }
}

static uint32_t player_color(PlayerID p)     { return p == PLAYER_RED ? RED_LIVE        : BLUE_LIVE; }
static uint32_t player_color_dim(PlayerID p) { return p == PLAYER_RED ? RED_FOG_OF_WAR  : BLUE_FOG_OF_WAR; }

/* Viewer derived from current phase/state by render_frame. Used by every
 * entity-drawing loop to consult the right per-player PlayerView. */
static PlayerID g_viewer;

static int viewer_can_see(const GameState *gs, int x, int y) {
    return gs->views[g_viewer].vis_now[x][y] ? 1 : 0;
}

static void draw_button(int id, int x, int y, int w, int h,
                        const char *label, int active, int enabled) {
    uint32_t fill   = enabled ? (active ? BUTTON_ACTIVE_FILL : BUTTON_ENABLED_FILL) : BUTTON_DISABLED_FILL;
    uint32_t border = enabled ? BUTTON_ENABLED_BORDER : BUTTON_DISABLED_BORDER;
    uint32_t txt    = enabled ? BUTTON_ENABLED_TEXT   : BUTTON_DISABLED_TEXT;
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

    /* Viewer for fog-of-war filtering:
     *   PLAN_RED                 → RED
     *   PLAN_BLUE                → BLUE
     *   PRE_SIM / SIMULATE /
     *   POST_SIM / GAME_OVER     → gs->sim_viewer (chosen via the
     *                              "View RED sim" / "View BLUE sim"
     *                              buttons at PRE_SIM time) */
    if (gs->phase == PHASE_PLAN_BLUE) g_viewer = PLAYER_BLUE;
    else if (gs->phase == PHASE_PLAN_RED) g_viewer = PLAYER_RED;
    else g_viewer = (gs->sim_viewer == PLAYER_BLUE) ? PLAYER_BLUE : PLAYER_RED;

    /* PRE_SIM is the "choose whose view to watch the sim from" stage —
     * before a viewer is committed, rendering anything that belongs to a
     * specific player (towers, creeps, beams, flags, spawns, receptacles,
     * resources) would leak that player's state through whatever default
     * viewer is set. Hide all player-owned entities; only the empty map
     * (zones, grid lines) and the sidebar buttons remain. */
    int hide_players = (gs->phase == PHASE_PRE_SIM);

    plat_clear(CANVAS_BG);

    /* Grid zones — per game-design.md §6, terrain/zones/landmarks are "the
     * empty map" the player always sees, so we paint every cell. But cells
     * outside the viewer's current vision get the muted FOG_OF_WAR shade so
     * stale-vision regions are visually distinct from live ones. During
     * PRE_SIM no viewer is committed, so render uniformly fog (no leak). */
    int viewer_committed = (gs->phase != PHASE_PRE_SIM);
    for (int x = 0; x < gs->grid_w; x++)
        for (int y = 0; y < gs->grid_h; y++) {
            int live = viewer_committed && viewer_can_see(gs, x, y);
            plat_fill_rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE,
                           zone_color(gs->grid[x][y].zone, live));
        }

    /* Grid lines */
    for (int x = 0; x <= gs->grid_w; x++)
        plat_draw_line(x * CELL_SIZE, 0, x * CELL_SIZE, gh, GRID_LINE);
    for (int y = 0; y <= gs->grid_h; y++)
        plat_draw_line(0, y * CELL_SIZE, gw, y * CELL_SIZE, GRID_LINE);

    if (!hide_players) {
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

    /* Towers — single pass over the grid that honors fog. For each cell:
     *   - own tower, any state: drawn live.
     *   - enemy tower in vis_now:
     *       * mid-initial-build → "?" placeholder (type withheld).
     *       * mid-upgrade with prior known-level mem → render the prior
     *         memorised level (the upgrade is silent).
     *       * else → drawn live.
     *   - cell out of vis_now: consult mem[x][y]. Render the snapshot
     *     dimmed if it's a known tower, "?" dimmed if it was building.
     *   - no tower live and no memory: nothing. */
    for (int x = 0; x < gs->grid_w; x++) {
        for (int y = 0; y < gs->grid_h; y++) {
            int id = gs->grid[x][y].thing_id;
            int has_live = (id >= 0 && gs->things[id].tag == THING_TOWER && gs->things[id].alive);
            int visible = viewer_can_see(gs, x, y);
            const FogTowerMemory *m = &gs->views[g_viewer].mem[x][y];
            int px = x * CELL_SIZE, py = y * CELL_SIZE;

            if (has_live) {
                const Thing *t = &gs->things[id];
                int own = (t->owner == g_viewer);
                if (own || visible) {
                    int building = t->tower.build_turns > 0;
                    /* Enemy initial build: hide the type with a "?". */
                    if (!own && building && t->tower.level == 1) {
                        snprintf(buf, sizeof(buf), "?");
                        plat_draw_text(px + 12, py + 8, buf, player_color_dim(t->owner));
                        continue;
                    }
                    /* Enemy mid-upgrade with prior known level → show old. */
                    if (!own && building && t->tower.level > 1 && m->occ == FOG_OCC_TOWER) {
                        snprintf(buf, sizeof(buf), "%c%d", game_tower_code((TowerType)m->type), m->level);
                        plat_draw_text(px + 7, py + 8, buf, player_color_dim((PlayerID)m->owner));
                        continue;
                    }
                    /* Live (or own) draw. */
                    char code = game_tower_code(t->tower.type);
                    uint32_t col = building ? player_color_dim(t->owner) : player_color(t->owner);
                    if (building) snprintf(buf, sizeof(buf), "%c%d*", code, t->tower.level);
                    else          snprintf(buf, sizeof(buf), "%c%d",  code, t->tower.level);
                    plat_draw_text(px + 7, py + 8, buf, col);
                    if (t->hp < t->max_hp) {
                        int barw = (CELL_SIZE - 6) * t->hp / (t->max_hp > 0 ? t->max_hp : 1);
                        plat_fill_rect(px + 3, py + CELL_SIZE - 4, CELL_SIZE - 6, 2, HP_BAR_BG);
                        plat_fill_rect(px + 3, py + CELL_SIZE - 4, barw, 2, HP_BAR_FILL);
                    }
                    continue;
                }
                /* Enemy live tower the viewer can't see: fall through to
                 * draw whatever the memory remembers. */
            }
            if (visible) continue;   /* in vision, no tower live → nothing */
            if (m->occ == FOG_OCC_TOWER) {
                snprintf(buf, sizeof(buf), "%c%d", game_tower_code((TowerType)m->type), m->level);
                plat_draw_text(px + 7, py + 8, buf, player_color_dim((PlayerID)m->owner));
            } else if (m->occ == FOG_OCC_BUILDING) {
                snprintf(buf, sizeof(buf), "?");
                plat_draw_text(px + 12, py + 8, buf, player_color_dim((PlayerID)m->owner));
            }
        }
    }

    /* Tower attack beams: hide enemy beams whose source isn't visible. */
    for (int i = 0; i < gs->thing_count; i++) {
        const Thing *t = &gs->things[i];
        if (t->tag != THING_TOWER || !t->alive) continue;
        if (t->beam_ttl <= 0) continue;
        if (t->owner != g_viewer && !viewer_can_see(gs, t->x, t->y)) continue;
        int x1 = t->x * CELL_SIZE + CELL_SIZE/2;
        int y1 = t->y * CELL_SIZE + CELL_SIZE/2;
        int x2 = t->last_target_x * CELL_SIZE + CELL_SIZE/2;
        int y2 = t->last_target_y * CELL_SIZE + CELL_SIZE/2;
        plat_draw_line(x1, y1, x2, y2, TOWER_BEAM);
    }

    /* Creeps. Each creep is drawn as its catalog `code` letter — type-at-a-
     * glance instead of an undifferentiated dot. Heavy creeps (melee_damage > 0)
     * still get the dim color shade.
     *
     * Fog filter: own creeps always shown; enemy creeps only if their cell
     * is in vis_now. Creeps that the viewer once saw but are now out of
     * vision (or dead) are drawn from creep_mem[i] as dim letters — that's
     * the inspectable-corpse feature from §6. */
    static int creep_cnt[MAX_GRID_W][MAX_GRID_H][2][CREEP_TYPE_MAX_COUNT];
    memset(creep_cnt, 0, sizeof(creep_cnt));
    for (int i = 0; i < gs->thing_count; i++) {
        const Thing *t = &gs->things[i];
        if (t->tag != THING_CREEP || !t->alive) continue;
        int own = (t->owner == g_viewer);
        if (!own && !viewer_can_see(gs, t->x, t->y)) continue;
        creep_cnt[t->x][t->y][t->owner][t->creep.type]++;
        int heavy = game_creep_active_melee_damage(t->owner, t->creep.type) > 0;
        uint32_t col = heavy ? player_color_dim(t->owner) : player_color(t->owner);
        int cx = t->x * CELL_SIZE + CELL_SIZE/2;
        int cy = t->y * CELL_SIZE + CELL_SIZE/2;
        char glyph[2] = { game_creep_active_code(t->owner, t->creep.type), 0 };
        plat_draw_text(cx - 4, cy - 7, glyph, col);
        if (t->creep.slow_ticks > 0)
            plat_draw_circle(cx, cy, 9, CREEP_SLOW_HALO);
        if (t->creep.has_flag) {
            PlayerID flag_owner = (t->owner == PLAYER_RED) ? PLAYER_BLUE : PLAYER_RED;
            uint32_t fcol = player_color(flag_owner);
            plat_draw_line(cx - 9, cy - 12, cx - 9, cy + 2, fcol);
            plat_draw_triangle(cx - 9, cy - 12, cx + 1, cy - 9, cx - 9, cy - 6, fcol);
        }
    }

    /* Corpses: every creep_mem entry whose live thing isn't currently
     * drawn above gets a dim letter at its remembered cell. Covers both
     * "creep moved out of vision" mid-sim and "creep died" post-sim —
     * start_simulation wipes creep_mem so this only shows memories from
     * the current round. */
    for (int i = 0; i < MAX_THINGS; i++) {
        const FogCreepMemory *cm = &gs->views[g_viewer].creep_mem[i];
        if (!cm->valid) continue;
        const Thing *t = (i < gs->thing_count) ? &gs->things[i] : NULL;
        int live_visible = t && t->tag == THING_CREEP && t->alive &&
                           ((t->owner == g_viewer) ||
                            viewer_can_see(gs, t->x, t->y));
        if (live_visible) continue;
        int cx = cm->x * CELL_SIZE + CELL_SIZE/2;
        int cy = cm->y * CELL_SIZE + CELL_SIZE/2;
        char glyph[2] = { game_creep_active_code((PlayerID)cm->owner, (CreepType)cm->type), 0 };
        plat_draw_text(cx - 4, cy - 7, glyph, player_color_dim((PlayerID)cm->owner));
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
                plat_fill_rect(x * CELL_SIZE + 1, by, label_w, 12, CROWDING_BADGE_BG);
                plat_draw_text(x * CELL_SIZE + 2, by, buf, player_color((PlayerID)p));
                by += 13;
            }
        }
    }

    /* Selection highlight */
    if (gs->selected_x >= 0 && gs->selected_y >= 0) {
        plat_draw_rect(gs->selected_x * CELL_SIZE + 1, gs->selected_y * CELL_SIZE + 1,
                       CELL_SIZE - 2, CELL_SIZE - 2, SELECTION_HIGHLIGHT);
    }
    } /* end if (!hide_players) */

    /* Placement-intent hint banner */
    if (gs->placement_intent >= 0 && (gs->phase == PHASE_PLAN_RED || gs->phase == PHASE_PLAN_BLUE)) {
        plat_fill_rect(0, 0, gw, 18, PLACEMENT_BANNER_BG);
        snprintf(buf, sizeof(buf), "Click grid to place %s  (click button again to cancel)",
                 game_tower_name(gs->placement_intent));
        plat_draw_text(6, 2, buf, PLACEMENT_BANNER_TEXT);
    }

    /* ── Sidebar ── */
    int sx = gw;
    plat_fill_rect(sx, 0, SIDEBAR_W, gh, SIDEBAR_BG);
    plat_draw_line(sx, 0, sx, gh, SIDEBAR_BORDER);
    int line = 12;

    snprintf(buf, sizeof(buf), "TURN %d", gs->turn);
    plat_draw_text(sx + 10, line, buf, TEXT_PRIMARY);
    line += 20;

    const char *phase_str = "—";
    uint32_t phase_col = TEXT_SECONDARY;
    if (gs->phase == PHASE_PLAN_RED)  { phase_str = "PLAN: RED";  phase_col = player_color(PLAYER_RED); }
    if (gs->phase == PHASE_PLAN_BLUE) { phase_str = "PLAN: BLUE"; phase_col = player_color(PLAYER_BLUE); }
    if (gs->phase == PHASE_PRE_SIM)   { phase_str = "CHOOSE VIEW"; phase_col = PHASE_LABEL_HIGHLIGHT; }
    if (gs->phase == PHASE_SIMULATE)  { phase_str = "SIMULATING"; phase_col = PHASE_LABEL_HIGHLIGHT; }
    if (gs->phase == PHASE_POST_SIM)  { phase_str = "SIM ENDED";  phase_col = PHASE_LABEL_HIGHLIGHT; }
    if (gs->phase == PHASE_GAME_OVER) { phase_str = "GAME OVER";  phase_col = PHASE_LABEL_GAME_OVER; }
    plat_draw_text(sx + 10, line, phase_str, phase_col);
    line += 24;

    int is_plan = (gs->phase == PHASE_PLAN_RED || gs->phase == PHASE_PLAN_BLUE);
    if (is_plan) {
        draw_button(BTN_LOCK_IN, sx + 10, line, SIDEBAR_W - 20, 24, "Lock In", 0, 1);
        line += 32;
    }

    /* Player blocks. Enemy resources/income are masked under fog — the
     * viewer doesn't get their opponent's exact economy. Skipped during
     * PRE_SIM: no viewer is committed yet, so a default viewer would leak
     * one side's real numbers. */
    if (!hide_players) {
        for (int p = 0; p < 2; p++) {
            int current = (gs->phase == PHASE_PLAN_RED && p == PLAYER_RED) ||
                          (gs->phase == PHASE_PLAN_BLUE && p == PLAYER_BLUE);
            if (current)
                plat_fill_rect(sx + 4, line - 5, SIDEBAR_W - 8, 22, CURRENT_PLAYER_HIGHLIGHT);
            if ((PlayerID)p == g_viewer) {
                snprintf(buf, sizeof(buf), "%s  $%d  +%d",
                         p == PLAYER_RED ? "RED" : "BLUE",
                         gs->players[p].resources, gs->players[p].income_per_turn);
            } else {
                snprintf(buf, sizeof(buf), "%s  $???  +??",
                         p == PLAYER_RED ? "RED" : "BLUE");
            }
            plat_draw_text(sx + 10, line, buf, player_color((PlayerID)p));
            line += 22;
        }
        line += 4;
    }

    if (is_plan) {
        PlayerID p = game_planning_player();

        plat_draw_text(sx + 10, line, "PLACE TOWER", TEXT_SECTION_HEADER);
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

        plat_draw_text(sx + 10, line, "CREEP UPGRADES", TEXT_SECTION_HEADER);
        line += 18;
        for (int i = 0; i < gs->players[p].creep_upgrade_count; i++) {
            const CreepUpgrade *u = &gs->players[p].creep_upgrades[i];
            const char *desc = game_creep_upgrade_description(i);
            if (u->completed) {
                snprintf(buf, sizeof(buf), "%s  READY", desc);
                plat_fill_rect(sx + 10, line, SIDEBAR_W - 20, 20, UPGRADE_READY_BG);
                plat_draw_text(sx + 14, line + 3, buf, UPGRADE_READY_TEXT);
            } else if (u->purchased) {
                snprintf(buf, sizeof(buf), "%s  %dt", desc, u->turns_remaining);
                plat_fill_rect(sx + 10, line, SIDEBAR_W - 20, 20, UPGRADE_PURCHASED_BG);
                plat_draw_text(sx + 14, line + 3, buf, UPGRADE_PURCHASED_TEXT);
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
                plat_draw_text(sx + 10, line, buf, TEXT_PRIMARY);
                line += 18;
                if (t->tower.build_turns > 0)
                    snprintf(buf, sizeof(buf), "  HP %d/%d  BUILDING %dt",
                             t->hp, t->max_hp, t->tower.build_turns);
                else
                    snprintf(buf, sizeof(buf), "  HP %d/%d  READY",
                             t->hp, t->max_hp);
                plat_draw_text(sx + 10, line, buf, TEXT_MUTED);
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
    } else if (gs->phase == PHASE_PRE_SIM) {
        /* Two committal buttons — clicking either sets sim_viewer and
         * kicks off the actual simulation. */
        plat_draw_text(sx + 10, line, "Watch the sim as:", TEXT_SECONDARY);
        line += 20;
        draw_button(BTN_START_SIM_AS_RED,  sx + 10, line, SIDEBAR_W - 20, 24,
                    "View RED sim", 0, 1);
        line += 28;
        draw_button(BTN_START_SIM_AS_BLUE, sx + 10, line, SIDEBAR_W - 20, 24,
                    "View BLUE sim", 0, 1);
    } else if (gs->phase == PHASE_SIMULATE) {
        snprintf(buf, sizeof(buf), "Tick %d / %d", gs->sim_tick, SIM_TICKS_PER_TURN);
        plat_draw_text(sx + 10, line, buf, TEXT_SECONDARY);
        line += 20;
        snprintf(buf, sizeof(buf), "View: %s", g_viewer == PLAYER_RED ? "RED" : "BLUE");
        plat_draw_text(sx + 10, line, buf, player_color(g_viewer));
    } else if (gs->phase == PHASE_POST_SIM) {
        /* Hand-off gate. The URL still holds the PRE_SIM snapshot from
         * Blue's lock-in — Blue copies it to Red here, *before* the
         * Continue click reveals Red's planning view on this device. */
        snprintf(buf, sizeof(buf), "View: %s", g_viewer == PLAYER_RED ? "RED" : "BLUE");
        plat_draw_text(sx + 10, line, buf, player_color(g_viewer));
        line += 22;
        draw_button(BTN_CONTINUE_TO_NEXT_TURN, sx + 10, line, SIDEBAR_W - 20, 24,
                    "Continue to Red's turn", 0, 1);
    } else if (gs->phase == PHASE_GAME_OVER) {
        snprintf(buf, sizeof(buf), "%s WINS!",
                 gs->winner == PLAYER_RED ? "RED" : "BLUE");
        plat_draw_text(sx + 10, line, buf,
                       gs->winner == PLAYER_RED ? player_color(PLAYER_RED) : player_color(PLAYER_BLUE));
        line += 26;
        draw_button(BTN_RESTART, sx + 10, line, SIDEBAR_W - 20, 24, "Restart", 0, 1);
    }

    if (gs->status_ttl > 0 && gs->status_msg[0]) {
        plat_fill_rect(sx + 4, gh - 22, SIDEBAR_W - 8, 18, STATUS_MSG_BG);
        plat_draw_text(sx + 10, gh - 50, gs->status_msg, STATUS_MSG_TEXT);
    }

    /* Frame stats overlay */
    FrameStats fs;
    plat_get_frame_stats(&fs);
    snprintf(buf, sizeof(buf), "FPS:%.0f  MaxLag:%.0fms", fs.fps, fs.max_lag_ms);
    plat_draw_text(gw + 10, gh - 16, buf, TEXT_FAINT);
}
