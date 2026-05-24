# Coding Norms

## General Style
- Variable names should be descriptive, and not shortened (exceptions may exist). Example: "grid_height" instead of "gh".
- Variables should be declared close to their first use, rather than at the top of the function.

## Data-Oriented Style

Use a data-oriented coding style, not object-oriented. Structs should be plain bags of data with no behavior attached. Functions should be organized into cohesive modules, but never bundled with the data they operate on. Keep data and logic separate.

## Strong Typing

Be as strongly typed as the language allows. Leverage the type system to make invalid states unrepresentable.

## Performance Awareness

Think about how the machine actually executes your code. Prefer contiguous data layouts over pointer-heavy structures. Avoid unnecessary indirection — every layer of abstraction, every virtual dispatch, every pointer chase is a cost. When there is a simple, direct way to do something and an "architecturally clean" way that adds indirection, prefer the direct way. Indirection is only justified when it solves a concrete, present-tense problem, not a hypothetical future one.

## Compress, Don't Decompose

Good code cleanup means making code *shorter and more capable*, not splitting it into more pieces. When you clean up code, aim to compress: find redundancy, eliminate unnecessary steps, and make each line of code do more useful work. Do not confuse "more files" or "more functions" with "cleaner." A single 40-line function that reads clearly top to bottom is better than five 8-line functions that force you to jump around to understand what's happening. Code hygiene is important — but hygiene means compression, not decomposition.

## Layered Testing

Every code change in every layer (game, render, platform) ships with a test that exercises the change. Two requirements on the test:

- It exists by the time the change is committed.
- It has been observed failing at some point during the change. A test that has never failed isn't a real test — it might be passing trivially.

### When a test is not required

- **Self-evidently correct when running the game.** Copy edits, trivial renames, an obviously-wrong constant.
- **Purely visual.** Colors, font sizes, spacing, animation timing, layout nudges. Reviewed by eye.

When in doubt, write the test.

### Test at the layer of abstraction of the change

Every behavior lives at a particular **layer of abstraction**. The test for that behavior sits at the same layer — not above, not below.

Worked example: "towers, previously immune, now take damage."

1. Start at the feature layer. Write a test that asserts towers have HP and that an attack reduces HP. This is the level of abstraction of the feature itself.
2. While implementing, you realize different creep types should deal different damage. The damage calculation is now a smaller, separable behavior — a *lower* layer of abstraction.
3. Write a focused test against the damage calculation directly: given creep type and base damage, the result is X. It doesn't run a full simulation tick — it sits one layer below the feature test.

A higher-layer test exercises **one** code path through the lower-layer behaviors it sits on top of. It is not exhaustive — that's not its job. When a lower-layer function gains a branch (or already has one), each branch is tested at the lower layer, not by piling more cases onto the higher-layer test.

### Applying it

- **New feature:** the first test goes at the feature's natural layer of abstraction, with enough cases to cover the feature's *behaviors* at that level (not its internal code paths).
- **New branch in a sub-behavior:** drop a layer and write the test there. Don't duplicate it at the higher layer.
- **Sub-behavior with a single code path, already exercised transitively by a higher-layer test:** leave it alone. Adding a lower-layer test would just duplicate coverage.
- **Modifying an existing sub-behavior:** the change is driven by a test at *its* layer, not the layer above.

This gives you focused failures (a broken test points at the layer that changed) and zero duplicate coverage (each code path is exercised by exactly one test at exactly one layer).
