# Tech design doc: refresh + compress

## Motivation
`docs/tech-design.md` had drifted out of date:

- Referenced `thing.c/h` and `grid.c/h` modules that don't exist —
  `Thing`/`Cell` structs live in `game.h`, BFS lives in `game.c`.
- No mention of `map_config.{c,h}` (added later).
- Heavy ASCII diagram + repeated explanations of catalog semantics
  already documented in the per-module header comments.
- Missing several patterns an AI reader needs to know on first contact:
  the single-array "Thing" tagged-union approach, the cfg→header build
  pipeline, the fixed-timestep frame loop, click routing, and the
  fixture-config testing pattern.

## What I did
Rewrote `docs/tech-design.md` as a map for an AI reader walking in cold.
Structure:

1. Stack table (one-liner per layer choice)
2. Strict layer separation (game ← render ← platform) and why
3. Accurate module map — every current source file with one-line role
4. **Fat array of things** — the central pattern: one `Thing` tagged
   union, `grid[][]` stores indices, free slots reused via `THING_NONE`.
   Explicit "how to add a new entity kind" guidance.
5. Catalogs — cfg files map to runtime indices; static spec stays in
   catalog, mutable state moves to `Player`.
6. Build pipeline — `sed` recipe turns cfg files into embedded string
   literals; `-s SINGLE_FILE=1` for the self-contained HTML output.
7. Frame loop — 60Hz logic, `SIM_FRAMES_PER_TICK`, accumulator clamp,
   phase machine.
8. Input routing — click → grid coord OR `render_button_at` → game call;
   palette buttons use `BASE + idx` ranges so they resize with catalogs.
   Mobile viewport behavior also called out.
9. Tests — table of binaries, fixture-config pattern, render tests
   against the button table.

## What I cut
- The big ASCII canvas diagram (the module map conveys the same info).
- Detailed re-explanations of tower/creep catalog semantics that already
  live in the headers' top-of-file comments.
- Per-stat lists for towers / creeps (those belong in `game-design.md`
  or the cfg comments).

## Why this shape
The doc's job is to let a future AI find the right file fast and reuse
existing patterns rather than invent new ones. The biggest invitations
to drift are "add another array for X" or "create a Manager class" —
the "Fat array of things" section is the explicit warning against both.

## Follow-up: Memory section added
After the initial refresh, audited every dynamic allocation call in
`src/` and `tests/` — there are **zero**. Project deliberately uses
no heap. Added a **Memory** section to `tech-design.md` that:

1. States the no-heap invariant explicitly (so a future AI doesn't
   reach for `malloc` reflexively).
2. Tabulates the live footprint by static global, measured by compiling
   a one-off `printf("%zu", sizeof(...))` probe against the real headers.
   Total mutable static state is ~58 KB; embedded cfg `.rodata` is ~7 KB.
3. Captures the patterns: compile-time `MAX_*` caps, file-static scratch
   buffers (BFS) instead of stack, single `GameState s` in `game.c`,
   per-module catalog globals, `THING_NONE` slot reuse instead of free.
4. Calls out the non-reentrancy of BFS as a direct consequence of the
   file-static scratch choice — so anyone tempted to call BFS from
   inside BFS gets the warning up front.
