# Spawn order + line-spawn

Creeps used to materialise as a stack on the spawn cell at tick 0 of every
sim: `start_simulation` walked completed upgrades and spawned every
(spawn_type × spawn_count) before the first movement step. Visually
unreadable when the wave was big, and gave no way for the cfg to express
"send the bruisers in front of the retrievers."

This change adds a `spawn_order` field on creep types and shifts spawning
from a single up-front burst to a one-per-tick drain off a per-player queue.

## Data model

- `CreepTypeConfig.spawn_order` (int, default 0). Lower spawns first; ties
  preserve declaration order. Parsed via the standard `set_type_field`
  path in `creep_config.c`.
- `Player.spawn_queue[MAX_SPAWN_QUEUE]` + `spawn_queue_count` +
  `spawn_queue_pos`. Sized at compile time (64) like everything else in
  the zero-heap model. Overflow is silent — bump the cap if cfgs ever
  push past it.

## Sim flow

`start_simulation` now builds the queue: for each completed upgrade, push
`spawn_count` copies of `spawn_type` into the player's queue, then
insertion-sort by `spawn_order`. `sim_one_tick` calls
`drain_spawn_queue(p)` for both players at its very top — pop at most one
creep onto the spawn cell — before the movement pass runs. Putting the
drain *before* movement means the freshly-spawned creep takes its first
step on the same tick, so the "after N ticks the first creep is at step N"
timing the older all-at-once model implied is preserved for the head of
the wave.

Existing tower behaviors, pathing, flag pickup, snapshot encode/decode
are unchanged. Snapshot reload still ends in `start_simulation()` when
phase is SIMULATE, which now also rebuilds the queue.

## Shipped data

`data/creep_upgrades.cfg` now declares all four creep types with explicit
spawn orders — BRUISER (1) tanks at the front, then SIEGE (2) opens
lanes, then RETRIEVER (3) carries the flag through the cleared path, then
SWARM (4) fills in. Tunable in the cfg without code changes.

## Test changes

- `test_zero_research_turns_spawns_same_turn`,
  `test_completed_upgrade_spawns_retriever`,
  `test_siege_attacks_tower`, `test_banana_creep_carries_and_attacks`,
  `test_simulate_phase_respawns_creeps` (snapshot): all checked creep
  counts immediately after `enter_sim()`. Added `step_ticks(N)` before
  the count so the queue has time to pop N creeps.
- `test_stacked_creep_count_badge` (render): old setup relied on two
  retrievers landing on the spawn cell at tick 0. With the new model
  they never overlap organically, so the test now drives the game to a
  state with two BLUE creeps and snaps the second one onto the first
  cell via direct state mutation, then asserts the badge renders. Also
  un-commented the badge-presence assertion, which had silently rotted.
- New tests:
  - `test_spawn_order_controls_queue` (game layer): two creep types with
    spawn_order 1 and 2, both bought; the lower-ordered one appears on
    tick 1, the other on tick 2.
  - `test_creeps_spawn_one_per_tick` (game layer): the +2 SIEGE upgrade
    pops exactly one creep per tick, not both at tick 0.
  - `test_spawn_order` (creep_config layer): parser accepts the new key,
    defaults to 0, rejects non-integer values.
- New fixture `TEST_CREEP_SPAWN_ORDER_CFG` in `tests/test_fixtures.h`
  for the queue-ordering test.
