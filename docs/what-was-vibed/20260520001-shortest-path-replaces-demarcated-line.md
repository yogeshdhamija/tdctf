# Shortest-path creep movement replaces the demarcated line

## Motivation

The previous pathing system maintained a per-player static "line" — an L-shaped
sequence of cells from spawn → enemy flag → receptacle, drawn on the grid as a
visible path. Creeps walked toward the closest unvisited cell on that line each
tick. This created two layers of state (`path_x/path_y/path_len` plus a
per-creep `path_progress`) and a hardcoded outbound/return geometry that didn't
really match the design's "shortest unblocked path" intuition.

The user wanted the line gone. The new rule is the simplest possible: every
creep just BFS-walks to the flag, then to the receptacle.

## What changed

### Game model (`src/game/game.h`, `src/game/game.c`)

- Removed `MAX_PATH`, `path_x`, `path_y`, `path_len` from `GameState`. There
  is no precomputed path anymore.
- Removed `game_build_path` entirely. `game_init` no longer builds a path.
- Renamed the per-creep `path_progress` (int index into the line) to
  `visited_flag` (boolean). Set to 1 the first tick a creep occupies the
  flag's current cell.
- Rewrote `game_pathing_next_step`: signature now takes `visited_flag`
  instead of `path_progress`. The goal is a single cell — the enemy flag's
  current cell while `visited_flag == 0`, else the creep's own receptacle.
- Replaced the goal-set-based `bfs_step_in` with `bfs_to_goal`, a direct
  source-to-single-goal BFS. The neighbour expansion order remains
  `+x, -x, +y, -y` — horizontal-first — so on a tie BFS returns the
  horizontal step.
- `paths_valid` (placement-time reachability check) now uses two
  `bfs_to_goal` calls per player: spawn → flag, flag → receptacle.

### Sim tick (`src/game/game.c::sim_one_tick`)

- Move loop no longer advances `path_progress`. After each creep's move it
  checks whether the creep is sitting on the enemy flag's current cell and
  sets `visited_flag = 1` if so. Flag pickup remains a separate side-effect
  in the next loop.

### Render (`src/render/render.c`)

- Removed the path-overlay drawing block. There is no line to draw.

### Game design doc (`docs/game-design.md` §10)

- Rewrote the pathing section to describe the new rule: shortest unblocked
  BFS to the flag's current cell, then to the receptacle, with a horizontal
  tiebreak.

## Tests

### `tests/test_pathing.c`

- Kept the full-board feature test (`retriever_walks_full_loop_and_wins`):
  a RED retriever now walks the BFS-shortest spawn → flag → receptacle and
  triggers the win.
- Replaced the line-specific tests with four lower-layer tests:
  - `test_goal_switches_on_visited_flag` — covers the
    `visited_flag ? receptacle : flag` branch in `game_pathing_next_step`.
    Confirmed failing by ignoring the branch.
  - `test_horizontal_tiebreak` — covers the BFS expansion order. Confirmed
    failing by swapping DX/DY to vertical-first.
  - `test_detour_around_obstacle` — covers BFS detouring around a blocked
    cell.
  - `test_carried_flag_is_not_a_goal` — confirms that a carried flag is
    excluded from the goal set, so non-carriers fall back to the
    receptacle. Confirmed failing by removing the `carried_by == -1` guard.
  - `test_dropped_flag_is_a_goal` — confirms the flag's *current* (not
    home) cell drives the goal when it's been dropped off-line.

### `tests/test_game.c::test_initial_state`

- Removed the `path_len`/`path_x`/`path_y` assertions (those fields are
  gone).

## Behaviour notes

- A creep that arrives at the flag's cell, regardless of type, flips its
  `visited_flag` and then heads to its own receptacle on the next tick.
  Siege creeps (which never pick up the flag) still need to "touch" the
  flag's current cell to progress to phase 2.
- A carried flag is NOT a goal — non-carriers in phase 1 fall back to
  their own receptacle while the flag is being carried, so they don't
  tail-chase a teammate. `visited_flag` is not set during this fallback,
  so if the carrier later dies and the flag drops, the followers revert
  to heading toward the new dropped-flag location.
- The horizontal tiebreak emerges from the `{+x, -x, +y, -y}` expansion
  order in BFS, which means whenever two cells are tied for the shortest
  distance from the source, the horizontal neighbour is the BFS-tree
  ancestor and therefore the returned first step.
