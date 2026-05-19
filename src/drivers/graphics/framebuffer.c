#include "framebuffer.h"
#include "font.c"
static framebuffer_t g_fb;

void fb_init(BootInfo *info) {
    g_fb.address = (uint64_t)info->fb_addr;
    g_fb.width   = info->fb_width;
    g_fb.height  = info->fb_height;
    g_fb.pitch   = info->fb_pitch;
    g_fb.bpp     = info->fb_bpp;
}

framebuffer_t* fb_get(void) {
    return &g_fb;
}

bool fb_in_bounds(framebuffer_t *fb, int x, int y) {
    return (x >= 0 && y >= 0 && (uint32_t)x < fb->width && (uint32_t)y < fb->height);
}

void fb_draw_pixel(framebuffer_t *fb, int x, int y, uint32_t color) {
    if (!fb_in_bounds(fb, x, y)) return;
    volatile uint32_t *pixel = (volatile uint32_t*)(fb->address + y * fb->pitch + x * 4);
    *pixel = color;
}

void fb_clear(framebuffer_t *fb, uint32_t color) {
    for (uint32_t y = 0; y < fb->height; y++) {
        for (uint32_t x = 0; x < fb->width; x++) {
            volatile uint32_t *pixel = (volatile uint32_t*)(fb->address + y * fb->pitch + x * 4);
            *pixel = color;
        }
    }
}

void fb_draw_rect(framebuffer_t *fb, int x, int y, int w, int h, uint32_t color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            fb_draw_pixel(fb, x + dx, y + dy, color);
        }
    }
}

void fb_draw_line(framebuffer_t *fb, int x0, int y0, int x1, int y1, uint32_t color) {
    /* Bresenham */
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        fb_draw_pixel(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}
extern const uint8_t font8x16[96][16];

void fb_draw_char(framebuffer_t *fb, int x, int y, char c, uint32_t fg, uint32_t bg) {
    uint8_t uc = (uint8_t)c;
    if (uc < 0x20 || uc > 0x7F) uc = 0x20;
    const uint8_t *glyph = font8x16[uc - 0x20];
    
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            fb_draw_pixel(fb, x + col, y + row, color);
        }
    }
}

void fb_draw_string(framebuffer_t *fb, int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        fb_draw_char(fb, x, y, *s, fg, bg);
        x += 8;   /* nächste Char-Position */
        s++;
    }
}