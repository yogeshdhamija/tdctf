# Pin a map fixture for the zone-restrictions test

After 20260520012 extracted the map into `data/map.cfg`, behavior tests in
`test_game.c` were still implicitly coupled to whatever map happens to
ship. `test_placement_zone_restrictions` clicks (15, 10) and asserts the
cell is a placeable NEUTRAL zone — true under the original 30x20 layout,
but broken by a funky map with `XX` debris columns at x=14,15.

This is the same coupling pattern 20260520005 solved for tower configs:
pin a frozen fixture so the behavior test stops caring about the shipped
file.

## How

- New `TEST_MAP_CFG` in `tests/test_fixtures.h` — the standard 30x20
  layout with the landmark coords the suite already hard-codes
  (RED spawn 10,8; BLUE spawn 19,11; receptacles 4,4 / 25,15; flags
  4,15 / 25,4) and a clean neutral strip from x=10..19.
- New `game_init_with_configs_and_map(tower_cfg, creep_cfg, map_cfg)`
  on `game.h`. Mirrors the existing `game_init_with_configs` but also
  routes through `map_config_load_from_string` instead of
  `_load_default`.
- `test_placement_zone_restrictions` switches to the new entrypoint.
  Other tests in `test_game.c` / `test_render.c` keep using
  `game_init_with_configs`; they pass under the current funky map
  because their assertions only touch cells outside the debris band.
  They can be migrated to `TEST_MAP_CFG` incrementally if the shipped
  map drifts further.

## Verified

Before: `build/test_game` failed at
`tests/test_game.c:141: tower_id_at(15, 10) >= 0` under the current
`data/map.cfg` (funky map, XX at x=14,15).
After: `make test` — 6 binaries, 287 assertions, 0 failures. The fix
test no longer touches `data/map.cfg`; the user can edit map.cfg freely
without breaking it.
