# Towers prioritize the creep closest to the defended flag

Towers used to target whichever in-range enemy creep was closest to the
tower itself (Manhattan distance). That happily ignored the fact that a
creep two cells from the gun but one cell from picking up the flag is
strictly more urgent than a creep one cell from the gun but still three
cells from the flag.

New rule (game.c `sim_one_tick` tower-attack block): among enemy creeps
within range, pick the one with the smallest Manhattan distance to
`s.flags[t->owner]` — the tower owner's own flag, the one being stolen.

A nice side effect of how the carried-flag position works: once a creep
picks up the flag, `s.flags[k].x/y` tracks the carrier's cell, so the
carrier's flag-distance is always 0 and the targeting rule naturally
focuses fire on the carrier. No special case needed.

## Test

`test_tower_targets_creep_closest_to_flag` in `tests/test_game.c`. Sets
up two BLUE retrievers and snaps them onto cells where the two metrics
disagree (one creep nearer the tower, the other nearer the flag), then
asserts the flag-closer creep takes the damage. Slow_ticks=1 freezes
both creeps for the observation tick so positions stay fixed when the
tower picks its target. Observed failing before the game.c change
(tower hit the tower-closer creep) and passing after.

No other tests have multiple in-range targets at once, so no existing
assertions changed.
