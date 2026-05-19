// schedular.h
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"
#include "../kernel/idt.h"   // reason: schedule() needs InterruptFrame*

void schedular_init();
void scheduler_add(char *name, void *entry, uint8_t priority);
void schedular_remove(uint32_t pid);
void schedule(InterruptFrame *frame);   // needs the live frame!
PCB *get_current_process();
PCB *scheduler_get_head();  // gibt head zurück
void reaper_process(void);
#endif