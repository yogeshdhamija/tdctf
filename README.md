# tdctf

- See `docs/game-design.md` for an understanding of what is being built.
- See `docs/tech-design.md` for an understanding of the code.
- If you're GenAI, before writing any code, read and follow `docs/ai-instructions/coding-norms.md`.
- If you're GenAI, as part of making any changes, read and follow `docs/ai-instructions/context-preservation.md`

## Common commands:
```
make clean
make test
make build
```

## Setup
Install the Emscripten SDK (one-time):
    make setup

## Build
    make

Produces a single self-contained `build/index.html` (wasm and JS embedded as base64). Double-click it in a file browser, or send it to a friend — no server required.
