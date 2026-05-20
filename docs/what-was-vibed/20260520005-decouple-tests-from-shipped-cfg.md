# Behavior tests pin a fixture config, not the shipped one

After 20260520004 made towers fully data-driven, the behavior tests in
`test_game.c` and `test_render.c` were still implicitly coupled to whatever
`data/towers.cfg` happened to contain. Several tests pinned specific
numbers (`max_hp == 70`, `resources == 20` after a placement, the GUNNER
2-shot kill math in `test_flag_drop_on_death`, etc.), so editing the
shipped config would silently break the suite.

This change decouples them: behavior tests now load a frozen fixture
config independent of `data/towers.cfg`.

## How

- New `void game_init_with_tower_config(const char *cfg)` on `game.h`.
  Internally `game_init()` is split into "load default config" +
  `game_init_state()` (everything else); the new entrypoint substitutes
  the first half. The render/platform layer keeps calling plain
  `game_init()`, which still pulls the shipped config.
- New `tests/test_fixtures.h` with a `TEST_TOWERS_CFG` string literal.
  Every behavior test in `test_game.c` and `test_render.c` now calls
  `game_init_with_tower_config(TEST_TOWERS_CFG)` instead of
  `game_init()`. The fixture documents in a header comment which numbers
  the tests actually pin (and why).
- `Makefile` adds `-Itests` and an explicit dep on
  `tests/test_fixtures.h` for the affected test binaries.

`tests/test_tower_config.c::test_default_config_values` continues to pin
the shipped config. That's the *one* test that should fail when the cfg
is edited — its whole job is to flag accidental drift.

## Verified

Mutated `data/towers.cfg` to set GUNNER `cost = 999` (which under the old
coupling would crater the placement, upgrade, gunner-damage, and
flag-drop tests). Result: only `test_default_config_values` failed.
After restoring, all 233 assertions pass across 4 binaries.

## Effect

Editing `data/towers.cfg` is now a data-only change. Add a new tower,
delete an old one, retune numbers — only `test_default_config_values`
will yell, and updating it is mechanical. The fixture file changes only
when a behavior test pins a new number.
