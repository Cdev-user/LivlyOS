#ifndef IO_H
#define IO_H
#include "kernel.h"


// static inlen sagt dem compiler überl wo das aufgerufen wird schreib diese funktion da hin


static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outw(uint16_t port, uint16_t val) {
    // port = wohin geschrieben wird (I/O Port)
    // val = der 16-Bit Wert, der geschrieben wird
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
    // %0 = val (kommt ins AX Register)
    // %1 = port (direkt oder über DX)
    // bedeutet: schreibe val -> port
}
#endif