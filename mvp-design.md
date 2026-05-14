# MVP High-Level Design Document

## Overview

A single-player, hotseat-style Tower Defense Capture the Flag game. One player controls both sides. Built in ANSI C, compiled to WASM, rendered on a browser canvas. All visuals are minimalist: grid lines, letter codes for towers, colored dots for creeps, and plaintext lists for creep upgrades.

---

## 1. Tech Stack & Build Pipeline

| Layer | Choice |
|---|---|
| Language | ANSI C (C99) |
| Compile target | WebAssembly via Emscripten (`emcc`) |
| Rendering | HTML5 `<canvas>` 2D context, called from C via Emscripten's SDL2 or raw JS interop |
| Input | Mouse clicks on canvas — grid cell selection and UI button regions |
| UI | Rendered entirely on canvas — text and rectangles in a sidebar region to the right of the grid |
| Build | Single `Makefile` — `emcc` to produce `.wasm` + `.js` glue + `index.html` shell |

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

## 3. Rendering Specification

Everything is drawn on a single `<canvas>`. The canvas is divided into two regions: the game grid on the left and a UI sidebar on the right. Full visibility — no fog of war.

### Canvas Layout
- **Total canvas size:** `(CELL_SIZE * grid_w + SIDEBAR_W)` x `(CELL_SIZE * grid_h)` — e.g., 960 + 220 = 1180 x 640 pixels.
- **Grid region:** left side, `CELL_SIZE * grid_w` pixels wide.
- **Sidebar region:** right side, `SIDEBAR_W` pixels wide (220px default). Drawn with text and rectangles using the same `plat_draw_*` primitives as the grid.

### Grid
- Grid of square cells, 30x20. Each cell is `CELL_SIZE` pixels (32px default).
- Grid lines: thin gray (`#444`) lines.
- Build zones: subtle background tint per cell — red-tinted, blue-tinted, or neutral (slightly lighter gray).
- Creep path line: dashed white line overlaid on the default path.

### Towers
- Rendered as **text centered in the cell**.
- Format: `{TypeCode}{Level}` — e.g., `G1` (Gunner level 1), `G2` (Gunner level 2).
- Color: player color (red/blue).
- Under construction: rendered with a `*` suffix and dimmed, e.g., `G1*`.
- Tower type codes:

| Code | Tower | Notes |
|---|---|---|
| `G` | Gunner | Basic single-target damage |
| `S` | Slammer | AoE damage + slow effect |
| `R` | Resource | Generates income per turn |

### Creeps
- Rendered as **filled circles** (radius ~4px) in player color.
- Different creep types use different shades/brightness of the player's color:
  - Retriever: solid player color
  - Siege: darker shade
- No HP bar or visual health indicator. Creeps have numeric HP reduced by tower damage.

### Flag
- Rendered as a **triangle** (pennant shape) in the owning player's color.

---

## 4. UI Sidebar (on canvas)

The sidebar is rendered entirely on the canvas using `plat_draw_text()` and `plat_fill_rect()`. Clickable elements are rectangular hit zones — the platform layer translates mouse clicks in the sidebar region into game actions by checking pixel coordinates against known button bounds.

```
┌─────────────────────────────┐
│  TURN 7        [Lock In]    │
│  Phase: PLANNING             │
├─────────────────────────────┤
│  Player RED                  │
│  Resources: 150              │
│  Income: +20/turn            │
│                              │
│  [Switch to BLUE]            │
├─────────────────────────────┤
│  TOWERS  (click grid to      │
│          place)              │
│  [G] Gunner      (30)       │
│  [S] Slammer     (50)       │
│  [R] Resource    (80)       │
├─────────────────────────────┤
│  CREEP UPGRADES             │
│  * Retriever (default)      │
│  [Buy] +2 Siege (40) 1t     │
│  [Buy] +1 Retriever (30)    │
├─────────────────────────────┤
│  SELECTED TOWER: G1 @ (5,3) │
│  HP: 100/100                 │
│  [Upgrade] to Lv2 (30)      │
│  [Destroy]                   │
└─────────────────────────────┘
```

Buttons are rendered as filled rectangles with text. The render layer defines the layout; the platform layer stores button bounds from the last frame and uses them for hit-testing clicks.

---

## 5. Game Loop & Phase Flow

```
GAME START
  │
  ▼
┌──────────────────────┐
│   PLANNING PHASE     │◄──────────────────────┐
│   (Player RED)       │                        │
│   - place/upgrade    │                        │
│   - buy creep upgrades│                        │
│   [Lock In] ─────────┤                        │
│                      │                        │
│   PLANNING PHASE     │                        │
│   (Player BLUE)      │                        │
│   - place/upgrade    │                        │
│   - buy creep upgrades│                        │
│   [Lock In] ─────────┤                        │
└──────────┬───────────┘                        │
           ▼                                    │
┌──────────────────────┐                        │
│   RESOLVE CONFLICTS  │                        │
│   - neutral zone:    │                        │
│     if both players  │                        │
│     placed on same   │                        │
│     cell, destroy    │                        │
│     both towers,     │                        │
│     cell becomes     │                        │
│     impassable debris│                        │
└──────────┬───────────┘                        │
           ▼                                    │
┌──────────────────────┐                        │
│   SIMULATION PHASE   │                        │
│   - spawn creeps     │                        │
│   - step-by-step sim │                        │
│   - animate on canvas│                        │
│   - resolve combat   │                        │
│   - check flag state │                        │
└──────────┬───────────┘                        │
           ▼                                    │
      Flag captured                             │
      & returned?                               │
       /        \                               │
     YES         NO ────────────────────────────┘
      │
      ▼
  GAME OVER
```

### Hotseat Model (MVP)
Since this is single-player controlling both sides:
- RED plans first, then BLUE plans.
- Full visibility — both players can see the entire board at all times.
- Then simulation runs and both sides watch.
- No timer enforcement needed in MVP (optional).

---

## 6. Core Data Structures (C)

All game entities (towers and creeps) are stored in a single flat array of tagged-union "Things". Grid cells, flags, and targeting all reference Things by index into this array. This simplifies cross-referencing and reduces the number of parallel arrays to manage.

```c
#define MAX_GRID_W 40
#define MAX_GRID_H 30
#define MAX_THINGS 400
#define MAX_CREEP_UPGRADES 8

typedef enum { PLAYER_RED = 0, PLAYER_BLUE = 1 } PlayerID;
typedef enum { PHASE_PLAN_RED, PHASE_PLAN_BLUE, PHASE_SIMULATE, PHASE_GAME_OVER } Phase;

typedef enum { THING_NONE = 0, THING_TOWER, THING_CREEP } ThingType;

typedef enum {
    TOWER_GUNNER,    // G — single-target damage
    TOWER_SLAMMER,   // S — AoE damage + slow
    TOWER_RESOURCE   // R — income generation
} TowerType;

typedef enum {
    CREEP_RETRIEVER,
    CREEP_SIEGE
} CreepType;

typedef enum { ZONE_NEUTRAL, ZONE_RED, ZONE_BLUE } ZoneType;

// Tagged union: every game entity is a Thing
typedef struct {
    ThingType tag;
    PlayerID  owner;
    int       x, y;        // grid cell position
    int       hp, max_hp;
    int       alive;
    union {
        struct {
            TowerType type;
            int       level;       // 1-2
            int       build_turns; // >0 means under construction
        } tower;
        struct {
            CreepType type;
            int       has_flag;
            int       path_progress; // furthest point on path reached
            int       slow_ticks;    // remaining ticks of slow effect (0 = normal)
        } creep;
    };
} Thing;

typedef struct {
    ZoneType zone;
    int thing_id;          // index into things[]; -1 if no tower
} Cell;

typedef struct {
    int   id;
    int   cost;
    int   research_turns;  // turns to complete after purchase
    int   turns_remaining; // 0 = complete; >0 = researching
    int   purchased;
    int   completed;
    char  description[64];
} CreepUpgrade;

typedef struct {
    int           resources;
    int           income_per_turn;
    CreepUpgrade  creep_upgrades[MAX_CREEP_UPGRADES];
    int           creep_upgrade_count;
} Player;

typedef struct {
    int  x, y;
    PlayerID owner;
    int  carried_by;  // thing index (creep), or -1
    int  at_home;     // still on receptacle
} Flag;

typedef struct {
    Cell    grid[MAX_GRID_W][MAX_GRID_H];
    int     grid_w, grid_h;
    Thing   things[MAX_THINGS];
    int     thing_count;
    Player  players[2];
    Flag    flags[2];
    Phase   phase;
    int     turn;
    // Receptacle positions
    int     receptacle_x[2], receptacle_y[2];
    // Default creep path waypoints per player
    int     path_x[2][64], path_y[2][64];
    int     path_len[2];
} GameState;
```

---

## 7. Key Systems — MVP Scope

### 7.1 Pathfinding
- **Algorithm:** BFS on the grid from creep's current position.
- **Target:** Nearest cell on the pre-defined path line that is *ahead of* the creep's furthest visited path point. If carrying flag, target is own receptacle.
- **Recompute:** When any tower is placed/destroyed that changes walkability.
- **Blocked path rule:** Towers cannot be placed if it would make the path completely impassable (validate before placement).

### 7.2 Combat (Simulation Phase)
Simulation runs in discrete **ticks** (e.g. 30 ticks per turn). Each tick:
1. Move all creeps one step along their path.
   - **Speed:** Base move rate is 1 cell per tick.
   - **Slow:** Creeps with `slow_ticks > 0` skip their move and decrement `slow_ticks` by 1 instead.
2. Each tower selects a target within range and deals damage.
   - Gunner: single target, closest creep in range.
   - Slammer: AoE damage centered on closest target. All creeps hit receive `slow_ticks = 2`.
   - Resource: no attack.
3. Siege creeps attack the first tower in their adjacent cells.
4. Resolve deaths (creeps and towers).
5. Check flag pickup/drop.

### 7.3 Flag Mechanics
- Flag starts at a fixed position on each player's side.
- Retriever creep entering the flag cell picks it up.
- If carrier dies, flag drops at that cell.
- If carrier reaches own receptacle, **game over — that player wins**.

### 7.4 Resource System
- Starting resources: 100.
- Base income: +20/turn.
- Resource tower: +10/turn each (after build completes).
- All costs are immediate; build/research time is separate.

---

## 8. Simulation Visualization

During the simulation phase, the canvas animates tick-by-tick at a fixed speed:
- Creeps snap to grid cells each tick (no sub-cell interpolation).
- Tower attacks: brief line drawn from tower to target (flash for ~100ms).
- No damage numbers.
- No playback controls — simulation runs at fixed 1x speed.
- At end of simulation: hold final state for 2 seconds, then transition to planning.

---

## 9. MVP Creep Upgrades (per player)

Flat list of independent purchases — no dependencies, no tree. Each upgrade can be bought once. Research turns must elapse before the upgrade takes effect.

| Upgrade | Cost | Research Turns | Effect |
|---|---|---|---|
| +1 Retriever | 30 | 1 | Spawn 1 additional retriever per turn |
| +2 Siege Creeps | 40 | 1 | Spawn 2 siege creeps per turn |
| +2 Retrievers | 60 | 2 | Spawn 2 additional retrievers per turn |
| +2 Siege Creeps | 70 | 2 | Spawn 2 additional siege creeps per turn |

Base: 1 retriever spawns per turn for free.

---

## 10. MVP Tower Upgrades

Each tower has one upgrade (levels 1–2). Upgrade cost equals the tower's base cost. Each upgrade takes 1 turn to build.

| Tower | Lv1 (base) | Lv2 (upgrade) |
|---|---|---|
| Gunner (G) | 10 dmg, 3 range | +5 dmg, +1 range |
| Slammer (S) | 5 dmg AoE r=1, 2 range, slow | +3 dmg, AoE r=2 |
| Resource (R) | +10/turn, 3 build turns | +10/turn |

---

## 11. MVP Map

One hardcoded map to start.

```
 Legend:  R = Red receptacle    B = Blue receptacle
         F = Red flag           f = Blue flag
         ~ = Red-only zone      . = Neutral zone     # = Blue-only zone

    0         1         2
    0123456789012345678901234567890
 0  ~~~~~~~~~~..........##########
 1  ~~~~~~~~~~..........##########
 2  ~~~~~~~~~~..........##########
 3  ~~~~~~~~~~..........##########
 4  ~~~~R~~~~~..........#####f####
 5  ~~~~~~~~~~..........##########
 6  ~~~~~~~~~~..........##########
 7  ~~~~~~~~~~..........##########
 8  ~~~~~~~~~~..........##########
 9  ~~~~~~~~~~..........##########
10  ~~~~~~~~~~..........##########
11  ~~~~~~~~~~..........##########
12  ~~~~~~~~~~..........##########
13  ~~~~~~~~~~..........##########
14  ~~~~~~~~~~..........##########
15  ~~~~F~~~~~..........#####B####
16  ~~~~~~~~~~..........##########
17  ~~~~~~~~~~..........##########
18  ~~~~~~~~~~..........##########
19  ~~~~~~~~~~..........##########

Default path (Red creeps → Blue flag):
  Row 4: (0,4) → (29,4)  (straight horizontal)

Default path (Blue creeps → Red flag):
  Row 15: (29,15) → (0,15) (straight horizontal)
```

Grid: 30 wide x 20 tall. Symmetrical, simple. Red attacks along row 4 toward Blue's flag. Blue attacks along row 15 toward Red's flag. Neutral zone in the middle is contested — both players can build there.

---

## 12. Layer Interfaces

### Game API (called by platform layer)
```c
// Lifecycle
void     game_init(void);
void     game_sim_tick(void);                  // advance simulation one tick

// Player actions (called during planning phase)
void     game_click(int grid_x, int grid_y);   // select cell
void     game_place_tower(int type);            // place at selected cell
void     game_upgrade_tower(int thing_idx);
void     game_destroy_tower(int thing_idx);
void     game_buy_creep_upgrade(int upgrade_id);
void     game_lock_in(void);                    // end planning phase

// State queries (called by render layer)
const GameState* game_get_state(void);          // read-only access to full state
```

### Draw API (declared in platform.h, implemented per-platform)
```c
// Abstract drawing primitives — render.c calls these, platform implements them
void     plat_clear(uint32_t color);
void     plat_draw_rect(int x, int y, int w, int h, uint32_t color);
void     plat_fill_rect(int x, int y, int w, int h, uint32_t color);
void     plat_draw_circle(int cx, int cy, int r, uint32_t color);
void     plat_fill_circle(int cx, int cy, int r, uint32_t color);
void     plat_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void     plat_draw_text(int x, int y, const char* text, uint32_t color);
void     plat_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);
```

### Platform → Render → Game call flow
```
platform_web.c: emscripten main loop callback each frame:
  1. Handle any queued input → translate click coords to grid cell or UI button hit,
     then call game_click(), game_place_tower(), etc.
  2. If phase == SIMULATE: call game_sim_tick()
  3. Call render_frame(game_get_state()) — render.c draws grid AND sidebar, calls plat_draw_*()
```

---

## 13. File Structure

```
tdctf/
├── design.md              # Original game design
├── mvp-design.md          # This document
├── Makefile               # emcc build (+ native test build target)
├── src/
│   ├── game/              # GAME LAYER — pure C, no platform deps
│   │   ├── game.c/h       #   State, init, phases, resources, flags, skill tree
│   │   ├── thing.c/h      #   Tagged-union Thing, tower/creep logic, combat, spawning
│   │   └── grid.c/h       #   Grid, zones, BFS pathfinding
│   ├── render/            # RENDER LAYER — reads game state, calls draw primitives
│   │   └── render.c/h     #   Frame rendering: grid, things, flags, effects
│   └── platform/          # PLATFORM LAYER — target-specific implementations
│       ├── platform.h     #   Abstract draw API + lifecycle declarations
│       └── platform_web.c #   Emscripten: canvas via JS interop, input, main loop
├── web/
│   └── index.html         # Minimal shell page — just a <canvas> element
└── build/                 # emcc output (.wasm, .js glue)
```

---

## 14. What's Cut from MVP

| Full Design Feature | MVP Status |
|---|---|
| Multiplayer / networking | Cut — hotseat only |
| Planning phase timer | Optional, not enforced |
| Campaign / story mode | Cut |
| Bot AI | Cut — human plays both sides |
| Resource-retrieving creeps | Cut — simplify to basic retrievers |
| Tower "energy stealing" interactions | Cut |
| Tower branching upgrade paths | Cut — 1 linear upgrade per tower |
| Tower upgrade levels 3–4 | Cut — base + 1 upgrade only |
| Debris from mutual-build collisions | Simplified — just block the cell |
| Dropped flag location persistence | Kept (core mechanic) |
| Flag aura buff | Cut |
| Creep skill tree (dependencies) | Cut — flat independent purchases |
| Creep HP / speed upgrades | Cut |
| ~10 tower types | 3 tower types (Gunner, Slammer, Resource) |
| Wall, Spike, Electric towers | Cut |
| Scout creep type | Cut |
| Scout-related upgrades | Cut |
| Fog of war | Cut — full visibility |
| Creep HP bars | Cut — no visual health indicator |
| Damage numbers | Cut |
| Smooth creep animation | Cut — snap to grid cells per tick |
| Simulation playback controls | Cut — fixed 1x speed |
| Detailed creep corpse inspection | Cut |
| Map selection | 1 hardcoded map |
| HTML/CSS UI panel | Cut — all UI rendered on canvas as text/rectangles |

---

## 15. Build & Run

```bash
# Install Emscripten SDK (one-time)
# https://emscripten.org/docs/getting_started/downloads.html

# Build
make

# Serve locally (WASM requires HTTP, not file://)
python3 -m http.server 8080 --directory build/

# Open http://localhost:8080
```
