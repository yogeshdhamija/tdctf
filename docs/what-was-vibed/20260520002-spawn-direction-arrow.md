# Spawn-direction arrow at each creep spawn

## Motivation

Previously the creep spawn cells were not visually marked at all — only the
receptacles and flags had on-grid indicators. A player looking at the map
couldn't tell at a glance where creeps would appear or which way they'd head
out, especially after towers reshape the shortest path.

The user asked for an arrow drawn at each spawn cell, pointing the way the
creeps are about to go.

## What changed

### Render (`src/render/render.c`)

Added a spawn-direction arrow block right after the receptacles are drawn.
For each player it:

- Calls `game_pathing_next_step(gs, spawn_x, spawn_y, owner, /*visited_flag=*/0, &nx, &ny)`
  to get the first BFS step the next creep would take out of the spawn cell.
- Skips if there's no path (`paths_valid` should prevent this at placement
  time, but the guard keeps it safe) or if the step lands on the spawn
  itself (already at the goal).
- Draws a line + filled triangle head in the player's color, 22px long
  across the spawn cell's center, oriented along the cardinal direction
  `(dx, dy) = (nx - sx, ny - sy)`. The perpendicular for the arrowhead is
  `(-dy, dx) * 5` so the head is the same width in any orientation.

Because the arrow is derived live from the same BFS the simulation uses, it
updates in the planning phase as towers are placed/removed and the shortest
path changes.

## Tests

None added — this change is purely visual per
`docs/ai-instructions/coding-norms.md` ("Colors, font sizes, spacing,
animation timing, layout nudges. Reviewed by eye."). The underlying pathing
behavior is already covered by `tests/test_pathing.c`; the new render block
just reads from it.

`make test` (all existing suites) and the full `emcc` web build both pass.
