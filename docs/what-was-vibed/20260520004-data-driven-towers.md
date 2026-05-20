# Towers are now defined entirely by the config file

The previous change (20260520003) moved tower *numbers* into `data/towers.cfg`
but kept the four tower **types** hardcoded in `game.h` as an enum
(`TOWER_BLOCKER`, `TOWER_GUNNER`, `TOWER_SLAMMER`, `TOWER_RESOURCE`). The
config also baked in implicit "this kind of tower has these fields" rules
via the per-tower section layout.

This change finishes the job:

- **No enum.** `TowerType` is now `typedef int TowerType` — towers are
  identified by runtime indices assigned by `tower_config` at parse time,
  in declaration order. Adding a new tower means adding it to
  `data/towers.cfg`; no code changes anywhere.
- **Every tower is a generic stat bag.** There is no "blocker doesn't
  attack" or "only resource towers generate income" rule. Any tower can
  damage, AoE, slow, and generate income — the behavior is purely a
  function of which numbers are non-zero at the current level.
- **Each level is its own section.** Each level fully redefines `cost`,
  `hp`, `build_turns`, and all combat/income stats. There is no
  inheritance from level N-1.

## New file format

```
tower BUFFALO
  code U
  name Buffalo

level BUFFALO 1
  cost         50
  hp           60
  build_turns  2
  dmg          4        # attacks
  range        3
  aoe          1        # ...and AoEs
  slow         1        # ...and slows
  cooldown     2
  income       5        # ...and generates income

level BUFFALO 2
  cost         40       # cost to upgrade from L1
  hp           90       # absolute, not bonus
  build_turns  1
  dmg          0        # L2 can fully reshape behaviour
  range        0
  income       25
```

Sections:
- `tower <ID>` — identity block; keys are `code` and `name` only.
- `level <ID> <N>` — level-N stats; must come after the tower section, and
  level numbers must be sequential (1, 2, 3...). Levels can range up to
  `TOWER_MAX_LEVELS`.

## API changes

New on `game.h`:
- `int game_tower_count(void)` — live count from the catalog.
- `int game_tower_id(const char *name)` — string id → index; returns -1
  if not found.
- `int game_tower_upgrade_cost(TowerType, int from_level)` — cost of
  `from_level → from_level+1`; returns 0 if already at max.
- `int game_tower_max_level(TowerType)` — number of levels defined.

Removed: `TOWER_BLOCKER`, `TOWER_GUNNER`, `TOWER_SLAMMER`,
`TOWER_RESOURCE`, `TOWER_TYPE_COUNT`. The `TowerType` typedef remains as
`int` for self-documenting code.

## Render & platform changes

`ButtonID` no longer enumerates one entry per tower. The placement palette
uses `BTN_PLACE_TOWER_BASE + <tower index>` so it resizes automatically
with the catalog. `platform_web.c` dispatches any button id at or above
`BTN_PLACE_TOWER_BASE` to `game_set_placement(id - BTN_PLACE_TOWER_BASE)`.

## Tests

- `tests/test_tower_config.c` rewritten — 58 assertions, 6 tests. Covers
  default values, comment/whitespace handling, combined-feature parsing,
  per-level independence, structural errors (level-before-tower,
  non-sequential levels, duplicate towers, empty tower), and declaration
  order → index mapping. The level-sequence check and "tower with no
  levels" check were observed failing under mutation before being kept.
- `tests/test_game.c` & `tests/test_render.c` updated to use
  `game_tower_id("GUNNER")` instead of `TOWER_GUNNER` everywhere.
- `tests/test_render.c`'s button-presence table now sized to cover the
  dynamic placement-button range.

`make test`: 4 binaries, 233 assertions, 0 failures.
`make`: WASM build produces `build/index.html` cleanly.

## What a new tower looks like end-to-end

Add this to `data/towers.cfg` and rebuild — no other changes needed:

```
tower BUFFALO
  code U
  name Buffalo
level BUFFALO 1
  cost 60
  hp   80
  ...
```

The renderer's placement palette will show the new button on the next
build. The game's resources, placement-validity, attack, and upgrade
logic all consult the catalog dynamically, so the tower is
fully-functional immediately.

## What didn't change

- The renderer's per-tower draw is still "glyph + level number" — the
  visual presentation is generic, so towers added via config look exactly
  like the existing four. Custom visuals per tower would need a render
  change.
- Creep upgrades (`init_creep_upgrades`) are still hardcoded — out of
  scope for this change.
- `TOWER_MAX_COUNT = 16`, `TOWER_MAX_LEVELS = 8` are still compile-time
  ceilings. Add headroom by editing `tower_config.h`.
