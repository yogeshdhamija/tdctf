# Test infrastructure + tests for creep pathing

**Date:** 2026-05-18 15:00
**Files touched:** `tests/test_pathing.c` (new), `Makefile`.

## Why

`coding-norms.md` requires consumer-driven testing: "all code changes are
driven by tests written at the boundary of the interface being changed."
The preceding pathing change shipped without any. This adds the missing
tests and the harness they need.

## What

### Native test target

The project's only build target was the emscripten/WASM bundle. Added a
native build of `game.c` against a test file using plain `cc`, with
`-O0 -g -Wall -Wextra`. `game.c` has no emscripten dependency so this
"just works." New Makefile targets:

- `make test` — builds and runs `build/test_pathing`.
- `build/test_pathing` — the test binary, built from
  `tests/test_pathing.c` + `src/game/game.c`.

### `tests/test_pathing.c`

A single-file test, no dependencies beyond stdio. One `CHECK` macro, a
test name string, a counter. Each test calls `game_init()` and drives
the simulation through the public `game_*` API; assertions read state
via `game_get_state()`. No internal helpers (`bfs_step`,
`bfs_set_path_forward`, etc.) are touched directly — per
`coding-norms.md` §3 they're covered transitively by the consumer test.

Four tests:

1. **`test_line_following_unobstructed`** — on the default map, the RED
   retriever should walk one cell per tick along the line and
   `path_progress` should tick up in lockstep. Catches: goal set being
   the flag only (path_progress would stay 0), or BFS losing the
   demarcated line entirely.

2. **`test_detour_around_blocking_tower`** — RESOURCE tower on path
   index 6. The creep must never occupy the blocked cell and
   `path_progress` must eventually exceed 6. Catches: creep getting
   stuck, or rejoining the line at a backward cell.

3. **`test_path_progress_monotonic`** — same scenario, asserts
   `path_progress` is non-decreasing across every tick of the outbound
   trip. Catches: the high-to-low scan in `sim_one_tick` overwriting a
   larger value with a smaller one.

4. **`test_placement_validity`** — receptacle and at-home flag cells
   are rejected; a normal path-cell placement is accepted. Indirectly
   exercises the `paths_valid` branch of `placement_valid`.

## Choices worth flagging

- **TOWER_RESOURCE used for the obstacle.** GUNNER would shoot the
  retriever and turn the detour test into a damage test. RESOURCE is
  build-locked but still occupies the grid cell (which is all the test
  cares about).
- **Hard-coded expected positions in test 1.** The unobstructed case
  has only one shortest first-step at every tick, so position is
  deterministic regardless of BFS expand order.
- **No paths_valid rejection test.** A clean "this placement is
  rejected because it disconnects the flag" needs more than 100
  starting resources to set up the wall, and accumulating across turns
  would balloon the test. Skipped for now; tests 2 and 4 cover the
  accept-side. A debug-only "set resources" API would unblock this if
  we want it later.

## Verification

- `make test` → `87 assertions, 0 failures`.
- `make` (full WASM build, forced rebuild) still succeeds.
