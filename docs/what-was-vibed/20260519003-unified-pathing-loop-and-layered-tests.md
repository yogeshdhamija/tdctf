# Unified loop pathing + restructured pathing tests

**Date:** 2026-05-19
**Files touched:** `src/game/game.h`, `src/game/game.c`,
`tests/test_pathing.c` (rewritten), `tests/test_game.c`,
`tests/test_render.c`, `docs/game-design.md`.

## Why

Two problems with the previous pathing design:

1. The demarcated line ran only one way — receptacle → enemy flag — and
   the return journey was a separate branch inside the creep movement loop
   (`if (has_flag) { goal = receptacle } else { goal = forward line cells }`).
   That branch is the very thing the unified rule in `docs/game-design.md`
   §10 is meant to eliminate: every creep follows the same line, full stop.
2. The line started at the receptacle, which conflated "where creeps spawn"
   with "where the flag must be returned." On future maps these need to be
   distinct cells.

And one test-design problem:

3. `test_pathing.c` had four tests, each spinning up a full game board (init,
   buy upgrade, run a quiet turn, enter sim again) to exercise a single
   branch of `bfs_step`. Per `coding-norms.md` "Layered Testing," branch
   coverage for a sub-behavior sits at *that* sub-behavior's layer — not by
   stacking cases on top of a feature-layer test.

## Changes

### Game layer (`src/game/game.{h,c}`)

- **New fields** `spawn_x[2], spawn_y[2]` on `GameState`. Creeps spawn here,
  not at the receptacle. Default map: RED spawn `(0,10)`, BLUE spawn
  `(29,10)`. Receptacles unchanged (`(4,4)` and `(25,15)`).
- **Path is a loop** spawn → enemy flag → own receptacle. The construction
  is now `game_build_path(gs, p)` — exposed in `game.h` so tests can
  construct hand-built scenarios without `game_init`.
- **Unified pathing primitive** `game_pathing_next_step(gs, cx, cy, owner,
  path_progress, &nx, &ny)`. One step toward the closest cell on the line
  that hasn't been visited (index > `path_progress`), with the enemy flag's
  current cell added as a goal when it's on the ground. The creep's own
  cell is explicitly removed from the goal set so the step always moves.
  Carried flags are *not* goals — otherwise every other creep would chase
  the carrier instead of walking its line.
- **Removed `has_flag` branch from pathing.** Every creep — retriever or
  siege — uses the same call. `has_flag` only affects whether the flag's
  position is dragged along after the move. Flag pickup itself stays where
  it was: a separate side-effect of arriving on the flag's cell.
- **`path_progress` update** changed to "smallest index > current progress
  whose cell matches the creep's new cell" (was: largest). Smallest-k makes
  forward progress robust if the line ever revisits a coord — the progress
  walks one position at a time instead of jumping ahead.
- **`paths_valid` now checks both halves** of every player's loop: BFS from
  spawn must reach the enemy flag, AND BFS from the flag must reach the
  receptacle. Placing a tower that cuts either half is rejected.

### `tests/test_pathing.c` — rewritten from scratch

One feature-layer test, four lower-layer tests, zero duplicate coverage:

- `test_retriever_walks_full_loop_and_wins` — the only test that boots the
  full game. RED buys a retriever; after a quiet research turn it spawns at
  `(0,10)`, walks the entire line, picks up BLUE's flag at `(25,4)`,
  continues to `(4,4)`, wins. One code path through every layer (spawn →
  unified pathing → flag pickup → win check).
- `test_next_step_follows_line` — at the pathing layer. Constructs a tiny
  10×10 GameState; probes that the next step from spawn is +1 along the
  line, and the next step from the flag cell is onto the return half.
- `test_detour_around_obstacle` — same layer. Blocks a line cell and
  asserts the first step routes around it.
- `test_dropped_flag_is_a_goal` — same layer. A dropped flag (carried_by
  = -1, off the static line) becomes an extra BFS goal; when it's closer
  than the nearest reachable line cell, the creep heads for it.
- `test_carried_flag_is_not_a_goal` — same layer, same geometry as the
  previous test but with `carried_by != -1`. The flag is excluded from the
  goal set and the creep falls back to the line — a different first step
  (`(1,1)` instead of `(0,0)`), proving the conditional.

The lower-layer tests construct a `GameState` on the stack and call
`game_build_path` / `game_pathing_next_step` directly. No `game_init`, no
sim ticks, no turn machinery.

### `tests/test_game.c`

Geometry-dependent assertions updated for the new path:

- `test_initial_state` — added `spawn_x/y` assertions; path now starts at
  spawn and ends at receptacle.
- `test_completed_upgrade_spawns_retriever` — retriever expected at
  `(0,10)` (RED spawn), not `(4,4)` (receptacle).
- `test_gunner_damages_creep`, `test_slammer_slows_creep`,
  `test_flag_pickup` — tick budgets bumped to account for the longer BLUE
  path (`29,10 → 4,10 → 4,15` is ~10 ticks farther than the old straight
  east-west run).
- `test_flag_drop_on_death` — gunner relocated from `(4,14)` to `(5,15)`.
  At `(4,14)` the new path would put the gunner squarely on the approach
  *to* the flag, killing the carrier before pickup and producing no drop.
  At `(5,15)` the gunner sits on the first cell of the return half: the
  retriever picks up the flag at `(4,15)`, takes its first detour step to
  `(4,16)`, and dies there with the flag.
- Added `test_placement_validity` (moved from `test_pathing.c` — placement
  validity is a game-layer concern, not a pathing concern).

### `tests/test_render.c`

`test_stacked_creep_count_badge` — pinned coords moved from `(25,15)`
(BLUE receptacle) to `(29,10)` (BLUE spawn).

### `docs/game-design.md`

§10 rewritten to describe the loop line and the unified algorithm.

## Choices worth flagging

- **Spawn ≠ receptacle on the default map.** The user explicitly asked for
  this so the new fields aren't dead weight. The current cells are picked
  to be on opposite sides of each player's zone, which produces a U-shaped
  loop that's clearly visible on the map.
- **No tests for the spawn-==-receptacle loop case.** With identical
  endpoints the line collapses into a true loop (path[0] == path[N-1])
  and the "closest unvisited cell" rule has tiebreaking issues — siege
  creeps could pick path[N-1] over path[1] at the very first step. The
  user chose distinct cells, so the loop case is left as a known
  limitation rather than handled.
- **Siege creeps walk the full loop.** With `has_flag` removed from
  pathing, every creep heads for the receptacle once the flag and return
  half are visited. Siege creeps thus end up parked at their own
  receptacle if they survive — harmless for MVP, and the user explicitly
  picked "fully unified" over a `creep_type` branch.
- **`test_placement_validity` did *not* gain a loop-severing case.** The
  starting 100 resources isn't enough to actually wall off either half of
  the new loop, so a clean unit test would need more setup than warranted
  for a property that's covered indirectly by every other placement test
  (each placement attempt runs `paths_valid` under the hood).
- **`game_pathing_next_step` takes `const GameState *`, not the global.**
  Required to make hand-built test states callable without poking the
  module-level `s`. Internally `sim_one_tick` just passes `&s`.

## Verification

- `make test` → `12 assertions, 0 failures` (pathing),
  `121 assertions, 0 failures` (game), `42 assertions, 0 failures`
  (render). 175 assertions total.
- `make` produces `build/index.html` cleanly.
- New tests verified failing against pre-change behavior: the high-level
  test asserts `winner == PLAYER_RED`, which RED couldn't achieve before
  (RED had no retriever spawning at all because its only path went from
  receptacle outbound, and the previous tests already showed it never won
  on its own). The lower-layer tests call `game_pathing_next_step` which
  didn't exist in the previous code.
