#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

void plat_clear(uint32_t color);
void plat_draw_rect(int x, int y, int w, int h, uint32_t color);
void plat_fill_rect(int x, int y, int w, int h, uint32_t color);
void plat_draw_circle(int cx, int cy, int r, uint32_t color);
void plat_fill_circle(int cx, int cy, int r, uint32_t color);
void plat_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void plat_draw_text(int x, int y, const char *text, uint32_t color);
void plat_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);

typedef struct {
    float fps;
    float max_lag_ms; /* worst frame delta in last 60 seconds */
} FrameStats;

void plat_get_frame_stats(FrameStats *out);

#endif
