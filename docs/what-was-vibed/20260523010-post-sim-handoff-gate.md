# Post-sim hand-off gate

## Problem

When Blue finished watching the sim, the game auto-transitioned straight
to PLAN_RED. Whoever was holding the device (typically Blue, who just
watched their sim) would see Red's planning view — towers, resources,
upgrade queue — for as long as it took them to copy the URL and forward
it to Red. Information leak.

## Reasoning that snapshot saves don't need to change

Snapshot pushes happen only on `BTN_LOCK_IN` and `BTN_RESTART` (platform_web.c).
The PRE_SIM snapshot Blue pushes at lock-in stays in the URL through
`game_choose_sim_view`, every sim tick, and the old `end_simulation` —
none of those push. So the URL Blue could copy mid-sim or post-sim is
already the PRE_SIM hand-off Red needs (Red opens it, lands in PRE_SIM,
picks their view, watches their sim independently). The leak was purely
in the on-screen rendering, not in URL state. Fix is a local gate; no
changes to snapshot timing.

## Fix

New phase `PHASE_POST_SIM` sits between SIMULATE and PLAN_RED. When the
sim's end-of-turn hold (`sim_end_hold`) decrements to zero, `game_frame`
parks in POST_SIM instead of calling `end_simulation()` directly. The new
public `game_continue_to_next_turn()` is what actually runs
`end_simulation()` — and the only thing that triggers it is the new
sidebar button `BTN_CONTINUE_TO_NEXT_TURN` ("Continue to Red's turn"),
dispatched in `platform_web.c::on_click` *without* `push_current_snapshot`
so the URL stays pinned to PRE_SIM.

Renderer treats POST_SIM like SIMULATE for fog/viewer purposes
(`g_viewer = gs->sim_viewer`), and adds a new sidebar branch that draws
only the Continue button — no planning controls, no PRE_SIM view-choice
buttons.

Snapshot encoder maps POST_SIM → 'V' as a defensive fallback. The path
where encode runs in POST_SIM doesn't exist today (lock-in and restart
are the only push sites and neither reaches POST_SIM), but the fallback
keeps the format valid if a future caller stumbles in.

## Tests

- `test_game.c::test_post_sim_gate` — sim parks in POST_SIM (not PLAN_RED);
  turn counter, resources, and creep-upgrade research timers are all held
  until `game_continue_to_next_turn()` runs; the continue commit reproduces
  every `end_simulation` side-effect.
- `test_render.c::test_post_sim_shows_continue_button` — POST_SIM sidebar
  presents `BTN_CONTINUE_TO_NEXT_TURN` and hides every other button
  category.
- Existing drain loops in `test_pathing.c` and `test_render.c` that waited
  for PLAN_RED to detect end-of-sim updated to dismiss POST_SIM via
  `game_continue_to_next_turn()`. The `run_sim_to_completion` helper in
  `test_game.c` got the same treatment.
