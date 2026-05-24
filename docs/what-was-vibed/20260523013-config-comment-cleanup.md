# Config comment cleanup

Compressed the comment blocks in `data/map.cfg`, `data/towers.cfg`, and `data/creep_upgrades.cfg`. Goal: preserve the schema/field documentation and per-section intent, drop implementation-detail prose and verbose narrative.

What got cut:
- Long worked examples of the upgrade merge semantic (kept the rule, dropped the disjoint-fields illustration).
- Per-tower design essays (Blocker/Gunner/Splasher/Resource) collapsed to a single role line each above the `tower` block.
- Per-creep role essay collapsed to one line listing roles and spawn order.
- "Unspecified keys default to 0" repeated boilerplate folded into the key-list intro.

What stayed:
- Every documented field name, meaning, and default.
- Section grammar (`creep <ID>`, `upgrade <ID>`, `tower <ID>`, `level <ID> <N>`) and ordering rules.
- The merge semantic for creep upgrades (overlay, declaration order, later-wins, inherit).
- Map cell symbol table and grid format rules.
- Commented-out "standard map" alternative.

Tests: `make test` — all suites pass (656 assertions, 0 failures).
