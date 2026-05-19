#ifndef PMM_H
#define PMM_H
#include "../kernel/kernel.h"

#define PAGE_SIZE 4096  // ← Größe einer Page

// Was brauchst du noch?
// 1. Funktion die eine freie Page alloziert
// 2. Funktion die eine Page freigibt
// 3. Funktion die die PMM initialisiert mit der E820-Map

void pmm_init(BootInfo *info);  
void pmm_free(uint64_t adresse);
void pmm_reserve_range(uint64_t addr, uint64_t length);
uint64_t pmm_alloc();              //gibt adresse zurück adressen sind immer 64bits
uint64_t pmm_get_free_pages();
uint64_t pmm_get_total_pages();

// kmalloc braucht das um zu wissen wo der Heap anfangen darf.
extern uint64_t pmm_bitmap_end;

#endif