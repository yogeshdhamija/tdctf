# Selected-tower status shows build/upgrade turns remaining

The creep-upgrade sidebar tiles show `desc  Nt` while researching, but the
selected-tower status line only showed `BUILDING` — no countdown — while a
tower was mid-build or mid-upgrade. The player had to guess how many more
turns until it came online. Now the status line reads `HP h/m  BUILDING Nt`,
mirroring the creep-upgrade format.

## Change

`src/render/render.c` — the selected-tower status snprintf branched on
`build_turns > 0` to pick `"BUILDING"` vs `"READY"`. Split into two
snprintfs so the building branch can append ` %dt` from `t->tower.build_turns`.
Build and upgrade share this field, so a single edit covers both cases.

## Test

`tests/test_render.c::test_selected_tower_shows_build_turns` — place a
SLAMMER (`build_turns=2` in the fixture); placement auto-selects the new
tower. Render and scan the captured `plat_draw_text` calls for a string
containing both `BUILDING` and `2t`. Observed failing before the render.c
edit (only `BUILDING` was emitted) and passing after.
