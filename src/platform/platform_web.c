#include <emscripten.h>
#include <emscripten/emscripten.h>
#include <string.h>
#include "../game/game.h"
#include "../render/render.h"
#include "platform.h"

static double last_time = 0.0;
static double accumulator = 0.0;

/* Snapshot I/O buffer. Sized generously: a full board with 100 towers and
 * all 8 upgrades fits comfortably; 4 KB leaves headroom for the URL prefix
 * the browser adds on top. JS writes the inbound snapshot into this buffer
 * (via stringToUTF8) and reads the outbound one with UTF8ToString — one
 * shared static page that game.c serializes into / out of. */
#define SNAPSHOT_BUF_SIZE 4096
static char g_snapshot_buf[SNAPSHOT_BUF_SIZE];

/* ── JS interop: canvas 2D drawing ── */

EM_JS(void, js_canvas_init, (int w, int h), {
    var canvas = document.getElementById("canvas");
    canvas.width = w;
    canvas.height = h;
    canvas.style.display = "block";
    /* Size the viewport to the canvas so the whole thing fits horizontally on
       any device by default; pinch-zoom remains available for zooming in/out. */
    var viewport = document.getElementById("viewport");
    if (viewport) viewport.setAttribute("content",
        "width=" + w + ", initial-scale=1, user-scalable=yes");
    window._ctx = canvas.getContext("2d");
    canvas.addEventListener("mousedown", function(e) {
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

/* ── Snapshot ↔ URL plumbing ── */

/* The browser URL is the source of truth for "which snapshot is loaded".
 * On every lock_in we encode the new state and pushState — so each click
 * adds a history entry and back/forward navigates between snapshots.
 * On initial load and on popstate we read #s=… from the URL and load it
 * (or fall back to a fresh game when the fragment is absent).
 *
 * The snapshot lives in the URL *fragment* (hash) rather than the query
 * string so that index.html is one cacheable resource: fragments are not
 * sent to the server and don't participate in the HTTP cache key. */

EM_JS(void, js_push_snapshot, (const char *snapshot), {
    var s = UTF8ToString(snapshot);
    var url = new URL(window.location);
    url.hash = "s=" + s;
    history.pushState(null, "", url);
});

EM_JS(void, js_replace_snapshot, (const char *snapshot), {
    var s = UTF8ToString(snapshot);
    var url = new URL(window.location);
    url.hash = "s=" + s;
    history.replaceState(null, "", url);
});

/* Reads #s=… into the wasm buffer. Returns 1 if a snapshot was present,
 * 0 if the URL fragment doesn't start with `s=`. */
EM_JS(int, js_read_url_snapshot, (char *buf, int buf_size), {
    var h = window.location.hash;
    if (h.charAt(0) === "#") h = h.substring(1);
    if (h.substring(0, 2) !== "s=") return 0;
    stringToUTF8(h.substring(2), buf, buf_size);
    return 1;
});

EM_JS(void, js_install_popstate, (), {
    window.addEventListener("popstate", function(e) {
        Module._on_popstate();
    });
});

static void push_current_snapshot(void) {
    int n = game_snapshot_encode(g_snapshot_buf, SNAPSHOT_BUF_SIZE);
    if (n > 0) js_push_snapshot(g_snapshot_buf);
}

static void replace_current_snapshot(void) {
    int n = game_snapshot_encode(g_snapshot_buf, SNAPSHOT_BUF_SIZE);
    if (n > 0) js_replace_snapshot(g_snapshot_buf);
}

EMSCRIPTEN_KEEPALIVE
void on_popstate(void) {
    if (js_read_url_snapshot(g_snapshot_buf, SNAPSHOT_BUF_SIZE)) {
        game_snapshot_load(g_snapshot_buf);
    } else {
        game_init();
    }
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
    if (btn >= BTN_PLACE_TOWER_BASE) {
        game_set_placement(btn - BTN_PLACE_TOWER_BASE);
        return;
    }
    if (btn >= BTN_BUY_UPGRADE_BASE) {
        game_buy_creep_upgrade(btn - BTN_BUY_UPGRADE_BASE);
        return;
    }
    switch (btn) {
        case BTN_LOCK_IN:            game_lock_in();                        push_current_snapshot(); break;
        case BTN_UPGRADE_TOWER:      game_upgrade_selected();                                        break;
        case BTN_DESTROY_TOWER:      game_destroy_selected();                                        break;
        case BTN_RESTART:            game_init();                           push_current_snapshot(); break;
        case BTN_START_SIM_AS_RED:   game_choose_sim_view(PLAYER_RED);      push_current_snapshot(); break;
        case BTN_START_SIM_AS_BLUE:  game_choose_sim_view(PLAYER_BLUE);     push_current_snapshot(); break;
        default: break;
    }
}

EMSCRIPTEN_KEEPALIVE
void on_right_click(void) {
    game_set_placement(-1); /* cancel placement intent */
}

/* ── Frame stats ── */

#define FRAME_RING_SIZE 360  /* ~6s at 60fps */
static double g_frame_deltas[FRAME_RING_SIZE];
static int    g_frame_ring_pos;
static int    g_frame_ring_count;
static double g_last_frame_time;  /* ms, from emscripten_get_now */
static float  g_fps;
static float  g_max_lag_ms;

void plat_get_frame_stats(FrameStats *out) {
    out->fps        = g_fps;
    out->max_lag_ms = g_max_lag_ms;
}

static void update_frame_stats(void) {
    double now = emscripten_get_now();
    if (g_last_frame_time > 0.0) {
        double dt = now - g_last_frame_time;
        g_frame_deltas[g_frame_ring_pos] = dt;
        g_frame_ring_pos = (g_frame_ring_pos + 1) % FRAME_RING_SIZE;
        if (g_frame_ring_count < FRAME_RING_SIZE) g_frame_ring_count++;

        /* FPS: average of last 60 frames */
        int n = g_frame_ring_count < 60 ? g_frame_ring_count : 60;
        double sum = 0;
        for (int i = 0; i < n; i++) {
            int idx = (g_frame_ring_pos - 1 - i + FRAME_RING_SIZE) % FRAME_RING_SIZE;
            sum += g_frame_deltas[idx];
        }
        g_fps = (sum > 0) ? (float)(n * 1000.0 / sum) : 0;

        /* Max lag: worst delta in entire ring buffer (up to 60s) */
        double worst = 0;
        for (int i = 0; i < g_frame_ring_count; i++) {
            if (g_frame_deltas[i] > worst) worst = g_frame_deltas[i];
        }
        g_max_lag_ms = (float)worst;
    }
    g_last_frame_time = now;
}

/* ── Main loop ── */

static void frame(void) {
    double current_time = emscripten_get_now(); // Returns ms
    double dt = current_time - last_time;
    last_time = current_time;
    if (dt > 250.0) {
        dt = 250.0;
    }
    accumulator += dt;

    update_frame_stats();

    while (accumulator >= MS_PER_LOGIC_FRAME) {
        game_frame(); // Runs exactly 60 times per real-time second
        accumulator -= MS_PER_LOGIC_FRAME;
    }

    render_frame(game_get_state());
}

int main(void) {
    game_init();
    /* If the URL we were opened with carries a snapshot, restore it now —
     * before the canvas is sized — so the rest of init sees the snapshot's
     * grid dimensions. (Today the map is fixed, but the snapshot may have
     * been authored against a different map.cfg; reading the URL first
     * keeps that future-safe.) */
    if (js_read_url_snapshot(g_snapshot_buf, SNAPSHOT_BUF_SIZE)) {
        game_snapshot_load(g_snapshot_buf);
    }
    const GameState *gs = game_get_state();
    int canvas_w = CELL_SIZE * gs->grid_w + SIDEBAR_W;
    int canvas_h = CELL_SIZE * gs->grid_h;
    last_time = emscripten_get_now();
    js_canvas_init(canvas_w, canvas_h);
    js_install_popstate();
    /* Anchor the URL to the current state so back/forward has somewhere
     * to land even before the user's first Lock In. */
    replace_current_snapshot();
    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
