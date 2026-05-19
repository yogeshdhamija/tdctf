# FPS and Max-Lag Frame Counter

## What changed
Added a diagnostic overlay showing FPS and maximum frame lag over the last 60 seconds.

## Files modified
- `src/platform/platform.h` — Added `FrameStats` struct and `plat_get_frame_stats()` API
- `src/platform/platform_web.c` — Frame timing measurement using `emscripten_get_now()` with a 3600-entry ring buffer (~60s at 60fps). Computes rolling FPS (averaged over last 60 frames) and tracks worst frame delta in the buffer.
- `src/render/render.c` — Displays "FPS:XX  MaxLag:XXms" in gray text at bottom-left of the grid area.

## Design decisions
- Timing lives in the platform layer (where the frame loop runs), exposed via `platform.h` so the render layer can display it without platform dependencies.
- Ring buffer of 3600 entries covers ~60 seconds of history at 60fps for the max-lag metric.
- FPS is averaged over the most recent 60 frames for stability.
