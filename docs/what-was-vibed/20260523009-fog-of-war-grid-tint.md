# Fog-of-war grid tint

Cells outside the viewer's current vision are now painted in a muted
`ZONE_*_BG_FOG_OF_WAR` shade; cells inside vision get the brighter
`ZONE_*_BG_LIVE` shade. Same LIVE / FOG_OF_WAR split the player entity
colors already use. Motivation: previously every cell shared the same
zone background, so a player looking at an enemy tower glyph couldn't
tell whether they were seeing it live or remembering it from frozen
fog memory.

## What changed

- **`src/render/palette.h`** — replaced the four `ZONE_*_BG` constants
  with `ZONE_*_BG_LIVE` / `ZONE_*_BG_FOG_OF_WAR` pairs. The previous
  values are kept verbatim as the FOG variants (subtle zone tints); the
  new LIVE variants are roughly 2× brighter so vision regions pop.
- **`src/render/render.c`** — `zone_color` now takes a `live` flag and
  picks the appropriate constant. The zone-paint loop in `render_frame`
  computes `live = viewer_committed && vis_now[x][y]` per cell.
  `viewer_committed` is false during `PHASE_PRE_SIM` (no viewer chosen
  yet) so we paint everything as fog there — avoids leaking either
  player's vision before the watch-from choice is made.

## Test

- `tests/test_render.c::test_fog_dims_grid_outside_vision` — places a
  GUNNER (vision=2) in RED's zone, renders in PLAN_RED, and asserts the
  tower cell and a cell 2 steps away report `ZONE_RED_BG_LIVE` while a
  far cell in the same zone reports `ZONE_RED_BG_FOG_OF_WAR`. Required
  extending the test's `plat_fill_rect` stub from a no-op to a capture
  buffer with a `fill_color_at(x, y, w, h)` exact-rect lookup; the
  zone-paint loop's per-cell fill is the only `plat_fill_rect` call at
  whole-cell coordinates, so the lookup disambiguates from overlays
  (HP bars, crowding badges).
- Observed failing: with `live` forced to 1, the `out_of_vision == FOG`
  assertion fails; restoring the real expression passes.
