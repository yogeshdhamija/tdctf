# Creeps rendered as letters, not dots

## Motivation

Creeps were drawn as undifferentiated filled circles (size and dim-vs-bright
distinguished "heavy" melee from "light"). Hard to tell creep types apart at
a glance — you had to read the crowding badge above the cell, which only
appears when 2+ creeps share a cell.

## Change

`src/render/render.c` — live and corpse creep rendering now uses
`plat_draw_text` with the catalog's `code` glyph instead of `plat_fill_circle`
/ `plat_draw_circle`. With the default `data/creep_upgrades.cfg`, swarm = `w`,
attacker = `a`, tank = `t`, scout = `s`.

- Live creep: code letter centered in the cell, player color (dim if heavy).
- Corpse: same letter at the remembered cell, dim color (the FogCreepMemory
  already carries `type`, so the lookup works for out-of-vision creeps too).
- Slow halo bumped from `r+2` (≈7–9) to a fixed `r=9` so it fully encircles
  the letter.
- Flag indicator's pole nudged left by 2 px (from `cx-7` to `cx-9`) so it
  doesn't overlap the letter glyph.

The per-cell crowding badge is unchanged — letters at one position still
overlap when creeps stack, so the badge still does the disambiguation work.

## Tests

Purely visual change (coding-norms §Layered Testing exception). All existing
tests still pass. Render tests assert on the button table and text overlays,
not on circle draws, and the platform draw primitives are stubbed in
`test_render.c`.
