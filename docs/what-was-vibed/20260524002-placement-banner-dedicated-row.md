# Placement banner gets a dedicated row above the grid

The "Click grid to place …" hint banner used to be drawn at canvas y=0
on top of the first grid row, covering whatever cell content lived there
(zone color, tower glyph, creep, etc.) while it was visible.

Reserved a fixed `BANNER_H = 20` px row at the top of the canvas for the
banner, and shifted all grid rendering down by that amount:

- `src/render/render.h` — added `BANNER_H`.
- `src/render/render.c` — every cell-y pixel calc now adds `BANNER_H`;
  banner fill widened from h=18 to h=BANNER_H and text nudged to y=3.
  `ui_h` is computed from `BANNER_H + grid_pixel_h` so a tall map's
  canvas grows to fit the new banner row.
- `src/platform/platform_web.c` — `canvas_h` includes `BANNER_H`;
  `on_click` subtracts `BANNER_H` from `py` before converting to cell
  coords, and clicks in the banner row are ignored.
- `tests/test_render.c` — fog/glyph/badge tests that asserted on exact
  grid pixel positions add `BANNER_H` to their y coordinates. New test
  `test_placement_banner_above_grid` asserts the banner fill spans
  `(0, 0, gw, BANNER_H)` and that no grid cell paints at y=0.
