# Tower stats moved to data/towers.cfg

Tower base stats and per-level upgrade data were hardcoded in `src/game/game.c`
across three sites: a `TOWERS[]` table, a `tower_attack()` switch, and inline
constants in `game_upgrade_selected` / `end_simulation`. This change moves all
of it into a single human-editable text file.

## File format

`data/towers.cfg` — sectioned text, one fact per line:

```
tower GUNNER
  cost              30
  hp                50
  build_turns       0
  code              G
  name              Gunner
  upgrade_cost      30
  upgrade_build     1
  upgrade_hp_bonus  20
  level1.dmg        10
  level1.range      3
  level1.cooldown   2
  level2.dmg        15
  level2.range      4
  level2.cooldown   2
```

- `#` starts a comment to end-of-line. Blank lines and indentation are ignored.
- Section is `tower <ID>` where `<ID>` is one of `BLOCKER`, `GUNNER`, `SLAMMER`,
  `RESOURCE` (matches the `TowerType` enum).
- Keys: `cost`, `hp`, `build_turns`, `code`, `name`, `upgrade_cost`,
  `upgrade_build`, `upgrade_hp_bonus`. Per-level keys (`level1.*`, `level2.*`):
  `dmg`, `range`, `aoe`, `slow`, `cooldown`, `income`.
- Unknown sections, unknown keys, malformed integers, and orphan key lines
  all fail the parse.

## How it's loaded

Embedded at build time. A Makefile rule pipes `data/towers.cfg` through `sed`
to produce `build/tower_config_data.h`, which defines a `const char
TOWER_CONFIG_DEFAULT[]` string literal. Both the WASM build and the native
test binaries `#include` it through `src/game/tower_config.c`. The
single-file `build/index.html` therefore continues to need no external
files at runtime — the config is baked into the wasm.

`game_init()` calls `tower_config_load_default()`, which parses the embedded
string into a static `TowerCatalog`. All tower lookups in `game.c` go through
`tower_config_get()`.

## Files

- `data/towers.cfg` — new, source of truth
- `src/game/tower_config.h` / `.c` — new parser, ~130 lines combined
- `tests/test_tower_config.c` — new; 4 tests, 29 assertions; covers the
  embedded defaults, comment/whitespace handling, error paths, and
  multi-section dispatch
- `src/game/game.c` — `TOWERS[]`, `TowerSpec`, `TowerAttack`, and
  `tower_attack()` all deleted; replaced with a small `spec(type)` helper
  that returns `&tower_config_get()->towers[type]`. The resource-tower
  income calculation generalized to "any tower's `level.income`" — no
  behavior change since only `RESOURCE` has nonzero income today, but it
  means a new income-producing tower can be added entirely in the config.
- `Makefile` — new generation rule, `-Ibuild` added to include path,
  `tower_config.c` added to game sources and to each test binary.

## Tests

`make test` — 4 binaries, 203 assertions total, 0 failures. The new parser
test was observed failing under a mutation (changing `cost 30` to `cost 31`
in the data file) before being committed. The existing 158 game/render/
pathing assertions all still pass with byte-identical default config,
confirming the wiring is value-preserving.

## Not in scope (intentionally)

- Creep upgrades (`init_creep_upgrades` in `game.c`) are still hardcoded —
  they're a separate system per game-design §7 and weren't part of the
  request.
- Base income (`+20`/turn) and starting resources stay hardcoded — they're
  game-rules constants, not tower stats.
- Max level is still `TOWER_MAX_LEVELS = 2` (a compile-time constant).
  Towers always have exactly two levels in the catalog. Raising the cap
  would mean making `levelN.*` accept N up to the new cap and adjusting
  `game_upgrade_selected` accordingly.
