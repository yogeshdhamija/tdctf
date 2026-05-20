# Map extracted to a config file

Tower stats (20260520003/004) and creep upgrades (20260520008/009) already
live in `data/*.cfg` and are parsed at startup into runtime catalogs. The
map — grid size, zones, spawn/receptacle/flag locations — was the last
hardcoded chunk in `game.c::game_init_state`. This change extracts it into
the same data-driven shape, with a dot-matrix monospace layout.

## New cfg file: `data/map.cfg`

Single section: a `grid` keyword followed by H lines of W characters each.
Cell symbols:

```
.   neutral zone
R   red zone
B   blue zone
X   debris (impassable)
1   red spawn point      (zone: neutral)
2   blue spawn point     (zone: neutral)
[   red receptacle       (zone: red)
]   blue receptacle      (zone: blue)
r   red flag start       (zone: red)
b   blue flag start      (zone: blue)
```

The shipped default is the same 30x20 board game.c used to build inline:
RED zone in x<10, BLUE zone in x≥20, neutral in between, with the six
landmarks at the exact coordinates the existing tests reference.

`#` is reserved for comments (line + trailing). Debris uses `X` instead so
the comment glyph doesn't collide with a cell symbol.

## New module: `src/game/map_config.{c,h}`

Parallels `tower_config` / `creep_config`:

- `MapConfig` — `{ width, height, zones[MAP_MAX_W][MAP_MAX_H],
   red_spawn_x/y, blue_spawn_x/y, red_recep_x/y, blue_recep_x/y,
   red_flag_x/y, blue_flag_x/y }`.
- `MapZone` enum (NEUTRAL/RED/BLUE/DEBRIS) — independent of game.h's
  `ZoneType` so the config module stays a pure-data parser with no game
  layer dependency. `game.c` maps between them.
- `map_config_get` / `_load_default` / `_load_from_string`.

The parser reads the optional preamble (comments + blank lines) until it
hits `grid`, then consumes subsequent non-blank lines as rows. Width is
fixed from the first row; every later row must match. Landmark uniqueness
is enforced (duplicate or missing → -1).

The Makefile generates `build/map_config_data.h` from `data/map.cfg`
exactly the same way `tower_config_data.h` and `creep_config_data.h` are
generated.

## Game changes

- `game_init_state` reads grid size, zones, and the six landmark
  coordinates from `map_config_get()` instead of hardcoding them. Cell
  zones go through a small `to_zone_type()` translator so the MapZone →
  ZoneType mapping stays explicit (and the two enums are free to drift
  independently).
- All four `game_init*` variants additionally call
  `map_config_load_default()`. There's no `game_init_with_map_config`
  yet; no test needed a custom map.

## Tests

- `tests/test_map_config.c` — 42 assertions, 9 tests: basic parse,
  debris, comments/blank-line tolerance, missing-landmark error,
  duplicate-landmark error, inconsistent-row-width error,
  unknown-symbol error, no-`grid`-keyword error, and a default-loads
  test that pins the shipped 30x20 coordinates. The four error tests
  were observed failing under inversion (assertion flipped from `!= 0`
  to `== 0`) before being kept.
- All five pre-existing test binaries (`test_pathing`, `test_game`,
  `test_render`, `test_tower_config`, `test_creep_config`) pass unchanged
  — the default map preserves the exact landmark coordinates the
  fixtures assume.

`make clean test`: 6 binaries, 297 assertions, 0 failures.
`make`: WASM build produces `build/index.html` cleanly.
