# Spawn marker: arrows → non-directional nested circles

Previously each spawn cell rendered an arrow pointing along the first BFS
step toward the enemy flag. Replaced with two concentric circles in the
player's color — non-directional, visually paired with the receptacle's
nested-rectangle marker so the two grid landmarks read as a matched set
(rectangles = receptacle, circles = spawn).

Change is confined to `src/render/render.c`. `game_pathing_next_step` is
still used by creep movement (`game.c:436`), so the function and its
tests are untouched.

Purely visual; no test added per `docs/ai-instructions/coding-norms.md`.
