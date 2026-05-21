# Pathing wobble

Per `docs/game-design.md` §10: "When multiple shortest paths exist (i.e.
there's a tie), creeps randomly choose to add an element of chance."
The code previously broke ties deterministically toward horizontal
movement (BFS expanded +x/-x before +y/-y, so the first parent recorded
was always the horizontal neighbour). Replaced with a uniform random
pick among the tied first-step candidates, sharing the existing
xorshift32 PRNG (`GameState.rng_state`) that already backs tower crits.

## What changed

`src/game/game.c`:
- `rng_next` now takes a `uint32_t *state` instead of reaching into the
  file-static `s.rng_state`. Crit-roll site passes `&s.rng_state`;
  pathing passes `&gs->rng_state`. Same stream, same snapshot field.
- `bfs_to_goal` rewritten as BFS-from-goal: populates a distance field
  (`bfs_dist`, renamed from `bfs_parent` since we no longer trace
  parents). After BFS, collects every walkable neighbour of the source
  whose distance is `source_d - 1` — each is the start of a distinct
  shortest path. Picks one uniformly via `rng_next` when there's more
  than one. Single-candidate case skips the roll, so trivial paths
  don't burn entropy.
- `paths_valid` calls `bfs_to_goal` with `step_x == NULL` (it only
  needs reachability), which short-circuits before any RNG read.
  Planning actions stay deterministic.
- `game_pathing_next_step` signature changed from `const GameState *`
  to `GameState *` — honest about the rng_state mutation. Public header
  comment updated to explain when the RNG is consumed.

`tests/test_pathing.c`:
- Replaced `test_horizontal_tiebreak` with three new tests:
  `test_wobble_picks_among_ties` (both candidates seen across calls),
  `test_wobble_is_deterministic_per_rng_state` (same state → same pick),
  and `test_single_shortest_step_does_not_consume_rng` (paths_valid /
  unique-step pathway proven RNG-pure).
- Loosened `test_carried_flag_is_not_a_goal` and
  `test_dropped_flag_is_a_goal` to accept either tied first-step
  rather than the old horizontal-first answer.

`tests/test_game.c` + `tests/test_fixtures.h`:
- Two existing tests (`test_flag_drop_on_death`,
  `test_banana_creep_carries_and_attacks`) had hard-coded approach
  routes through the 30x20 fixture map and broke under wobble — the
  carrier could now take a path that entered tower range too early or
  bypassed the blocker entirely. Rather than fight wobble with
  ad-hoc seeds or extra towers, added a focused `TEST_MAP_CORRIDOR_CFG`
  fixture: a 20x5 map whose BLUE spawn → RED flag leg is a single
  straight row with no shortest-path ties. Wobble has nothing to wobble
  over on a corridor, so the tests assert tick-precise behavior again.
  Both tests rewritten to use this map and the calibrated tower
  placements documented inline.

`docs/tech-design.md`:
- BFS scratch row renamed `bfs_parent` → `bfs_dist`.

## Determinism

Pathing wobble shares `GameState.rng_state` with the crit roll, so the
snapshot round-trip already in place (the `~N<rng>` section) preserves
the same timeline guarantee: a snapshot loaded twice produces the same
sim outcome. Wobble adds a per-creep-per-tick consumption when ties
exist; iteration order over `things[]` is index-based and unchanged by
load (towers repack but the effective order — towers first, then creeps
in spawn order — matches), so the consumption sequence stays stable.

Pre-existing caveat on mid-`SIMULATE` snapshots applies unchanged:
they restart the sim from tick 0 with the saved (already-advanced)
rng_state, which diverges from the original timeline post-save. Only
planning-phase snapshots reproduce the original future exactly.
