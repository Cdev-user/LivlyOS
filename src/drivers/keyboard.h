#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "kernel.h"

// Was brauchst du hier?
// 1. Port-Konstante
// 2. Eine Funktion die aufgerufen wird wenn IRQ1 feuert

#define KEYBOARD_PORT 0x60

#define KEY_UP      0x80
#define KEY_DOWN    0x81
#define KEY_LEFT    0x82
#define KEY_RIGHT   0x83
#define KEY_CTRL_C  0x84
#define KEY_CTRL_V  0x85
#define KEY_CTRL_Z  0x86
#define KEY_CTRL_Y  0x87
#define KEY_CTRL_S  0x88
#define KEY_SHIFT_UP    0x89
#define KEY_SHIFT_DOWN  0x8A
#define KEY_SHIFT_LEFT  0x8B
#define KEY_SHIFT_RIGHT 0x8C
void keyboard_handler();      // wird von IRQ1 aufgerufen
char keyboard_getchar();      // gibt letztes gedrücktes Zeichen zurück
void keyboard_push(uint8_t c);  // IRQ1 Handler schreibt rein
void keyboard_init(void);
uint8_t keyboard_read();        // Shell liest raus, wartet wenn leer

#endif