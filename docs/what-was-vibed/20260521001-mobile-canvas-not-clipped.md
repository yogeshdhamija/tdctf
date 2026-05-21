# Mobile: canvas no longer clipped, supports pinch-zoom

## Problem
On mobile browsers, the game's `<canvas>` was clipped — the right/bottom of
the grid and sidebar were unreachable, and there was no way to pan or zoom.

The `shell.html` had no `<meta name="viewport">` tag, so mobile browsers
defaulted to ~980 CSS px wide and clipped anything beyond that. The body's
`height: 100vh` + flex centering also prevented vertical scrolling when the
canvas was taller than the viewport.

## Fix
Two-file change, no game/render logic touched.

- `src/platform/shell.html`
  - Added `<meta name="viewport" id="viewport" content="width=device-width, initial-scale=1, user-scalable=yes">` as a sensible default before the canvas init runs.
  - Replaced `height: 100vh` with `min-height: 100vh`, and `align-items: center` with `align-items: flex-start`, so the page can scroll vertically when the canvas is taller than the viewport.
- `src/platform/platform_web.c` (inside `js_canvas_init`)
  - After setting canvas dimensions, rewrite the viewport `content` to
    `width=<canvas_w>, initial-scale=1, user-scalable=yes`. This makes the
    entire canvas width fit the device screen on first load regardless of
    device size, while leaving pinch-zoom enabled so the player can zoom in
    to tap small UI elements or zoom out to see everything at once.

Click coordinates were already correct on scaled canvases — the existing
mousedown handler scales by `canvas.width / rect.width`, and mobile browsers
synthesize a `mousedown` from a single-finger tap.

## Test
Purely visual/platform change (per `docs/ai-instructions/coding-norms.md`
"When a test is not required" → purely visual). Verified by clean rebuild.
