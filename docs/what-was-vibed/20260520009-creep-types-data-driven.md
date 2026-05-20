# Creep behaviors are now data-driven

20260520008 extracted creep upgrades into a config file, but the *creep
types themselves* — what each creep can do — were still hardcoded:

- `CreepType` was an enum with two members (`CREEP_RETRIEVER`,
  `CREEP_SIEGE`).
- "Picks up the enemy flag" was hardcoded to `CREEP_RETRIEVER`.
- "Melee-attacks adjacent enemy towers for 5 dmg" was hardcoded to
  `CREEP_SIEGE`.
- HP at spawn was `(ct == CREEP_SIEGE) ? 40 : 20`.

This change finishes the job for creeps the same way 20260520004 finished
it for towers: behaviors live in config, not in code. Adding a "BANANA"
creep type that BOTH carries the flag AND damages towers is now a
data-only change.

## New cfg shape

`data/creep_upgrades.cfg` now has two section kinds:

```
creep RETRIEVER
  code            R
  hp              20
  can_carry_flag  1

creep SIEGE
  code            S
  hp              40
  melee_damage    5

upgrade SIEGE_2
  cost            40
  research_turns  1
  spawn           SIEGE 2
  description     +2 Siege
```

- `creep <ID>` defines a creep type. Keys: `code` (1-char badge glyph),
  `hp`, `can_carry_flag` (0/1), `melee_damage` (0 = no melee).
- `upgrade <ID>` references a creep type via `spawn <CREEP_ID> <N>`.
  Upgrades without a `spawn` directive contribute no creeps (reserved
  for future stat-buff style upgrades).

A creep type can combine any subset of behaviors — `can_carry_flag=1`
and `melee_damage>0` is a normal config, not a special case.

## API changes

- `CreepType` is now `typedef int` (was an enum). `CREEP_RETRIEVER`,
  `CREEP_SIEGE`, `CREEP_TYPE_COUNT` are gone.
- New accessors on `game.h`:
  - `game_creep_type_count()`
  - `game_creep_type_id(const char *name)` — name → index, -1 if missing.
  - `game_creep_type_code(CreepType t)`
  - `game_creep_type_can_carry_flag(CreepType t)`
  - `game_creep_type_melee_damage(CreepType t)`
- `creep_config.h` exposes `CreepTypeConfig`, `CreepUpgradeConfig`,
  `CreepCatalog`, and `creep_config_lookup_type` /
  `_lookup_upgrade`. `CreepUpgradeConfig` lost `add_retrievers` and
  `add_siege` in favor of `spawn_type` (index) + `spawn_count`.

## Game changes

- `spawn_creep` reads HP from the type catalog instead of hardcoding by
  enum.
- Flag-pickup loop now gates on `ct_spec(type)->can_carry_flag`.
- The siege-melee loop is replaced by a generic creep-melee loop: any
  creep type with `melee_damage > 0` deals that damage to the first
  adjacent enemy tower it finds, once per tick.
- `start_simulation` now iterates each player's completed upgrades and
  spawns `spawn_count` creeps of `spawn_type` per upgrade; the old
  retriever/siege special cases are gone.

## Render changes

- The per-cell crowding badge is now generic. It iterates
  `game_creep_type_count()` and concatenates `"<count><code>"` for every
  type present at the cell. `"2R"`, `"2R1S"`, `"3N"` all fall out of the
  same code path.
- Heavy/light circle styling derives from `melee_damage > 0` instead of
  the enum, so new "fighter" creep types render correctly without
  per-type art.
- `creep_cnt[]` is sized by `CREEP_TYPE_MAX_COUNT` (8) instead of the
  defunct `CREEP_TYPE_COUNT`.

## Tests

- `tests/test_creep_config.c` rewritten — 59 assertions, 8 tests.
  Includes new cases: forward references in `spawn` are rejected, unknown
  creep ids in `spawn` are rejected, missing count token is rejected,
  duplicate creep ids are rejected, the combined-behaviors property
  (carry+melee on one type) parses cleanly. The forward-ref and unknown-id
  checks were observed failing under mutation before being kept.
- `tests/test_game.c` adds `test_banana_creep_carries_and_attacks`. A
  BANANA-style creep type (`can_carry_flag=1`, `melee_damage=3`) is the
  ONLY creep on the field. The test asserts that one BANANA creep both
  (a) damages an adjacent RED blocker and (b) eventually picks up the
  flag — i.e. the two behaviors are independent and compose. The new
  `TEST_CREEP_BANANA_CFG` fixture lives in `test_fixtures.h`.
  Pre-existing tests switched from `CREEP_RETRIEVER`/`CREEP_SIEGE` to
  `game_creep_type_id("RETRIEVER")` / `"SIEGE"`.
- The `TEST_CREEP_UPGRADES_CFG` fixture grew `creep` sections matching
  the old hardcoded defaults so existing behavior tests still pin
  exactly what they always pinned.

Mutated `can_carry_flag` check, `melee_damage` lookup, and
`creep_config_lookup_type` resolution under `spawn` — each broke
exactly the tests that pin the corresponding behavior.

`make clean test`: 5 binaries, 252 assertions, 0 failures.
`make`: WASM build produces `build/index.html` cleanly.

## What a new creep type looks like end-to-end

Add this to `data/creep_upgrades.cfg` and rebuild — no code changes:

```
creep BANANA
  code            N
  hp              30
  can_carry_flag  1
  melee_damage    3

upgrade BANANA_UPG
  cost            50
  research_turns  1
  spawn           BANANA 2
  description     +2 Banana
```

The planning sidebar picks up the new upgrade button, the simulation
honors `can_carry_flag` and `melee_damage` automatically, and the
per-cell crowding badge displays `"2N"` for stacked BANANAs.
