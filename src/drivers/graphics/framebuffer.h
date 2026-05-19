#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../../kernel/kernel.h"
/* Standard Farben (ARGB, A=0 ignoriert) */
#define COLOR_BLACK    0x00000000
#define COLOR_WHITE    0x00FFFFFF
#define COLOR_RED      0x00FF0000
#define COLOR_GREEN    0x0000FF00
#define COLOR_BLUE     0x000000FF
#define COLOR_YELLOW   0x00FFFF00
#define COLOR_CYAN     0x0000FFFF
#define COLOR_MAGENTA  0x00FF00FF
#define COLOR_GRAY     0x00808080
#define COLOR_DARKGRAY 0x00404040
#define COLOR_DARKBLUE 0x00000033

typedef struct {
    uint64_t address;   // physische Adresse
    uint32_t width;
    uint32_t height;
    uint32_t pitch;     // Bytes pro Scanline 
    uint8_t  bpp;
} framebuffer_t;

void fb_init(BootInfo *info);
framebuffer_t* fb_get(void);     // gibt globalen Framebuffer zurück

void fb_clear(framebuffer_t *fb, uint32_t color);
void fb_draw_pixel(framebuffer_t *fb, int x, int y, uint32_t color);
void fb_draw_rect(framebuffer_t *fb, int x, int y, int w, int h, uint32_t color);
void fb_draw_line(framebuffer_t *fb, int x0, int y0, int x1, int y1, uint32_t color);
void fb_draw_char(framebuffer_t *fb, int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(framebuffer_t *fb, int x, int y, const char *s, uint32_t fg, uint32_t bg);

bool fb_in_bounds(framebuffer_t *fb, int x, int y);

#endif