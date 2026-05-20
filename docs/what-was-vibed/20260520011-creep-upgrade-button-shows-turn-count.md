# Creep upgrade buy buttons show research turn count

Tower place buttons render as `[X] Name $cost Yt` — the trailing `Yt`
makes the build cadence visible at a glance. The creep upgrade buy
buttons previously showed only `desc $cost`, hiding the equivalent
research time until the player committed and the tile flipped to the
in-progress state. They now mirror the tower format: `desc $cost Yt`.

## Change

`src/render/render.c` — in the unpurchased branch of the creep-upgrade
list, look up `game_creep_upgrade_research_turns(i)` alongside the cost
and append ` %dt` to the button label. The completed/in-progress
branches were already turn-aware (`%dt` remaining); only the buyable
branch was missing the figure.

## Test

`tests/test_render.c::test_buy_upgrade_button_shows_turn_count` — render
PLAN_RED with the standard fixture, scan captured `plat_draw_text`
calls, and assert that slot 0's text contains `$30` and `1t`, and slot
2's contains `$60` and `2t`. Observed failing before the render.c edit
(no `1t`/`2t` substring) and passing after.
