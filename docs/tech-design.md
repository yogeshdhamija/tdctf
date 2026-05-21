# Tech Design

Reader guide: an AI walking in cold. This doc is a map — where things live,
what depends on what, what patterns to reuse. Behavior lives in the code
and `game-design.md`; don't duplicate it here.

## Stack

| Layer       | Choice |
|-------------|--------|
| Language    | C99 |
| Web target  | WebAssembly via Emscripten (`emcc`) |
| Rendering   | HTML5 `<canvas>` 2D context, called via `EM_JS` interop |
| UI          | All drawn on the canvas — no HTML/CSS UI |
| Input       | Mouse clicks on canvas; mobile pinch-zoom via viewport meta |
| Tests       | Native `gcc` build of the game + render layers, plain `assert` |
| Output      | Single self-contained `build/index.html` (wasm/js base64-embedded) |

No external game libraries. SDL is not used.

## Layer separation (strict)

```
game  ←  render  ←  platform
```

- **game/** — pure C, no platform/render includes. Compiles with `gcc`
  for native tests. Owns all state and rules.
- **render/** — includes game headers (reads state), calls abstract draw
  primitives declared in `platform.h`. No platform-specific code.
- **platform/** — implements the draw primitives, owns the main loop,
  bridges input. Today only `platform_web.c` (Emscripten); the layer
  exists so a second target wouldn't touch game or render.

Dependency direction is enforced by what each module `#include`s. Don't
breach it — it's what lets the test binaries link without Emscripten.

## Module map

| File | Responsibility |
|---|---|
| `src/game/game.{c,h}`           | `GameState`, the `Thing`/`Cell`/`Player`/`Flag` structs, phase machine, planning actions, per-tick simulation, BFS pathing. The whole game lives here. |
| `src/game/tower_config.{c,h}`   | Parses `data/towers.cfg` into `TowerCatalog`. Towers are declared in the cfg; ids are runtime indices. |
| `src/game/creep_config.{c,h}`   | Parses `data/creep_upgrades.cfg` into `CreepCatalog` (creep types + researchable upgrades). |
| `src/game/map_config.{c,h}`     | Parses `data/map.cfg` (dot-matrix grid) into `MapConfig`. Defines geometry and landmark positions; doesn't depend on game.h. |
| `src/render/render.{c,h}`       | Iterates `GameState`, draws grid + sidebar via `plat_*` primitives. Owns the button hit-table (`render_button_at`). |
| `src/platform/platform.h`       | Abstract draw API + `FrameStats`. |
| `src/platform/platform_web.c`   | `EM_JS` canvas drawing, main loop, mouse → `game_grid_click` / button dispatch, frame stats, viewport sizing for mobile. |
| `src/platform/shell.html`       | HTML shell `emcc` injects the script into. Viewport meta lives here. |

## Data model: fat array of things

There is **one** flat array of game entities:

```c
Thing  things[MAX_THINGS];   // tagged union: THING_TOWER | THING_CREEP | THING_NONE
int    thing_count;
```

Everything that lives on the grid — towers, creeps — is a `Thing` with a
`tag` and a union (`tower {…}` / `creep {…}`). The grid stores **indices**
into this array, not pointers:

```c
Cell grid[MAX_GRID_W][MAX_GRID_H];   // { ZoneType zone; int thing_id; }   // thing_id == -1 means empty
```

Flags are a small fixed-size sibling array (`Flag flags[2]`) and reference
their carrier by `thing_id` too.

**Patterns to keep using when adding things:**

- New entity kind → add a `THING_*` enum + a struct in the union. Don't
  create a parallel array. The whole sim already loops `things[]` once
  per tick; a new kind costs one branch.
- Allocation: `alloc_thing()` reuses `THING_NONE` slots before extending
  `thing_count`. Death sets `tag = THING_NONE`. Indices are stable for the
  lifetime of the slot's current occupant — fine to store across one tick,
  not across deaths.
- "Refer to a thing" always means store the int index, never a `Thing*`.
  Pointers into `things[]` are only safe within a single function call.

This is deliberate: contiguous, cache-friendly, trivially serialisable,
and makes the entire simulation a sequence of straight-through loops over
one array. Don't refactor toward "polymorphism" or per-kind vectors.

## Catalogs (data-driven content)

Towers, creep types, creep upgrades, and the map are **not hardcoded**.
Each lives in a `data/*.cfg` text file with a small parser:

```
data/towers.cfg          → tower_config.c        → TowerCatalog
data/creep_upgrades.cfg  → creep_config.c        → CreepCatalog
data/map.cfg             → map_config.c          → MapConfig
```

`TowerType` / `CreepType` are runtime integer indices into the catalog
arrays. To add a tower or creep type, edit the cfg — no C changes needed
unless you're introducing a new *behavior* (a stat that needs new sim code).

Per-player dynamic state (e.g. an upgrade's `purchased / completed /
turns_remaining`) lives on `Player.creep_upgrades[i]`, indexed parallel
to the catalog. Static spec stays in the catalog; only mutable state moves.

## Memory

**Zero heap allocation.** No `malloc` / `calloc` / `realloc` / `free` /
`alloca` / `strdup` anywhere in `src/` or `tests/`. (Emscripten links a
stub `malloc`; nothing we write calls it.) All mutable state is in
file-static globals sized at compile time from the `MAX_*` macros.

Live footprint (game running, default map):

| Static global               | Where               | Size    |
|----------------------------|---------------------|---------|
| `GameState s`               | `game.c`            | ~31.7 KB (Thing[400] = 22.4 KB; Cell[40×30] = 9.4 KB) |
| BFS scratch (`bfs_dist` + `bfs_qx/qy`) | `game.c` | ~14.4 KB |
| `TowerCatalog g_catalog`    | `tower_config.c`    | ~5.1 KB |
| `g_frame_deltas[360]`       | `platform_web.c`    | ~2.8 KB |
| `g_btns[64]`                | `render.c`          | ~1.3 KB |
| `MapConfig g_config`        | `map_config.c`      | ~1.2 KB |
| `CreepCatalog g_catalog`    | `creep_config.c`    | ~1.1 KB |
| **Total mutable static**    |                     | **~58 KB** |
| Embedded `*_CONFIG_DEFAULT` cfg text (`.rodata`) | `build/*_data.h` | ~7 KB |

The whole game's working set fits comfortably in L1.

**Why this matters / patterns to keep:**

- **Sized at compile time.** `MAX_GRID_W`, `MAX_GRID_H`, `MAX_THINGS`,
  `MAX_BEAMS`, `MAX_CREEP_UPGRADES`, `CREEP_TYPE_MAX_COUNT`,
  `TOWER_MAX_COUNT`, `TOWER_MAX_LEVELS`, etc. The actual map/catalog
  sizes (`grid_w`, `count`, `level_count`, …) are runtime values
  ≤ those maxes. Don't introduce dynamic resizing — bump the cap.
- **Scratch buffers are file-static, not stack.** BFS's parent grid and
  queue would be ~14 KB on the stack each call; lifting them to
  file-static keeps stack shallow and lets BFS be called repeatedly from
  hot paths (`paths_valid` runs it 4× per placement preview).
  Consequence: BFS is **not reentrant**. Don't call `bfs_to_goal` from
  inside another BFS.
- **One `static GameState s;` in `game.c`.** Single source of truth,
  reachable only through `game_get_state()`. No copies, no snapshots —
  the simulation mutates `s` in place each tick.
- **Catalogs are owned by their parser module** (`g_catalog` in
  `tower_config.c` / `creep_config.c`, `g_config` in `map_config.c`).
  `game.c` reads them through accessors, never holds its own pointer.
  Reloading a cfg (test hooks like `game_init_with_configs`) overwrites
  the parser's global in place — no allocation, no aliasing.
- **No frees ever needed.** Deaths flip a `Thing`'s `tag` to
  `THING_NONE`; `alloc_thing()` reuses those slots before extending
  `thing_count`. Phase transitions and `game_init()` use `memset(&s, 0,
  sizeof(s))` to wipe in one shot.
- **No leak surface.** Static lifetimes mean every reachable byte is the
  same byte across the whole program run. There is nothing to track,
  nothing to plug, and the WASM linear memory footprint is essentially
  constant after `main()`.

If you find yourself wanting `malloc`, you're either (a) introducing
unbounded state that should have a cap, or (b) computing something that
should live in a file-static scratch buffer beside its caller. Pick one
and don't reach for the allocator.

## Build pipeline

The default cfg files are **embedded into the binary** at compile time:

```
data/towers.cfg          ─sed→  build/tower_config_data.h     (string literal)
data/creep_upgrades.cfg  ─sed→  build/creep_config_data.h
data/map.cfg             ─sed→  build/map_config_data.h
```

The header generation is one `sed` recipe per file in the Makefile —
each line of the cfg becomes a `"line\n"` C string literal in a
`static const char *_CONFIG_DEFAULT[]`. Both the WASM build and native
tests link the exact same bytes, so behavior matches across targets.

WASM build (`emcc … -s SINGLE_FILE=1 --shell-file shell.html`) produces
one `build/index.html` with the wasm + JS glue base64-embedded. No
server, no separate assets — double-click works.

## Frame loop

Fixed timestep, decoupled render rate:

- `LOGIC_FPS = 60` → `MS_PER_LOGIC_FRAME ≈ 16.67ms` (in `platform.h`).
- Each browser frame, `platform_web.c::frame()` adds `dt` to an
  accumulator and drains it by calling `game_frame()` until the
  accumulator is empty. Lag spikes are clamped to 250ms so the sim never
  tries to catch up by more than ~15 ticks.
- `game_frame()` does nothing during planning phases. During
  `PHASE_SIMULATE` it counts down `SIM_FRAMES_PER_TICK = 8` real frames
  per *sim tick*, runs `sim_one_tick()` (the whole tick — movement,
  combat, deaths), and ends the turn after `SIM_TICKS_PER_TURN = 100`
  ticks or when no creeps remain.

Phases (in `Phase` enum): `PLAN_RED → PLAN_BLUE → SIMULATE → PLAN_RED …`,
plus `GAME_OVER` as a terminal.

## Input routing

Single click handler in `platform_web.c::on_click(px, py)`:

```
px < grid_w * CELL_SIZE?
  yes → game_grid_click(px/CELL_SIZE, py/CELL_SIZE)
  no  → render_button_at(px, py) → switch on ButtonID → call game_*()
```

`render_button_at` returns a `ButtonID` from a flat table the render
layer rebuilds every frame (`btn_push(...)` in `render.c`). Tower-place
and upgrade-buy buttons use ID ranges (`BTN_PLACE_TOWER_BASE + i`,
`BTN_BUY_UPGRADE_BASE + i`) so the palette resizes with its catalog —
no enum to keep in sync.

To add a fixed button: add an entry to `ButtonID` in `render.h`, draw
it with `draw_button(...)` in `render.c`, dispatch it in
`platform_web.c::on_click`.

Mobile: `js_canvas_init` rewrites the viewport meta to
`width=<canvas_w>`, so the canvas fits horizontally on any device and
pinch-zoom Just Works. Single-finger taps synthesise `mousedown` — no
touch-specific code needed.

## Tests

Native binaries built and run by `make test`:

| Binary | Sources |
|---|---|
| `test_pathing`       | `tests/test_pathing.c` + game/ |
| `test_game`          | `tests/test_game.c` + game/  (full sim behaviors) |
| `test_render`        | `tests/test_render.c` + game/ + render/ (button layout, hit-testing — no platform) |
| `test_tower_config`  | `tests/test_tower_config.c` + `tower_config.c` |
| `test_creep_config`  | `tests/test_creep_config.c` + `creep_config.c` |
| `test_map_config`    | `tests/test_map_config.c` + `map_config.c` |

Behavior tests load **fixture** cfg strings (`tests/test_fixtures.h`)
via `game_init_with_configs[_and_map](…)`, not the shipped data, so
gameplay tuning in `data/*.cfg` can't break tests. When you add a stat
or change a behavior, edit the fixture if the test needs the new field.

Render tests don't touch a real canvas — they assert against the button
table that `render_frame` builds. To exercise a new button, render a
frame and call `render_button_at(x, y)`.

See `docs/ai-instructions/coding-norms.md` for "where does this test go"
(layer of abstraction) guidance.
