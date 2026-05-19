
#include "../../lib/stlib/types.h"
#include "framebuffer.h"

typedef struct {
    framebuffer_t *fb;
    int cursor_col;       // in Zeichen, 0...max_cols-1
    int cursor_row;       // in Zeichen, 0...max_rows-1
    int max_cols;         // fb->width / 8
    int max_rows;         // fb->height / 16
    uint32_t fg_color;
    uint32_t bg_color;
} fb_console_t;

void fb_console_init(framebuffer_t *fb);
void fb_console_putchar(char c);
void fb_console_print(const char *s);
void fb_console_clear(void);
void fb_console_set_color(uint32_t fg, uint32_t bg);


void fb_console_set_pos(int col, int row);
int  fb_console_get_col(void);
int  fb_console_get_row(void);
void fb_console_backspace(void);
void fb_console_set_cursor(int col, int row);

void fb_console_write_at(int x, int y, char c);
void fb_console_write_str_at(int x, int y, char *s);
void fb_console_clear_line(int y);
void fb_console_write_at_color(int x, int y, char c, uint8_t color);

int fb_console_get_width(void);
int fb_console_get_height(void);