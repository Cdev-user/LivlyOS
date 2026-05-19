#ifndef PROCESS_H
#define PROCESS_H

#include "../kernel/kernel.h"
#include "../kernel/idt.h"

typedef enum{
    NEW,         // gerade erstellt, noch nicht in Scheduler-Liste
    READY,       // wartet auf CPU
    RUNNING,     // hat CPU
    BLOCKED,     // wartet auf etwas (sleep, event, I/O)
    ZOMBIE       // tot, wartet auf Cleanup
} ProcessState;

typedef struct PCB {
    InterruptFrame regs;        // alle Register
    uint64_t cr3;               // eigener Adressraum
    ProcessState state;         // Status
    uint32_t pid;               // Prozess-ID

    struct PCB *next;           // nächster Prozess in der Liste
    char name[32];              //Zum beispiel ki_kernel oder so 


    uint64_t stack;
    uint8_t priority;

    uint8_t ticks_remaining;
} PCB;

PCB *create_process(char *name, void *entry, uint8_t priority);   // neuen Prozess erstellen
void kill_process(PCB *process);               // Prozess beenden

#endif
