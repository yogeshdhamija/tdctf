# PRE_SIM hides all player-owned state

Bug: during `PHASE_PRE_SIM` ("CHOOSE VIEW") the grid and sidebar leaked
one player's towers, creeps, landmarks, and economy. Cause: the render
layer derives `g_viewer` from `gs->sim_viewer`, which defaults to
`PLAYER_RED` until the user clicks one of the view-choice buttons. So
between BLUE locking in and the user picking a view, the canvas was
rendering RED's full fog view — RED's towers and economy visible, BLUE's
masked. Either default would leak one player's state.

Fix: in `src/render/render.c::render_frame`, set
`int hide_players = (gs->phase == PHASE_PRE_SIM);` and wrap the
entity-rendering sections (receptacles, spawn markers, flags, towers,
beams, creeps, corpses, crowding badges, selection highlight) in
`if (!hide_players) { ... }`. Also wrap the sidebar's player resource
loop with the same guard. Grid zones, grid lines, the TURN/phase label,
and the two PRE_SIM buttons (`View RED sim` / `View BLUE sim`) stay
visible.

## Tests

- `tests/test_render.c::test_pre_sim_hides_all_player_state` — both
  players place a Gunner, both lock in to land on PRE_SIM, then assert
  no `"G1"` glyph appears anywhere on the canvas and no `$` character
  appears in any sidebar text. Also re-checks that
  `BTN_START_SIM_AS_RED` / `BTN_START_SIM_AS_BLUE` remain hittable.

Verified the test fails without the render.c change (3 failures: RED's
own `G1` glyph rendered, and both `$100` and `$???` sidebar lines
present).
