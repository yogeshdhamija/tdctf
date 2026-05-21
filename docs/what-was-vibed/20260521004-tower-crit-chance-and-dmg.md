# Tower critical hits

Added two per-level tower stats, configurable in `data/towers.cfg` like every
other level field:

- `crit_chance` — integer percent (0–100) chance that a given attack lands as
  a critical hit
- `crit_dmg` — damage substituted for `dmg` when the crit roll succeeds

Defaults to 0/0, so towers that don't set these continue to behave exactly as
before.

## What changed

- `TowerLevelStats` (src/game/tower_config.h) gains two `int` fields. Parser
  in `tower_config.c::set_level_field` learns the new keys.
- `game.c` grows a tiny xorshift32 PRNG whose state lives in
  `GameState.rng_state` (so the snapshot round-trips it). Seeded to a fixed
  value in `game_init_state()` so sims are deterministic across runs (the
  test suite relies on this). The tower attack loop in `sim_one_tick` rolls
  once per attack; on a crit, `crit_dmg` replaces `dmg` for both
  single-target hits and AoE splash (uniform across all targets).
- `data/towers.cfg` documents the new keys and gives GUNNER L3 ("sniper") a
  20% crit chance for 200 damage — a thematic showcase, leaves L1/L2 alone.
- Snapshot format gains a `~N<rng>` section carrying the PRNG state, encoded
  via signed reinterpretation so values above INT_MAX round-trip through
  `read_int`. Missing N (older snapshots) leaves the freshly-init'd seed in
  place — backward-compatible by way of the loader's unknown-section skip.

## Tests

- `tests/test_tower_config.c::test_combined_features` now pins
  `crit_chance=25, crit_dmg=17` alongside the other fields; a follow-up
  fragment verifies the implicit-zero default when the keys are omitted.
- `tests/test_game.c::test_tower_crit_uses_crit_dmg` exercises the sim
  branch end-to-end with a `CRITTER` test fixture
  (`crit_chance=100, dmg=1, crit_dmg=999`) and asserts the retriever
  (HP 20) is one-shot — impossible without the crit replacement, since 1
  dmg over ~2-tick cooldown cannot kill in the test budget.
- `tests/test_snapshot.c::test_round_trip_preserves_rng_state` sets
  `rng_state = 0xDEADBEEFu` (high-bit set, to exercise the signed encoding
  edge), encodes, reloads from a clean init, and asserts the value survives.
- `tests/test_snapshot.c::test_legacy_snapshot_without_rng_loads` feeds a
  hand-crafted v1 snapshot string with no `N` section and verifies the
  loader leaves the freshly-init seed in place.

All three sim/snapshot tests were observed failing before their respective
changes landed.
