#include "console.h"
#include "vga.h"
#include "../drivers/graphics/fb_console.h"
#include "../drivers/graphics/framebuffer.h"
#include "../drivers/keyboard.h"
static int use_fb = 0;

void console_init(BootInfo *info) {
    if (info->fb_addr != 0) {
        fb_init(info);
        fb_console_init(fb_get());
        use_fb = 1;
    } else {
        // VGA Text Mode - schon initialisiert durch VGA Hardware
        use_fb = 0;
    }
}


void console_putchar(char c) {
    if (use_fb) fb_console_putchar(c);
    else        vga_putchar(c);
    
    /* Cursor an neue Position */
    console_set_cursor(console_get_col(), console_get_row());
}
void console_print(const char *s) {
    while (*s) {
        console_putchar(*s);
        s++;
    }
}

void console_clear(void) {
    if (use_fb) fb_console_clear();
    else        vga_clear();
}


void print_zahl(int zahl) {
    if (zahl == 0) {
        console_putchar('0');
        return;
    }
    if (zahl < 0) {
    console_putchar('-');   // Minus-Zeichen ausgeben
    zahl = -zahl;       // -42 → 42
    }
    
    char puffer[20];
    int laenge = 0;
    
    while (zahl > 0) {
        puffer[laenge] = '0' + (zahl % 10);
        laenge++;
        zahl = zahl / 10;
    }
    
    // rückwärts ausgeben
    for (int i = laenge - 1; i >= 0; i--) {
        console_putchar(puffer[i]);
    }
}


void kprintf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);  // starte nach fmt

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                int zahl = __builtin_va_arg(args, int);  // holt die 42
                print_zahl(zahl);  
            }
            else if (*fmt == 's') {
                char *s = __builtin_va_arg(args, char*);
                console_print(s);  
            }
            else if (*fmt == 'x') {
            uint64_t zahl = __builtin_va_arg(args, uint64_t);
            print_hex(zahl);
            }
            else if(*fmt == 'c') {
                char c = __builtin_va_arg(args, int);  // char wird als int übergeben
                console_putchar(c);
            }
            else if (*fmt == 'u') {
            uint32_t zahl = __builtin_va_arg(args, uint32_t);
            print_uint(zahl);
            }
            else if (*fmt == 'p') {
            uint64_t adresse = __builtin_va_arg(args, uint64_t);
            print_hex(adresse);  // kennst du schon!
            }
        } else {
            console_putchar(*fmt);  // normales Zeichen
        }
        fmt++;
    }

    __builtin_va_end(args);
}

void print_hex(uint64_t zahl) {
    if (zahl == 0) {
        console_print("0x0");
        return;
    }
    char puffer[20];
    int laenge = 0;
    while (zahl > 0) {
        int ziffer = zahl % 16;  // statt 10
        if (ziffer < 10) {
            puffer[laenge] = '0' + ziffer;
        } else {
            puffer[laenge] = 'A' + (ziffer - 10);  // 10→A, 11→B ...
        }
        laenge++;
        zahl /= 16;  // statt 10
    }
    console_print("0x");
    for (int i = laenge - 1; i >= 0; i--)
        console_putchar(puffer[i]);
}

void print_uint(uint32_t zahl) {
    if (zahl == 0) { console_putchar('0'); return; }
    
    char puffer[20];
    int laenge = 0;
    
    while (zahl > 0) {
        puffer[laenge] = '0' + (zahl % 10);
        laenge++;
        zahl /= 10;
    }
    
    for (int i = laenge - 1; i >= 0; i--)
        console_putchar(puffer[i]);
}

void console_set_pos(int col, int row) {
    if (use_fb) fb_console_set_pos(col, row);
    else        vga_set_pos(col, row);
    
    /* Cursor mit nachziehen */
    console_set_cursor(col, row);
}

int console_get_col(void) {
    if (use_fb) return fb_console_get_col();
    else        return vga_get_col();
}

int console_get_row(void) {
    if (use_fb) return fb_console_get_row();
    else        return vga_get_row();
}

void console_backspace(void) {
    if (use_fb) fb_console_backspace();
    else        vga_backspace();
}

void console_set_cursor(int col, int row) {
    if (use_fb) fb_console_set_cursor(col, row);
    else        vga_set_cursor(col, row);
}
void console_write_at(int x, int y, char c) {
    if (use_fb) fb_console_write_at(x, y, c);
    else        vga_write_at(x, y, c);
}

void console_write_str_at(int x, int y, char *s) {
    if (use_fb) fb_console_write_str_at(x, y, s);
    else        vga_write_str_at(x, y, s);
}

void console_clear_line(int y) {
    if (use_fb) fb_console_clear_line(y);
    else        vga_clear_line(y);
}

void console_write_at_color(int x, int y, char c, uint8_t color) {
    if (use_fb) fb_console_write_at_color(x, y, c, color);
    else        vga_write_at_color(x, y, c, color);
}

int console_get_width(void) {
    if (use_fb) return fb_console_get_width();
    else        return VGA_BREITE;   // 80
}

int console_get_height(void) {
    if (use_fb) return fb_console_get_height();
    else        return VGA_HOEHE;    // 25
}