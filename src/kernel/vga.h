// vga.h
#ifndef VGA_H
#define VGA_H
#include "kernel.h"



#define VGA_SCHWARZ  0
#define VGA_BLAU     1
#define VGA_GRUEN    2
#define VGA_CYAN     3
#define VGA_ROT      4
#define VGA_MAGENTA  5
#define VGA_BRAUN    6
#define VGA_WEISS    7

#define VGA_FARBE(fg, bg) ((bg) << 4 | (fg))




#define VGA_PUFFER ((volatile uint16_t*)0xB8000) 

#define VGA_HOEHE 25
#define VGA_BREITE 80

void vga_putchar(char c);
void vga_print(char *s);
void vga_setfarbe(uint8_t f);
void vga_backspace();
void vga_set_cursor(int col, int row);
void vga_clear();
void vga_write_at(int x, int y, char c);
void vga_clear_line(int y);
void vga_set_pos(int x, int y);
void vga_write_str_at(int x, int y, char *s);
void vga_write_at_color(int x, int y, char c, uint8_t color);
int vga_get_col();
int vga_get_row();

#endif