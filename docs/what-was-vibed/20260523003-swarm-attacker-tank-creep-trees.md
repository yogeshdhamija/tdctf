# Swarm / attacker / tank creep trees

## What changed

Rewrote `data/creep_upgrades.cfg` to define three creep types — `SWARM`,
`ATTACKER`, `TANK` — each with a four-rung research ladder. The first
rung is "+1 <type>" and defines the full creep profile. The next three
rungs each double a single stat and are gated on the previous rung via
`requires`.

| Tree     | Profile (set by +1)                              | Doublings (3×)                |
|----------|--------------------------------------------------|-------------------------------|
| Swarm    | hp 12, count 1, flag, code `w`, spawn_order 2    | count → 2, 4, 8               |
| Attacker | hp 40, count 1, flag, melee 10, code `a`, so 3   | melee_damage → 20, 40, 80     |
| Tank     | hp 80, count 1, flag, code `t`, spawn_order 1    | hp → 160, 320, 640            |

All three types can carry the flag. The swarm doublings all cost the
same (60), per request — attacker and tank doublings escalate in cost
and research turns.

Spawn order is tank (1) → swarm (2) → attacker (3): tanks step out
first to soak fire, swarm pours through behind them, attackers trail
to chew through anything still standing.

The doublings only restate the one field they change — `count` for
swarm, `melee_damage` for attacker, `hp` for tank. The merge semantic
in `game.c::compute_active_profiles` already inherits every
unspecified field from the previous overlay, so the inherited
`spawn_order`, `code`, `can_carry_flag`, etc. survive automatically.

## Cap bump

The new tree is 12 upgrades (3 trees × 4 rungs), but
`CREEP_UPGRADE_MAX_COUNT` and `MAX_CREEP_UPGRADES` were capped at 8.
Bumped both to 16 so the parser accepts the cfg and the per-player
state array fits. No behavior change beyond raising the ceiling — the
parser already errors past the cap, the existing per-player state
loop is bounded by the cap, and the snapshot buffer (4096 bytes) has
plenty of headroom for 12 short-id upgrades.

## Why this fit the "config-only-ish" budget

All the new behavior — the merge semantic that lets a doubling restate
a single field, the `requires` gating, the spawn_order sort, the
per-type catalogue — already existed from prior changes (see
`20260523001`, `20260523002`, `20260522001`). The only C-side change
was the cap bump.

## Tests

No new tests. The cfg values themselves are gameplay tuning, which
the test suite intentionally doesn't cover (tests load fixture cfgs,
per tech-design.md). The merge/requires/spawn-order behaviors the new
tree relies on each have existing fixture-based tests in
`test_game.c` / `test_creep_config.c`, and all 547 assertions across
the seven test binaries pass after the change.
