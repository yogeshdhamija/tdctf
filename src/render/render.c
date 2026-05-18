#include "render.h"
#include "../platform/platform.h"
#include <stdio.h>

#define CELL_SIZE 32
#define SIDEBAR_W 220

static uint32_t zone_color(ZoneType zone) {
    switch (zone) {
        case ZONE_RED:     return 0x1A0000;
        case ZONE_BLUE:    return 0x00001A;
        case ZONE_NEUTRAL: return 0x1A1A1A;
    }
    return 0x1A1A1A;
}

void render_frame(const GameState *state) {
    int gw = state->grid_w * CELL_SIZE;
    int gh = state->grid_h * CELL_SIZE;

    plat_clear(0x111111);

    /* Grid cells */
    for (int x = 0; x < state->grid_w; x++) {
        for (int y = 0; y < state->grid_h; y++) {
            plat_fill_rect(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE,
                           zone_color(state->grid[x][y].zone));
        }
    }

    /* Grid lines */
    for (int x = 0; x <= state->grid_w; x++)
        plat_draw_line(x * CELL_SIZE, 0, x * CELL_SIZE, gh, 0x444444);
    for (int y = 0; y <= state->grid_h; y++)
        plat_draw_line(0, y * CELL_SIZE, gw, y * CELL_SIZE, 0x444444);

    /* Flags */
    for (int i = 0; i < 2; i++) {
        uint32_t col = (state->flags[i].owner == PLAYER_RED) ? 0xCC3333 : 0x3333CC;
        int fx = state->flags[i].x * CELL_SIZE + CELL_SIZE / 2;
        int fy = state->flags[i].y * CELL_SIZE + 4;
        plat_draw_triangle(fx - 8, fy, fx + 8, fy + 10, fx - 8, fy + 20, col);
    }

    /* Sidebar background */
    plat_fill_rect(gw, 0, SIDEBAR_W, gh, 0x222222);

    /* Sidebar text */
    char buf[64];
    int sy = 10;
    snprintf(buf, sizeof(buf), "TURN %d", state->turn);
    plat_draw_text(gw + 10, sy, buf, 0xCCCCCC);
    sy += 24;

    const char *phase_str = "PLANNING (RED)";
    if (state->phase == PHASE_PLAN_BLUE)  phase_str = "PLANNING (BLUE)";
    if (state->phase == PHASE_SIMULATE)   phase_str = "SIMULATION";
    if (state->phase == PHASE_GAME_OVER)  phase_str = "GAME OVER";
    plat_draw_text(gw + 10, sy, phase_str, 0xCCCCCC);
    sy += 36;

    for (int p = 0; p < 2; p++) {
        uint32_t col = (p == PLAYER_RED) ? 0xCC6666 : 0x6666CC;
        snprintf(buf, sizeof(buf), "Player %s", p == PLAYER_RED ? "RED" : "BLUE");
        plat_draw_text(gw + 10, sy, buf, col);
        sy += 20;
        snprintf(buf, sizeof(buf), "  Resources: %d", state->players[p].resources);
        plat_draw_text(gw + 10, sy, buf, 0xAAAAAA);
        sy += 20;
        snprintf(buf, sizeof(buf), "  Income: +%d/turn", state->players[p].income_per_turn);
        plat_draw_text(gw + 10, sy, buf, 0xAAAAAA);
        sy += 30;
    }
}
