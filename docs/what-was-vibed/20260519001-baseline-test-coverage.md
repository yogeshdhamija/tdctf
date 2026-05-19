# Baseline test coverage

**Date:** 2026-05-19
**Files touched:** `tests/test_pathing.c` (updated), `tests/test_game.c` (new),
`tests/test_render.c` (new), `Makefile`.

## Why

`coding-norms.md` requires consumer-driven testing for every change. The
existing harness covered only creep pathing — the rest of the game layer
(phase machine, placement rules, upgrades, combat, flag mechanics, win
condition) and the render layer's click-hit testing shipped without
tests. This adds them. `tests/test_game.c` and `tests/test_render.c` are
intended to grow alongside future features in their respective layers;
they're catch-alls for behaviors that aren't focused enough to warrant
their own file (the way `test_pathing.c` is).

## Changes

### `tests/test_pathing.c` — fixed

Base retriever spawn count is currently 0, so the pre-existing pathing
tests had silently broken (every `first_creep(...)` returned NULL). Added
a `setup_turn2_with_red_retriever()` helper that buys the +1 retriever
upgrade in turn 1, drains the quiet turn-1 sim, and enters turn 2 where
the upgrade is complete and a retriever spawns. The obstacle tests also
switch from `TOWER_RESOURCE` (cost 80) to `TOWER_BLOCKER` (cost 20) so
that the upgrade + obstacle both fit in RED's starting 100 resources.

### `tests/test_game.c` — new

18 tests, 115 assertions, all going through `game_*` public API and
`game_get_state()`. No internals (BFS, walkable, placement_valid, etc.)
are referenced directly — they're covered transitively. Tests grouped by
concern:

- **Init / phases** — `test_initial_state`, `test_phase_transitions`.
- **Placement** — basics (cost, intent clearing, selected cursor),
  zone restrictions (RED/BLUE/NEUTRAL), insufficient resources, occupied
  cell.
- **Tower upgrade / destroy** — happy path + rejects-enemy-tower.
- **Creep upgrades** — purchase deducts cost, research countdown ticks
  down each sim turn, completion enables spawns.
- **Resource tower income** — base 20 → 30 on the turn build completes
  (the decrement-then-recompute ordering in `end_simulation` is the
  subtlety the test pins down).
- **Combat** — gunner damages a creep in range; slammer applies
  `slow_ticks` and damage (build_turns=2 forces a 2-turn warmup before
  this test can spawn a creep into range); siege creep adjacent to
  enemy tower deals melee damage.
- **Flag** — pickup sets `has_flag` / `carried_by` / clears `at_home`;
  carrier death produces a dropped state (`!at_home && carried_by==-1`)
  off the at-home cell.
- **Win** — BLUE retriever returns to BLUE receptacle with flag →
  `winner=BLUE`, `phase=PHASE_GAME_OVER`.

### `tests/test_render.c` — new

7 tests covering `render_button_at` — the click → `ButtonID` hit-test
that wires sidebar clicks to game actions. Approach:

1. Stub `plat_*` draw primitives at file scope (we don't care about
   pixels, only about which rectangles end up in the button hit table).
2. Build a `GameState` via `game_init()` + `game_*` actions.
3. Call `render_frame(state)`.
4. Sweep the canvas at stride 4 calling `render_button_at`; record the
   set of `ButtonID`s observed and assert against expectations.

Covers: no buttons in the grid area; Lock In + Place + Buy Upgrade
visible during planning; Buy Upgrade button disappears after purchase;
Upgrade/Destroy visible iff the selected tower is owned by the current
planner; SIMULATE phase has no buttons; Restart visible iff GAME_OVER.

### `Makefile`

Split the single `test` target into three binaries (`build/test_pathing`,
`build/test_game`, `build/test_render`) and made `make test` build and
run all three.

## Choices worth flagging

- **Three binaries, not one.** Each test file is its own binary so the
  files stay focused; they all build the same `game.c` (and `render.c`
  for the render binary) translation units and are essentially free.
- **`test_game.c` and `test_render.c` are catch-alls.** Future
  features in either layer can add to the existing file or, if the area
  grows large (the way pathing did), split off into its own
  `test_<concern>.c` and a matching Makefile target.
- **Platform layer left untested.** `platform_web.c` is mostly
  Emscripten/JS glue (`emscripten_set_main_loop`, `EM_ASM`, canvas
  bridging) which doesn't run under `cc`. The pure-C bits inside it
  (pixel→grid translation, the FPS/lag ring buffer) could be extracted
  into a testable helper if we ever care — left as a future split.
- **Render tests sweep instead of hard-coding pixel coords.** Asserting
  "click at (970, 56) returns BTN_LOCK_IN" locks in current layout math
  and produces brittle failures whenever the sidebar moves. The sweep
  approach asserts "BTN_LOCK_IN is hittable *somewhere*", which tests
  the wiring without coupling to layout.
- **No paths_valid rejection test.** The previous summary already flagged
  this — setting up a wall that disconnects the flag needs more than 100
  starting resources. Still skipped.
- **Flag drop test uses exactly one gunner.** Two gunners would shoot
  the retriever down at (6,15) on the same tick (10+10 dmg = 20 HP) and
  the flag would never be picked up — leaving `at_home=1` and no drop to
  observe. One gunner deals one shot per cooldown cycle so the retriever
  survives pickup at (4,15) and dies one tick into the return walk at
  (5,15), producing a clean dropped-flag state outside the at-home cell.
- **`run_sim_to_completion()` for quiet turns.** The sim spends 30
  frames per tick and 90 more on `sim_end_hold` after the last creep
  dies / max ticks reached; the helper loops `game_frame()` until phase
  returns to `PHASE_PLAN_RED`, bounded so a phase-machine regression
  can't hang the test.
- **`game_set_placement` toggle awareness.** Calling it twice with the
  same `TowerType` toggles the intent OFF — an easy footgun. The zone
  test was rewritten to set the intent once and rely on the fact that
  a rejected click leaves `placement_intent` intact for the next click.

## Verification

- `make test` → `87 assertions, 0 failures` (pathing),
  `115 assertions, 0 failures` (game), `35 assertions, 0 failures`
  (render). 237 assertions total.
- No source files under `src/` were modified — only tests and the
  Makefile.
