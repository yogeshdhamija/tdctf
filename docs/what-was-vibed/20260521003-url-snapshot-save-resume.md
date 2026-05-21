# URL-encoded snapshot save/resume

## Motivation

Wanted to be able to bookmark a game state, share it as a link, and use
the browser back/forward buttons to navigate between turns of a game in
progress. Constraint: the snapshot has to keep working when
`data/towers.cfg`, `data/creep_upgrades.cfg`, or `data/map.cfg` are edited
between save and load, as long as the edit isn't a direct conflict
(rename / removal). That means the snapshot can't bake config-derived
data into itself — it has to reference towers and upgrades by their
string IDs and pull stats from whatever catalog is live at load time.

## What changed

- New API in `src/game/game.{c,h}`:
    `int game_snapshot_encode(char *out, int out_size);`
    `int game_snapshot_load(const char *src);`

- Encoded format (single URL-safe line, RFC 3986 unreserved + `~ , :`):

  ```
  v1~T<turn>~P<phase>~R<res>:<inc>~r<upgs>~B<res>:<inc>~b<upgs>~W<towers>~F<flag-coords>
  ```

  Phase = `R`/`B`/`S`/`O`. Upgrades and towers reference their config IDs
  (e.g. `RETRIEVER_1`, `GUNNER`) so cfg reorders/edits don't break old
  snapshots. Tower stats (damage, range, cost, max_hp) and creep spawn
  counts come from the live catalog at load time — not from the snapshot.

- Things the snapshot *doesn't* carry, on purpose:
  - Creeps mid-sim. The snapshot only records phase boundaries; if phase
    is `SIMULATE`, the load path calls `start_simulation()` which
    deterministically re-spawns creeps from the completed upgrades it
    just restored.
  - `sim_tick`, `sim_frame_accum`, `sim_end_hold`, beam ttl, tower
    cooldown — these reset cleanly when `start_simulation` runs.
  - UI state (selection, placement intent) — `game_init_state` zeros
    these.

- Load path is lenient: a tower whose ID no longer exists in the cfg is
  silently dropped, ditto an upgrade whose ID was removed. A tower
  position that's now debris or out of bounds is dropped. The rest of
  the snapshot still loads.

- `src/platform/platform_web.c` now wires the snapshot into the URL:
  - `Lock In` click → `game_lock_in()` → `pushState` with new snapshot.
  - `Restart` click → `game_init()` → `pushState` with fresh snapshot.
  - `popstate` (browser back/forward) → re-read `?s=…` and load it.
  - On first paint, if the URL had `?s=…`, load it before sizing the
    canvas (so a future cross-map snapshot would land correctly).
    Either way, `replaceState` anchors the URL to the current state so
    back/forward has somewhere to return to even before the first Lock
    In.

- `Makefile`: exported `stringToUTF8` alongside `UTF8ToString` so the JS
  side can write the URL param into the wasm snapshot buffer.

## Tests

New `tests/test_snapshot.c` exercises the snapshot at the game layer:

- `round_trip_basic` — turn / phase / resources / placed tower / bought
  upgrade survive encode + reset + load.
- `round_trip_preserves_tower_hp` — encoded HP isn't silently refilled
  to max_hp on reload (would otherwise let players "heal by bookmark").
- `load_drops_unknown_tower_id` — encoding has a GUNNER and a BLOCKER,
  reload uses a cfg without GUNNER → GUNNER is silently dropped,
  BLOCKER restores.
- `load_survives_upgrade_reorder` — RETRIEVER_1 is at slot 0 originally,
  reload uses a cfg where it's at slot 1 and SIEGE_2 is removed →
  RETRIEVER_1 state follows the *name* to slot 1, SIEGE_2 drops silently.
- `simulate_phase_respawns_creeps` — snapshot taken at `PHASE_SIMULATE`
  restores back to `PHASE_SIMULATE` and the deterministic creep spawn
  list is recreated by the load-path `start_simulation` call.
- `encoded_form_is_url_safe` — every char in the encoded snapshot is in
  `[A-Za-z0-9~:,-_.]`.

Each was observed failing during development against deliberate breakages
in the encode/decode path before this commit landed.
