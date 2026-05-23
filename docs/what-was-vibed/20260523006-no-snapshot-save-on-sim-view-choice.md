# No snapshot save when picking sim viewer

Clicking "View RED sim" / "View BLUE sim" was pushing a new URL snapshot
(history entry). That's wrong: choosing whose perspective to watch
during the simulation isn't a new planning state worth a history entry,
and the snapshot it pushed was indistinguishable from the lock-in that
just happened.

Removed `push_current_snapshot()` from the `BTN_START_SIM_AS_RED` and
`BTN_START_SIM_AS_BLUE` cases in `src/platform/platform_web.c::on_click`.
Lock-in and restart still push, so the URL still anchors all the
meaningful state transitions.
