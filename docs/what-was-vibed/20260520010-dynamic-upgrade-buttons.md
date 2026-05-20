# Creep-upgrade buttons are now dynamic

After 20260520009 made creep types and upgrades fully data-driven, the
*click dispatch* for upgrade buttons was still wired to four fixed enum
slots — `BTN_BUY_UPGRADE_0..3` in `render.h`, with a 4-case switch in
`platform_web.c`. Adding a fifth upgrade to `data/creep_upgrades.cfg`
rendered the button correctly, but clicking it did nothing.

This change replaces the four fixed slots with a dynamic base, mirroring
the tower-placement palette:

- `render.h`: `BTN_BUY_UPGRADE_BASE = 500` (with `BTN_PLACE_TOWER_BASE`
  at 1000); the four `BTN_BUY_UPGRADE_0..3` enumerators are gone.
- `render.c`: buttons are emitted as `BTN_BUY_UPGRADE_BASE + i`.
- `platform_web.c`: any button id `>= BTN_BUY_UPGRADE_BASE` dispatches
  to `game_buy_creep_upgrade(id - BTN_BUY_UPGRADE_BASE)`; the explicit
  switch cases are gone.

## Tests

`test_planning_buttons_visible` previously hard-coded `present(...0..3)`
checks, so a 5th upgrade slipping through the dispatcher wouldn't have
been caught at this layer. Rewrote it to iterate
`game_creep_upgrade_count()`, so any upgrade visible in the catalog must
be hittable in the sidebar. Verified failing under mutation: rendering
only the first four upgrade buttons while the cfg has five trips the
loop's assertion.

`make clean test`: 5 binaries, 253 assertions, 0 failures.
`make`: WASM build clean.
