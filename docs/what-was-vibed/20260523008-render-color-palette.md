# Render color palette

Extracted every inline hex color in `src/render/render.c` into a new
`src/render/palette.h` of `#define`d 0xRRGGBB constants with names
that describe what each pixel represents.

## Naming

Per-player entity colors collapse to four constants (chosen via
mini-question to the user instead of the literal per-entity example):

```
RED_LIVE              0xCC4444   // live red entity in viewer's vision
RED_FOG_OF_WAR        0x661818   // fog-of-war red; also heavy-creep / corpse shade
BLUE_LIVE             0x4477CC
BLUE_FOG_OF_WAR       0x182455
```

`player_color()` / `player_color_dim()` are kept as the
`PlayerID → constant` dispatchers; their bodies now return the named
constants instead of literals.

Everything else gets a use-site-specific constant: canvas/sidebar
chrome, zone backgrounds, grid line, HP bar, tower beam, slow halo,
selection highlight, crowding-badge bg, placement banner, button
states, sidebar text shades, phase-label highlights, current-player
sidebar highlight, upgrade-card states, status banner.

## Mechanics

- `tests/test_render.c` already asserted `badge->color == 0x4477CC`;
  flipped it to `== BLUE_LIVE` and added a `palette.h` include.
- Makefile: added `src/render/palette.h` to the `test_render`
  dependency list. The web build picks it up transitively through
  `render.c`'s include — no Makefile change needed for the wasm build.
- `grep -E '0x[0-9a-fA-F]{6}' src/render/render.c` now returns nothing.

No behavior change. `make clean && make test` → 621 assertions,
0 failures across all 7 binaries.
