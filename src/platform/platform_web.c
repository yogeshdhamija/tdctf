#include <emscripten.h>
#include <string.h>
#include "../game/game.h"
#include "platform.h"

void render_frame(const GameState *state);

/* ── JS interop: canvas 2D drawing ── */

EM_JS(void, js_canvas_init, (int w, int h), {
    var canvas = document.getElementById("canvas");
    canvas.width = w;
    canvas.height = h;
    window._ctx = canvas.getContext("2d");
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
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
    ctx.stroke();
});

EM_JS(void, js_draw_line, (int x1, int y1, int x2, int y2, uint32_t color), {
    var ctx = window._ctx;
    ctx.strokeStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.beginPath();
    ctx.moveTo(x1 + 0.5, y1 + 0.5);
    ctx.lineTo(x2 + 0.5, y2 + 0.5);
    ctx.stroke();
});

EM_JS(void, js_draw_text, (int x, int y, const char *text, uint32_t color), {
    var ctx = window._ctx;
    ctx.fillStyle = "#" + ("000000" + (color & 0xFFFFFF).toString(16)).slice(-6);
    ctx.font = "14px monospace";
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

/* ── Platform API implementation ── */

void plat_clear(uint32_t color)                                                    { js_clear(color); }
void plat_fill_rect(int x, int y, int w, int h, uint32_t color)                   { js_fill_rect(x, y, w, h, color); }
void plat_draw_rect(int x, int y, int w, int h, uint32_t color)                   { js_draw_rect(x, y, w, h, color); }
void plat_fill_circle(int cx, int cy, int r, uint32_t color)                      { js_fill_circle(cx, cy, r, color); }
void plat_draw_circle(int cx, int cy, int r, uint32_t color)                      { js_draw_circle(cx, cy, r, color); }
void plat_draw_line(int x1, int y1, int x2, int y2, uint32_t color)              { js_draw_line(x1, y1, x2, y2, color); }
void plat_draw_text(int x, int y, const char *text, uint32_t color)              { js_draw_text(x, y, text, color); }
void plat_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) { js_draw_triangle(x1, y1, x2, y2, x3, y3, color); }

/* ── Main loop ── */

#define CELL_SIZE 32
#define SIDEBAR_W 220

static void frame(void) {
    const GameState *gs = game_get_state();
    render_frame(gs);
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
