# Shell page legend + raw cfg links

## Motivation

The game has no in-canvas tutorial and no doc panel — a new player loading
`index.html` can't tell a spawn marker from a receptacle, doesn't know what
the tower codes (`A1`, `B2*`, …) mean, and has no idea where to look up
tower or creep stats. Once the file is sent to a friend with `make build`,
they need that reference in the same artifact.

## Change

Edited `src/platform/shell.html` (the emcc `--shell-file` template that
gets baked into `build/index.html`):

- Page now lays out as a vertical flex column (`flex-direction: column`,
  `align-items: center`) so anything after the canvas falls below it,
  horizontally centered, instead of next to it.
- Added a `<div id="info">` block below the canvas with two sections:
  1. **CONFIG FILES** — links to the raw GitHub text on the `master`
     branch for `data/towers.cfg` and `data/creep_upgrades.cfg`. These
     are the same files the build embeds (so they always reflect what
     the player is actually playing against), and players are expected
     to keep one open in a tab as they plan.
  2. **LEGEND** — a `<table>` of icon → meaning rows. Each icon is a
     small inline SVG drawn with the same colors the canvas renderer
     uses (`render.c`), so swatches are guaranteed to match what the
     player sees on the grid:
     - Flag (triangle on pole), spawn point (double circle), receptacle
       (nested squares), creeps (light/heavy dots), slowed creep (blue
       ring), flag-carrier (creep with stolen-color mini-flag), tower
       label (`A1`, trailing `*` = building), attack beam, tower HP
       bar, build-zone tinting (red/blue/neutral), debris.

## Notes

- Pure shell-template change; no C source touched. All tests still pass
  (`make test`) — no behavior changed at any layer.
- The viewport rewrite in `platform_web.c::js_canvas_init` still sets
  `width=<canvas_w>`, so on mobile the info block wraps to the same
  width as the canvas (its `max-width: 760px` only kicks in on desktop).
- SVG colors duplicate the literals in `render.c` (`0xCC4444`, `0x4477CC`,
  `0xFFEE88`, `0x88CCFF`, `0x440000`/`0x44CC44`, zone tints). If those
  ever change in `render.c`, the swatches here will drift — there is no
  automated link. Considered acceptable: legend is for human reference
  and slight color drift would be cosmetic, not misleading.
