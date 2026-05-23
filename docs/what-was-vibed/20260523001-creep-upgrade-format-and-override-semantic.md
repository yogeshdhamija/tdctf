# Creep upgrade config: properties on upgrades, merge semantic

## Motivation

Previously, `data/creep_upgrades.cfg` defined a creep type's static
profile (code, hp, can_carry_flag, melee_damage, spawn_order) in the
`creep <ID>` section, and upgrades only specified `spawn <CREEP_ID> <N>`
to add `N` creeps per turn. Stacking was additive across upgrades.

The user wants upgrades to be able to *change* a creep type's profile
— including the per-turn count. That's incompatible with the old
"types own the profile, upgrades just add count" split.

## Changes

### Config format (`data/creep_upgrades.cfg`)

- Creep type sections are now bare identifiers: `creep <ID>` on its own,
  with no body.
- Upgrades own the full creep profile they activate:
  - `creep <ID>` — target creep type (replaces `spawn <ID> <N>`).
  - `count <N>`  — how many of those creeps spawn per turn.
  - `code`, `hp`, `can_carry_flag`, `melee_damage`, `spawn_order` — moved
    here from the type section.
  - `cost`, `research_turns`, `description` — unchanged.

### Merge semantic (per-field overlay)

For each `(player, creep_type)` pair, the active profile is built by
overlaying every completed upgrade targeting that type, in declaration
order. Each upgrade contributes only the fields it *explicitly set* in
the cfg — unspecified fields inherit from the previous overlay.

Examples:
- Upgrade A: `count 1, hp 25, melee_damage 5`
- Upgrade B: `hp 50`
- Both completed → `count 1` (A), `hp 50` (B wins), `melee_damage 5` (A).

Two upgrades that set the same field: the later-declared wins for that
field. Two upgrades that set disjoint fields combine. This lets pure
stat-buff upgrades exist alongside full-profile ones.

To distinguish "field set to 0" from "field not specified," the parser
records which fields it observed via a `set_flags` bitmask on
`CreepUpgradeConfig` (`CREEP_UPG_SET_COUNT`, `_CODE`, `_HP`,
`_CAN_CARRY_FLAG`, `_MELEE_DAMAGE`, `_SPAWN_ORDER`). The merge walk in
`game.c::compute_active_profiles` reads these bits to know which
fields to overlay.

### Code

- `CreepTypeConfig` shrinks to `{ id }`.
- `CreepUpgradeConfig` gains `creep_type` (replaces `spawn_type`), `count`
  (replaces `spawn_count`), plus the moved-over profile fields.
- Parser overloads the `creep` keyword: at the top level it declares a
  type section; inside an upgrade body it sets the target type. The
  upgrade body is the only section with inner keys; a type's section
  body is rejected.
- `Player` gains `ActiveCreepProfile active_creeps[MAX_CREEP_TYPES]`
  — the merged per-(player, creep type) profile. Rebuilt by
  `compute_active_profiles` at every `start_simulation`. Sim-tick
  accessors (flag-pickup, melee, spawn) read straight off
  `s.players[owner].active_creeps[type]`; no per-creep `spawning_upgrade`
  snapshot is needed.
- `spawn_queue` stores `CreepType` indices. `start_simulation` pushes
  `count` entries per active type and sorts by the merged
  `spawn_order`.
- Public API: `game_creep_type_*` per-field accessors removed; the
  active per-(player, type) profile is exposed via
  `game_creep_is_active` + `game_creep_active_{count, code, hp,
  can_carry_flag, melee_damage, spawn_order}(PlayerID, CreepType)`.
  Per-upgrade accessors for the moved profile fields were not added —
  they'd be misleading at the call sites that need a merged view.
  `game_creep_upgrade_count(void)` (catalog total) became
  `game_creep_upgrade_total(void)`.

### Tests

- `tests/test_creep_config.c` rewritten for the new schema: types are
  bare, all profile fields tested on upgrade bodies, type-body keys are
  errors, `creep <ID>` resolution inside upgrade bodies tested,
  spawn_order moved to upgrade-level.
- Fixtures in `tests/test_fixtures.h` translated to the new shape with
  slot ordering preserved.
- `tests/test_snapshot.c` `TEST_CREEP_UPGRADES_REORDERED_CFG` translated.
- New test `test_merge_semantic_field_overlay` in `tests/test_game.c`:
  a small dedicated cfg with `BASE` (full profile incl. count=1, hp=25,
  melee_damage=5) and `BUFF_HP` (only hp=50, same target type) — after
  both complete, asserts the active profile is the per-field merge
  (count=1, hp=50, melee_damage=5, plus the other base fields). Would
  fail under pure override (count and melee_damage would drop to 0).
- New parser test `test_set_flags` in `tests/test_creep_config.c`: pins
  that `set_flags` matches the keys observed in the cfg, including the
  "explicit zero" round-trip (a cfg line `hp 0` still flips the HP bit
  so a later upgrade inheriting won't recover an unset HP).

## Data file (`data/creep_upgrades.cfg`)

The default ladder is preserved by ID and pacing, but RETRIEVER upgrades
no longer stack: RECRUIT (+1), STRIKE_FORCE (+2), and ASSAULT_PACK (+3)
are now sequential *replacements* rather than additive tiers. Same for
SIEGE (SIEGE_TEAM → WRECKING_CREW) and BRUISER (BRUISER_SQUAD →
ELITE_GUARD). Game balance may need re-tuning later; the structural
change is independent of those values.

Each ladder entry restates the full creep profile. A future "stat-buff
only" upgrade (e.g. just `hp 50` on top of RECRUIT's profile) would
now Just Work: the merge layer preserves count and other unset fields
from the previously-active overlay.
