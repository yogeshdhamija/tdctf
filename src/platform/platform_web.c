#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <string.h>
#include "../game/game.h"
#include "../render/render.h"
#include "platform.h"

/* ── JS interop: canvas 2D drawing ── */

EM_JS(void, js_canvas_init, (int w, int h), {
    var canvas = document.getElementById("canvas");
    canvas.width = w;
    canvas.height = h;
    canvas.style.display = "block";
    window._ctx = canvas.getContext("2d");
    canvas.addEventListener("click", function(e) {
        var rect = canvas.getBoundingClientRect();
        var sx = canvas.width  / rect.width;
        var sy = canvas.height / rect.height;
        var x = ((e.clientX - rect.left) * sx) | 0;
        var y = ((e.clientY - rect.top)  * sy) | 0;
        Module._on_click(x, y);
    });
    canvas.addEventListener("contextmenu", function(e) {
        e.preventDefault();
        Module._on_right_click();
    });
});

EM_JS(void, js_clear, (uint32_t color), {
    var ctx = window._ctx;
    ctx.fillStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
});

EM_JS(void, js_fill_rect, (int x, int y, int w, int h, uint32_t color), {
    var ctx = window._ctx;
    ctx.fillStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.fillRect(x, y, w, h);
});

EM_JS(void, js_draw_rect, (int x, int y, int w, int h, uint32_t color), {
    var ctx = window._ctx;
    ctx.strokeStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.lineWidth = 1;
    ctx.strokeRect(x + 0.5, y + 0.5, w, h);
});

EM_JS(void, js_fill_circle, (int cx, int cy, int r, uint32_t color), {
    var ctx = window._ctx;
    ctx.fillStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
    ctx.fill();
});

EM_JS(void, js_draw_circle, (int cx, int cy, int r, uint32_t color), {
    var ctx = window._ctx;
    ctx.strokeStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
    ctx.stroke();
});

EM_JS(void, js_draw_line, (int x1, int y1, int x2, int y2, uint32_t color), {
    var ctx = window._ctx;
    ctx.strokeStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(x1 + 0.5, y1 + 0.5);
    ctx.lineTo(x2 + 0.5, y2 + 0.5);
    ctx.stroke();
});

EM_JS(void, js_draw_text, (int x, int y, const char *text, uint32_t color), {
    var ctx = window._ctx;
    ctx.fillStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.font = "13px monospace";
    ctx.textBaseline = "top";
    ctx.fillText(UTF8ToString(text), x, y);
});

EM_JS(void, js_draw_triangle, (int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color), {
    var ctx = window._ctx;
    ctx.fillStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.lineTo(x3, y3);
    ctx.closePath();
    ctx.fill();
});

/* ── Platform API ── */

void plat_clear(uint32_t color)                                                   { js_clear(color); }
void plat_fill_rect(int x, int y, int w, int h, uint32_t color)                   { js_fill_rect(x, y, w, h, color); }
void plat_draw_rect(int x, int y, int w, int h, uint32_t color)                   { js_draw_rect(x, y, w, h, color); }
void plat_fill_circle(int cx, int cy, int r, uint32_t color)                      { js_fill_circle(cx, cy, r, color); }
void plat_draw_circle(int cx, int cy, int r, uint32_t color)                      { js_draw_circle(cx, cy, r, color); }
void plat_draw_line(int x1, int y1, int x2, int y2, uint32_t color)               { js_draw_line(x1, y1, x2, y2, color); }
void plat_draw_text(int x, int y, const char *text, uint32_t color)               { js_draw_text(x, y, text, color); }
void plat_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    js_draw_triangle(x1, y1, x2, y2, x3, y3, color);
}

/* ── Input ── */

EMSCRIPTEN_KEEPALIVE
void on_click(int px, int py) {
    const GameState *gs = game_get_state();
    int gw = CELL_SIZE * gs->grid_w;
    if (px < gw) {
        if (gs->phase == PHASE_PLAN_RED || gs->phase == PHASE_PLAN_BLUE)
            game_grid_click(px / CELL_SIZE, py / CELL_SIZE);
        return;
    }
    int btn = render_button_at(px, py);
    switch (btn) {
        case BTN_LOCK_IN:         game_lock_in();                       break;
        case BTN_PLACE_GUNNER:    game_set_placement(TOWER_GUNNER);     break;
        case BTN_PLACE_SLAMMER:   game_set_placement(TOWER_SLAMMER);    break;
        case BTN_PLACE_RESOURCE:  game_set_placement(TOWER_RESOURCE);   break;
        case BTN_UPGRADE_TOWER:   game_upgrade_selected();              break;
        case BTN_DESTROY_TOWER:   game_destroy_selected();              break;
        case BTN_BUY_UPGRADE_0:   game_buy_creep_upgrade(0);            break;
        case BTN_BUY_UPGRADE_1:   game_buy_creep_upgrade(1);            break;
        case BTN_BUY_UPGRADE_2:   game_buy_creep_upgrade(2);            break;
        case BTN_BUY_UPGRADE_3:   game_buy_creep_upgrade(3);            break;
        case BTN_RESTART:         game_init();                          break;
        default: break;
    }
}

EMSCRIPTEN_KEEPALIVE
void on_right_click(void) {
    game_set_placement(-1); /* cancel placement intent */
}

/* ── Main loop ── */

static void frame(void) {
    game_frame();
    render_frame(game_get_state());
}

int main(void) {
    game_init();
    const GameState *gs = game_get_state();
    int canvas_w = CELL_SIZE * gs->grid_w + SIDEBAR_W;
    int canvas_h = CELL_SIZE * gs->grid_h;
    js_canvas_init(canvas_w, canvas_h);
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
