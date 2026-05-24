# Sidebar minimum height

Short maps used to clip the sidebar's button stack. The canvas was sized
to `grid_h * CELL_SIZE` and the sidebar shared that height, so a 5-row
map gave the sidebar only 160 px — far less than the planning stack
(turn header, Lock In, both player blocks, tower palette, creep
upgrade tiles, selected-tower controls) needs. Tower-palette and creep-
upgrade buttons drawn past the canvas edge were visually cut off.

Fix: introduce `SIDEBAR_MIN_H` in `src/render/render.h` (720 px, sized for
the worst-case PLAN layout with the shipped catalogs). The canvas height
in `platform_web.c::main` and the sidebar/status/frame-stats positions
in `render.c::render_frame` now use `max(grid_h * CELL_SIZE,
SIDEBAR_MIN_H)`. Grid lines and per-cell zone fills still draw at the
grid's true pixel height — only the right-side UI extends below.

Test: `test_short_map_sidebar_pads_to_min_height` in `tests/test_render.c`
renders with the 5-row `TEST_MAP_CORRIDOR_CFG` fixture, selects a tower,
and asserts (a) the sidebar background fill rect is `SIDEBAR_MIN_H` tall
(not 160) and (b) the Destroy button — the lowest in the selected-tower
stack — sits within that padded height. Observed failing on (a) before
the fix (sidebar_h == 160) and passing after.
