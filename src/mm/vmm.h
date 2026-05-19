#ifndef VMM_H
#define VMM_H
#include "../kernel/kernel.h"

// Page-Flags — was bedeuten die Bits in einem Page-Table-Eintrag?
#define PAGE_PRESENT  (1 << 0)  // Page ist vorhanden
#define PAGE_WRITE    (1 << 1)  // Schreiben erlaubt
#define PAGE_USER     (1 << 2)  // Ring 3 darf zugreifen

// Drei Funktionen brauchst du:
// 1. vmm_init()      — Paging aufsetzen
// 2. map_page()      — virtuelle Adresse → physische Adresse mappen
// 3. unmap_page()    — Mapping entfernen

void vmm_init(BootInfo *info);
void map_page(uint64_t virt, uint64_t phys,uint64_t flags);
void unmap_page(uint64_t virt);

uint64_t virt_to_phys(uint64_t virt);

#endif