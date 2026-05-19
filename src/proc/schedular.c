#include "schedular.h"
#include "../kernel/gdt.h"
#include "../kernel/console.h"
#include "../kernel/timer.h"
#include "../mm/pmm.h"
#include "../mm/kmalloc.h"
#include "../drivers/graphics/fb_console.h"

static PCB *head = NULL;     // erste Prozess in der Liste
static PCB *current = NULL;  // aktuell laufender Prozess

static uint8_t get_quantum(uint8_t priority){
    if( priority == 0) return 4;
    if( priority == 1) return 3;                   //wenn priority gleich 1 ist bekommt programm 3 sekunden cpu zeit
    if( priority == 2) return 2;
    return 1;                                      //Wenn priority über 3 ist
}


void schedular_init(){
    head = NULL;
    current = NULL;
}


int stop = 0;

void scheduler_add(char *name, void *entry, uint8_t priority) {
    PCB *neu = create_process(name, entry, priority);
        if(!neu) return;

kprintf("ADD %s: pid=%d stack=%x rip=%x\n", 
        name, neu->pid, neu->stack, neu->regs.rip);
    

        neu->ticks_remaining = get_quantum(priority);

            if(head == NULL) {
                current = neu;
                head = neu; 
                neu->next = neu;
                neu->state = RUNNING;
                return;
            } 
             else {
                // bis zum letzten Element gehen
                PCB *temp = head;
                    while (temp->next != head) {
                        temp = temp->next;
                    }
                // temp ist jetzt das letzte Element
                temp->next = neu;   // neuen anhängen
                neu->next = head;   // circular schließen
                neu->state = READY;
        }
        kprintf("AFTER LINK: head=%x head->rip=%x\n",
            (uint64_t)head, head->regs.rip);
}

void schedular_remove(uint32_t pid) {
    if (!head) return;
    PCB *cur = head;
    do {
        if (cur->pid == pid) {
            kill_process(cur);
            return;
        }
        cur = cur->next;
    } while (cur != head);
}


PCB *get_current_process() {
    return current;
}







void process_yield(void) {
    PCB *p = get_current_process();
    if (!p) return;
    p->ticks_remaining = 0;
    __asm__ volatile("hlt");   /* wartet auf Timer Interrupt */
}



//looks evry second for a ZOMBIE programm 
void reaper_process(void) {


    /* BIOS: rotes R oben-links */
    volatile uint16_t *vga = (volatile uint16_t*)0xB8000;
    vga[0] = (uint16_t)'R' | (0x4F << 8);
    
    /* UEFI: rotes Rechteck oben-links */
    framebuffer_t *fb = fb_get();
    if (fb && fb->address) {
        fb_draw_rect(fb, 0, 0, 50, 50, 0x00FF0000);
    }



    kprintf("Reaper started!\n");
    while (1) { 
        kprintf("REAPER TICK\n");
        PCB *head = scheduler_get_head();
        PCB *current = get_current_process();
        
        if (head) {
            PCB *prev = head;
            PCB *cur = head->next;
            int safety = 100;
            
            while (cur != head && safety--) {
                if (cur->state == ZOMBIE && cur != current) {
                    kprintf ("Reaper cleaning pid %d\n", cur->pid);
                    PCB *to_free = cur;
                    prev->next = cur->next;
                    cur = prev->next;
                    pmm_free(to_free->stack);
                    kfree(to_free);
                } else {
                    prev = cur;
                    cur = cur->next;
                }
            }
        }
        sleep_ms(1000);
    }
}










void schedule(InterruptFrame *frame) {
    if(!current || !head) return;             //wenn kein head oder current dann nope 

    if(current->ticks_remaining > 0) {
        current->ticks_remaining--;
    }

    if(current->ticks_remaining > 0) return;        //hat noch ticks ürbig soll weiter laufen


    if(current->state == RUNNING) {
        current->state = READY;          
    }

    PCB *next = current->next;

    uint8_t checked = 0;
    uint8_t total = 0;

    PCB *temp = head;  // ← initialisieren!
    do { 
        total++; 
        temp = temp->next;  // ← = nicht vergessen!
    } while(temp != head);       //total counten

    while(next->state == ZOMBIE && checked < total) {
        next = next->next;
        checked++;
    }

    if(next->state == ZOMBIE) return;           //keine laufende prozesse gefunden

    current->regs = *frame;
    current = next;                                             //next prozess
    current->state = RUNNING;                                   //running 
    current->ticks_remaining = get_quantum(current->priority);  //tick bekommen

    *frame = current->regs;
    tss_set_kernel_stack(current->stack + 4096);



}

PCB *scheduler_get_head() {
return head;
}  