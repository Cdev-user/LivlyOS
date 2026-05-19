#ifndef TIMER_H
#define TIMER_H
#include "kernel.h"

// 1. Eine Konstante für die Frequenz (wieviele Ticks pro Sekunde?)
// 2. Die globale Variable system_ticks — aber wie deklariert man
//    eine Variable im Header ohne sie zu definieren?
// 3. Zwei Funktionen: timer_init() und get_ms()


#define TIMER_HZ 1000  // 1000 Ticks pro Sekunde



extern uint64_t system_ticks;  


void timer_init();
uint64_t get_ms();

#endif