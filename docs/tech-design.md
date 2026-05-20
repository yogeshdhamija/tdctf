## 1. Tech Stack & Build Pipeline

| Layer | Choice |
|---|---|
| Language | ANSI C (C99) |
| Compile target | WebAssembly via Emscripten (`emcc`) |
| Rendering | HTML5 `<canvas>` 2D context, called from C via Emscripten's SDL2 or raw JS interop |
| Input | Mouse clicks on canvas — grid cell selection and UI button regions |
| UI | Rendered entirely on canvas — text and rectangles in a sidebar region to the right of the grid |
| Build | Single `Makefile` — `emcc` to produce `.wasm` + `.js` glue + `index.html` shell. Pre-steps convert `data/towers.cfg` and `data/creep_upgrades.cfg` into C string literal headers (`build/tower_config_data.h`, `build/creep_config_data.h`) embedded into the binary. |

**No external game libraries.** SDL2 only if needed for canvas abstraction; otherwise raw `emscripten.h` calls to a thin JS rendering layer. **No HTML/CSS UI** — everything is drawn on the canvas.

---

## 2. Architecture

The C code is organized into three layers with strict dependency direction: **Platform → Render → Game**. The game layer is pure C with no platform dependencies and can be compiled and tested natively with `gcc`. The render layer knows *what* to draw (iterates game state, calls abstract drawing primitives) but not *how*. The platform layer implements the drawing primitives, input bridging, and main loop for a specific target (Emscripten/web for MVP).

```
┌──────────────────────────────────────────────────────────┐
│                        Browser                            │
│  ┌──────────────────────────────────────────────────────┐ │
│  │                    <canvas>                          │ │
│  │  ┌──────────────────────────┬───────────────────┐   │ │
│  │  │       Grid Area          │   UI Sidebar      │   │ │
│  │  │  (towers, creeps, flags) │  (text + buttons)  │   │ │
│  │  └──────────────────────────┴───────────────────┘   │ │
│  └──────────────────────┬───────────────────────────────┘ │
│                         ▼                                  │
│  ┌──────────────────────────────────────────────────────┐ │
│  │  PLATFORM layer (platform_web.c)                     │ │
│  │  - Emscripten main loop & frame callback             │ │
│  │  - Implements draw API (canvas via JS interop)       │ │
│  │  - Input: mouse/click → grid coords or UI hit zones  │ │
│  ├──────────────────────────────────────────────────────┤ │
│  │  RENDER layer (render.c)                             │ │
│  │  - Iterates game state each frame                    │ │
│  │  - Draws grid AND UI sidebar using same primitives   │ │
│  │  - Calls: draw_rect, draw_text, draw_circle, etc.   │ │
│  │  - Portable: no platform includes                    │ │
│  ├──────────────────────────────────────────────────────┤ │
│  │  GAME layer (game.c, thing.c, grid.c)                │ │
│  │  - Pure game logic, zero platform deps               │ │
│  │  - State: grid[][], things[], flags, players         │ │
│  │  - Systems: pathfinding, combat, targeting,          │ │
│  │    spawning, resources, build/upgrade, phases        │ │
│  └──────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

### Layer Boundaries

**Game layer** — pure C, no `#include` of platform or render headers. Compilable with `gcc` for native testing.

| Module | Responsibility |
|---|---|
| `game.c/h` | Game state struct, init, phase transitions, turn loop, resources, flags, creep upgrades |
| `thing.c/h` | Tagged-union Thing type, creation, tower/creep logic, combat, spawning, movement |
| `grid.c/h` | Grid representation, zones, BFS pathfinding |
| `tower_config.c/h` | Parses the embedded `data/towers.cfg` text into a `TowerCatalog`. Tower **types** are not hardcoded — towers are declared in the config file and assigned runtime integer ids in declaration order. Each level is its own section and fully redefines the tower's stats at that level (cost, hp, build_turns, dmg, range, aoe, slow, cooldown, income); any combination of stats is permitted, so a single tower can damage, AoE, slow, and generate income simultaneously. `game.c` looks up everything via `tower_config_get()`. The render layer iterates `game_tower_count()` to build the placement palette dynamically. |
| `creep_config.c/h` | Parses the embedded `data/creep_upgrades.cfg` text into a `CreepCatalog` with two sections: `creep <ID>` defines a creep type (code, hp, can_carry_flag, melee_damage); `upgrade <ID>` defines a researchable upgrade that spawns N creeps of a referenced type each turn via `spawn <CREEP_ID> <N>`. Behaviors compose freely — a single creep type can both carry the flag AND damage adjacent enemy towers. Per-player upgrade runtime state (`purchased`/`completed`/`turns_remaining`) lives on `Player` in `game.h`, indexed parallel to the catalog's upgrades. `game.c` exposes `game_creep_type_*` and `game_creep_upgrade_*` accessors for the render layer; the render layer iterates `game_creep_type_count()` to build the per-cell crowding badge dynamically using each type's `code` glyph. |

**Render layer** — includes game headers to read state. Calls abstract drawing primitives declared in `platform.h`. No platform-specific code.

| Module | Responsibility |
|---|---|
| `render.c/h` | Iterates game state, calls draw primitives (rect, text, circle, line) to produce each frame |

**Platform layer** — implements the drawing primitives and bridges input/lifecycle to the game layer. MVP target is Emscripten/web.

| Module | Responsibility |
|---|---|
| `platform.h` | Abstract draw API + input/lifecycle function declarations |
| `platform_web.c` | Emscripten implementation: canvas drawing via JS interop, mouse input, main loop |

---
