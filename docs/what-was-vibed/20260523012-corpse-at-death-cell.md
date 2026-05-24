# Corpse renders at the actual death cell

## Bug

In the sim, three different things-at-death disagreed by one cell:

- Tower beam target (`last_target_x/y`): the cell the creep was on *after*
  this tick's move — i.e. its real death cell.
- Dropped flag (set in the resolve-deaths block from `t->x`/`t->y`): same
  — real death cell.
- Rendered corpse (read from `views[viewer].creep_mem[i]`): the cell the
  creep was on *before* this tick's move.

So beams and dropped flags appeared one cell ahead of the corpse along
the creep's path of travel.

## Cause

`sim_one_tick` ran in this order:

1. Spawn → move → pickup → win check
2. Tower attacks, creep melee
3. Resolve deaths (flips `alive=0`, `tag=THING_NONE`)
4. `fog_refresh_all()`

`fog_refresh`'s creep-memory loop skips `!t->alive`, so by the time it
ran, the dying creep had already been flipped to dead and its slot was
not refreshed. `creep_mem` therefore retained whatever was written at
the *previous* tick's end-of-tick refresh — the pre-move cell.

## Fix

Moved the `fog_refresh_all()` call from after `Resolve deaths` to right
before it in `src/game/game.c::sim_one_tick`. At that point a
lethally-hit creep still has `alive==1` and `tag==THING_CREEP`, so the
existing creep_mem loop records it at its post-move (= death) cell.

This also addresses the subtler "creep moves into a newly-visible cell
and dies there" case: because the refresh now happens after the move,
`vis_now` reflects the viewer's current alive units at their *new*
positions, so any cell newly illuminated by a scout this tick is in
`vis_now` when the corpse is recorded.

The post-death `vis_now` / tower-memory state is one tick stale for
~133ms until the next tick refreshes. Two traces confirm this is
cosmetically harmless:

- The world is static between sim ticks (no movement, no combat
  outside `sim_one_tick`), so the user sees the same view they would
  have seen the instant before the deaths registered.
- A dying tower's own cell still renders correctly: `resolve_deaths`
  sets `grid[x][y].thing_id = -1`, so the renderer's `has_live` check
  fails and it falls into `"visible, no tower live → nothing"` at
  `render.c:189` instead of consulting the (now-stale) tower memory.

## Test

`tests/test_game.c::test_corpse_at_death_cell` reuses the corridor +
gunner setup from `test_flag_drop_on_death` (the gunner kills the
carrier one tick after pickup, so the carrier dies one cell beyond the
flag's home with the flag in hand). After the sim, the test asserts:

- The flag was dropped mid-sim (`!at_home && carried_by == -1`).
- For the BLUE creep_mem entry in RED's view, `(cm->x, cm->y)` equals
  the dropped flag's `(x, y)`.

Confirmed failing before the fix (it caught the y-axis off-by-one in
the south-bound path that pathing wobble happens to pick on this
seed) and passing after.
