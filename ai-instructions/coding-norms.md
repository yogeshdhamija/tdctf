# Coding Norms

## Data-Oriented Style

Use a data-oriented coding style, not object-oriented. Structs should be plain bags of data with no behavior attached. Functions should be organized into cohesive modules, but never bundled with the data they operate on. Keep data and logic separate.

## Strong Typing

Be as strongly typed as the language allows. Leverage the type system to make invalid states unrepresentable.

## Performance Awareness

Think about how the machine actually executes your code. Prefer contiguous data layouts over pointer-heavy structures. Avoid unnecessary indirection — every layer of abstraction, every virtual dispatch, every pointer chase is a cost. When there is a simple, direct way to do something and an "architecturally clean" way that adds indirection, prefer the direct way. Indirection is only justified when it solves a concrete, present-tense problem, not a hypothetical future one.

## Compress, Don't Decompose

Good code cleanup means making code *shorter and more capable*, not splitting it into more pieces. When you clean up code, aim to compress: find redundancy, eliminate unnecessary steps, and make each line of code do more useful work. Do not confuse "more files" or "more functions" with "cleaner." A single 40-line function that reads clearly top to bottom is better than five 8-line functions that force you to jump around to understand what's happening. Code hygiene is important — but hygiene means compression, not decomposition.

## Consumer-Driven Coding

TDD is not required, but testing is essential. Follow "consumer-driven coding" — all code changes are driven by tests written at the boundary of the interface being changed.
This includes MVP code-- don't write any code that isn't intended to be tested.

### How it works

1. **Start from the consumer's perspective.** Before writing or changing any code, write a test at the boundary of the interface you're modifying. The test expresses what the consumer of that interface expects.
   - Example: "I want the game to have creeps" → write a test at the game layer asserting that creeps exist → write code to make it pass.

2. **Be intentional when breaking code into smaller functions** DON'T be like Uncle Bob. Only extract functions where reuse is expected or multiple callers are desired. This is part of the philosophy — not premature abstraction, but deliberate interface design. When working on X, when you extract a function, be aware that you're creating a new interface with a level of abstraction of (X+1). 

3. **Smaller functions form a new interface layer.** You do not need to separately test extracted functions when they're covered by the original consumer test. However, if you later change the behavior of one of these functions, that change must be driven by a new test written at *that* function's interface's level of abstraction (X+1) — one layer down from the original test.
