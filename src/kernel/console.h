#ifndef CONSOLE_H
#define CONSOLE_H

#include "kernel.h"

void console_init(BootInfo *info);
void console_putchar(char c);
void console_print(const char *s);
void console_clear(void);
void kprintf(const char *fmt, ...);
void print_hex(uint64_t zahl);
void print_uint(uint32_t zahl);
void print_zahl(int zahl);
void console_set_pos(int col, int row);
int  console_get_col(void);
int  console_get_row(void);
void console_backspace(void);
void console_set_cursor(int col, int row);
void console_write_at(int x, int y, char c);
void console_write_str_at(int x, int y, char *s);
void console_clear_line(int y);
void console_write_at_color(int x, int y, char c, uint8_t color);
int console_get_width(void);    // gibt max_cols
int console_get_height(void);   // gibt max_rows
#endif