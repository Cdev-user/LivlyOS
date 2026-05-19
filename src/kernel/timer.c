#include "timer.h"
#include "io.h"

// Hier kommt die echte Definition von system_ticks
uint64_t system_ticks = 0;

void timer_init() {
    // PIT konfigurieren
    // Divisor berechnen: 1193180 / TIMER_HZ
    // Was schreibst du auf welche Ports?
    

    uint16_t divisor = 1193180 / TIMER_HZ;  // = 1193

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);  // unteres Byte direkt
    outb(0x40, divisor >> 8);    // oberes Byte direkt




}

uint64_t get_ms() {
    // Bei 1000 Hz = system_ticks direkt in Millisekunden!
    return system_ticks;
}