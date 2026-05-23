# Creep upgrade `requires` prerequisite

## Motivation

Creep upgrades all sat in one flat list. Players could buy the +3 Retrievers
tier on turn 1 (cash permitting) without ever owning the +1 or +2 tier first.
We want a research ladder: certain upgrades should be gated behind another
upgrade having finished researching.

## What changed

Added an optional `requires <UPGRADE_ID>` directive in
`data/creep_upgrades.cfg`. Until the referenced upgrade has reached
`completed`, the dependent upgrade's buy button is disabled (rendered as
`<desc> (locked)`) and `game_buy_creep_upgrade` rejects the purchase.

### Catalog (`creep_config.h` / `creep_config.c`)

- New field `int requires` on `CreepUpgradeConfig`. Holds the catalog index of
  the prerequisite upgrade, or `-1` for "no prerequisite". Initialised to `-1`
  alongside `creep_type`.
- Parser handles `requires` like `creep`: the referenced upgrade is resolved
  at parse time, so it must be declared earlier in the file (forward
  references reject). An upgrade can't require itself.

### Game (`game.h` / `game.c`)

- New accessor `game_creep_upgrade_requires(int idx)`.
- `game_buy_creep_upgrade` now rejects with status "Prerequisite not complete"
  when the required upgrade isn't completed.

### Render (`render.c`)

When the buy button is shown for a locked upgrade, the label becomes
`<desc> (locked)` and the button is disabled. Button id range is unchanged,
so no `ButtonID` shuffle.

### Shipped ladder (`data/creep_upgrades.cfg`)

Applied the natural T1 → T2 → T3 gating:

- `STRIKE_FORCE` requires `RECRUIT`
- `WRECKING_CREW` requires `SIEGE_TEAM`
- `ASSAULT_PACK` requires `STRIKE_FORCE`
- `ELITE_GUARD` requires `BRUISER_SQUAD`

The SWARM and base bruiser/siege tiers stay unlocked from turn 1.

## Tests

- `tests/test_creep_config.c::test_requires` — parser: happy path, default
  `-1`, forward reference rejects, unknown id rejects, self-reference rejects.
  Also extended `test_defaults` to assert `requires == -1`.
- `tests/test_game.c::test_creep_upgrade_requires_gating` — game-layer: buy
  rejects when prerequisite isn't completed (no resource debit), then
  succeeds once the gate completes. Uses a new
  `TEST_CREEP_REQUIRES_CFG` fixture (`tests/test_fixtures.h`) with two
  instant-research upgrades where slot 1 requires slot 0.

Both tests were observed failing (parser → ignored `requires` directive;
game → bypassed gate check) before the implementation was re-enabled.
