# Creep upgrades extracted to a config file

Tower stats live in `data/towers.cfg` and are loaded by `tower_config`
at startup (20260520003, 20260520004). Creep upgrades stayed hardcoded in
`init_creep_upgrades` in `game.c`. This change brings creep upgrades in
line with the same data-driven shape.

## New cfg file: `data/creep_upgrades.cfg`

Four upgrades, one section per upgrade. The keys mirror the dynamic
fields used by `count_spawns` and `game_buy_creep_upgrade`:

```
upgrade RETRIEVER_1
  cost            30
  research_turns  1
  add_retrievers  1
  description     +1 Retriever
```

Section keys: `cost`, `research_turns`, `add_retrievers`, `add_siege`,
`description`. Unspecified keys default to 0 / empty.

## New module: `src/game/creep_config.{c,h}`

Parallels `tower_config.{c,h}`:

- `CreepUpgradeConfig` — static spec for one upgrade.
- `CreepUpgradeCatalog` — `{ count, upgrades[CREEP_UPGRADE_MAX_COUNT] }`.
- `creep_config_get` / `_load_default` / `_load_from_string` / `_lookup`.

Parser differs from tower_config in one place: `description` may contain
spaces, so values consume the rest of the line (with whitespace trimmed)
rather than tokenizing into N words. Single-word values like `cost 30`
still work transparently.

The Makefile generates `build/creep_config_data.h` from
`data/creep_upgrades.cfg` exactly the same way `tower_config_data.h` is
generated, embedding the cfg text as a C string literal.

## Game / render changes

- `CreepUpgrade` slimmed down to dynamic state only:
  `{ turns_remaining, purchased, completed }`. The previous `id`, `cost`,
  `research_turns`, `add_retrievers`, `add_siege`, `description` fields are
  gone — read them from the catalog by parallel index.
- `init_creep_upgrades` sizes the per-player array from the catalog and
  zeroes the dynamic state.
- `count_spawns` and `game_buy_creep_upgrade` look up static fields via
  `creep_config_get()`.
- New accessors on `game.h` for the render layer:
  - `game_creep_upgrade_count()`
  - `game_creep_upgrade_cost(int idx)`
  - `game_creep_upgrade_research_turns(int idx)`
  - `game_creep_upgrade_description(int idx)`
- `render.c`'s sidebar reads upgrade spec via those accessors instead of
  the struct fields.

## Init hooks

- `game_init()` now loads both `data/towers.cfg` and
  `data/creep_upgrades.cfg`.
- `game_init_with_tower_config(cfg)` keeps default creep upgrades.
- `game_init_with_creep_config(cfg)` keeps default towers (new).
- `game_init_with_configs(tower_cfg, creep_cfg)` pins both (new) — used by
  behavior tests so they stay decoupled from the shipped data
  (continuing the pattern from 20260520005).

## Tests

- `tests/test_creep_config.c` — 37 assertions, 6 tests covering basic
  parse, multi-word descriptions with whitespace preservation, comment /
  blank-line handling, default-zero behavior, structural errors
  (key-outside-section, unknown key, non-integer value, missing
  `upgrade` id, duplicate id, missing value), and declaration-order →
  index mapping. The description and dup-id checks were observed failing
  under mutation before being kept.
- `tests/test_fixtures.h` adds `TEST_CREEP_UPGRADES_CFG` documenting the
  four slot indices that `test_game.c` and `test_render.c` reference.
- `test_game.c` and `test_render.c` switched from
  `game_init_with_tower_config(TEST_TOWERS_CFG)` to
  `game_init_with_configs(TEST_TOWERS_CFG, TEST_CREEP_UPGRADES_CFG)`. The
  cost-60 / research-turns-2 numbers in
  `test_creep_upgrade_purchase_and_research` are now read via
  `game_creep_upgrade_cost(2)` / `_research_turns(2)`.

`make clean test`: 5 binaries, 226 assertions, 0 failures.
`make`: WASM build produces `build/index.html` cleanly.
