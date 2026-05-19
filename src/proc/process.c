#include "../kernel/kernel.h"
#include "process.h"
#include "../mm/kmalloc.h"
#include "../mm/pmm.h"

static uint32_t next_pid = 0;


PCB *create_process(char *name, void *entry, uint8_t priority){
    /* Input Validation ZUERST */
    if (!entry) return NULL;
    if (!name) return NULL;
    if (name[0] == '\0') return NULL;
    
    PCB *pcb = kmalloc(sizeof(PCB));
    if (!pcb) return NULL;
    
    /* Stack allokieren - mit Check */
    uint64_t stack = (uint64_t)pmm_alloc();
    if (!stack) {
        kfree(pcb);
        return NULL;
    }
    
    /* PCB nullen damit nichts undefined ist */
    for (int i = 0; i < (int)sizeof(PCB); i++) {
        ((uint8_t*)pcb)[i] = 0;
    }
    
    /* Name kopieren (max 31 Zeichen + Terminator) */
    int i;
    for (i = 0; i < 31 && name[i] != '\0'; i++) {
        pcb->name[i] = name[i];
    }
    pcb->name[i] = '\0';
    
    /* Register Setup */
    pcb->regs.cs     = 0x08;          // Ring 0 Code Segment
    pcb->regs.ss     = 0x10;          // Ring 0 Data Segment
    pcb->regs.rflags = 0x202;         // IF=1 (Interrupts enabled)
    pcb->regs.rip    = (uint64_t)entry;
    pcb->regs.rsp    = stack + 4096;  // Stack wächst nach unten
    
    /* Aktuelle CR3 lesen statt hardcoden (funktioniert in BIOS UND UEFI) */
    __asm__ volatile("mov %%cr3, %0" : "=r"(pcb->cr3));
    
    pcb->stack    = stack;
    pcb->pid      = next_pid++;
    pcb->priority = priority;
    pcb->state    = NEW;
    pcb->next     = NULL;
    
    return pcb;
}

void kill_process(PCB *process){
        if(!process) return;
        if(process->state == ZOMBIE) return;

        process->state = ZOMBIE;        //status auf Zombie machen

        //stack wird vom reaper freigegeben
        return;
        
}    

