# Reworked testing norm — "layer of abstraction" model

**Date:** 2026-05-19
**Files touched:** `docs/ai-instructions/coding-norms.md`.

## Why

The Consumer-Driven Coding norm had two problems by the end of the day:

1. The first revision still left the layer-coverage question ambiguous,
   and the codebase was reading it as "game-layer changes need tests;
   other layers are optional."
2. More importantly, the "interface boundary" framing was a misread of
   the underlying intent. People (and Claude) were reading "interface
   boundary" as "every function boundary" — implying that, for example,
   a static BFS helper would need its own test on first introduction.
   That isn't what's wanted. The real concept is **layer of
   abstraction**: tests sit at the layer of the behavior being
   protected. A higher-level test exercises *one* code path through the
   lower-level behaviors it sits on; branches in the lower layer get
   tested at the lower layer, not by adding cases to the test above.

## Changes

Replaced the entire **Consumer-Driven Coding** section with a new
**Layered Testing** section. Headline shifts:

- "Interface boundary" terminology is gone. The replacement concept is
  **layer of abstraction**.
- Worked example added: implementing "towers can take damage." Start
  with a feature-layer test (HP decreases on attack); if implementation
  reveals a separable behavior like a damage-calculation function with
  branches, that gets its *own* test at its own (lower) layer — not
  more cases on the feature-layer test.
- Codified the no-duplicate-coverage rule: a higher-layer test
  exercises one code path through whatever sits below it; further
  branches in lower layers are tested at the lower layers.
- Codified the "test must be observed failing" requirement. Order is
  flexible — test-first or code-first both OK — but a test that never
  failed isn't a real test.
- Kept the two test-not-required carve-outs from the previous draft:
  self-evidently-correct changes and purely visual changes.
- Dropped the old three-step "How it works" list — its content is
  absorbed into the new "Applying it" bullets.

## Verification

- Documentation-only change — no code touched.
- `make test` still passes (237 assertions, 0 failures).
