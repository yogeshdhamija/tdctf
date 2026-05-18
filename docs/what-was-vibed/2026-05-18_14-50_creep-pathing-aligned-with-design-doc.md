# Creep pathing aligned with design doc

**Date:** 2026-05-18 14:50
**Files touched:** `src/game/game.c` only.

## What the design doc says (section 10)

> The creep pathing algorithm is: "take the shortest unblocked path to any
> point on the line that's ahead of the furthest point you've already
> visited."

Two ingredients are required:

1. A demarcated **line** of grid cells (not just two endpoints).
2. A creep targets **any point on the line ahead of its furthest-visited
   index**, picking the cell with the shortest unblocked path.

## What was there before

- `path_x[2][MAX_PATH] / path_y[2][MAX_PATH]` only held two points
  (receptacle and enemy flag); `path_len = 2`.
- `bfs(...)` was single-goal: it ran from creep to the flag and used the
  receptacle's `y` coordinate as a tiebreaking *direction-order hint* so
  diverted creeps drifted back to a horizontal lane. That approximated
  line-following but isn't what the doc specifies.
- `Thing.creep.path_progress` was declared in the struct and never read
  or written.

## Changes

### 1. Build the line of cells (`game_init`)

Replaced the two-point assignment with an L-shaped fill from receptacle
to enemy flag, one grid cell per index. With the current map this gives
each player a 22-cell horizontal line; render code already walks
consecutive path points so it draws the same visual line.

### 2. Rewrote BFS as multi-goal via a goal mask

Old approach: pass a single `(gx, gy)` and a `path_y` hint, with custom
direction-order bias for tiebreaking.

New approach:

- `bfs_goal[MAX_GRID_W][MAX_GRID_H]` — caller-populated mask of
  acceptable target cells.
- `bfs_clear_goals()` / `bfs_set_path_forward(player, from_idx)` —
  helpers to fill it.
- `bfs_step(sx, sy, &step_x, &step_y)` — returns 1 if any goal cell is
  reachable; outputs the first step on the shortest such path.

The direction-order hack is gone — line-attraction now falls out
naturally from the goals being on the line.

`walkable_for(...)` is removed (only one caller; the start/goal
exceptions were redundant because BFS never re-expands the start cell
and the previous goal-bypass was no longer needed).

### 3. Simulator uses path_progress

In `sim_one_tick` creep movement:

- Outbound (no flag): goal set is `path_x/path_y[player][i]` for every
  `i > path_progress`. After moving, the creep scans path indices from
  high to low and bumps `path_progress` to the highest index it now
  occupies (so detours don't reset progress).
- Inbound (carrying flag): goal is just the home receptacle — the doc
  is loudest about the outbound case, so I kept return-trip pathing
  simple.

### 4. `paths_valid()` preserves its original meaning

It still asks "is the enemy flag reachable from the receptacle?" — just
expressed as a one-cell goal mask. An earlier draft of the change
relaxed this to "any forward path cell reachable", but that's too
permissive: it would have let a player place a tower that blocks the
flag but not the cell next to the receptacle.

## Observable behavior change

- Creeps now greedily skip forward along the demarcated line as the
  design doc specifies. When the player blocks a path cell, the creep
  BFSes to the next reachable forward path cell and rejoins the line as
  soon as possible — including "knowing" which fork eventually leads to
  the flag without having scouted it, matching the doc's note about
  creeps having information the player doesn't.

## Verification

- `make` succeeds.
- I did not run the game in a browser; the WASM module compiles cleanly
  and no existing call sites of `bfs(...)` remain unupdated.
