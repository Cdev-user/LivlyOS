#include "../kernel/kernel.h"
#include "vmm.h"
#include "pmm.h"

// PML4 Tabelle — die oberste Ebene
static uint64_t *pml4 = (uint64_t*)0x1000;  // aus Stage2

// Aus virtueller Adresse die Indizes berechnen
static uint64_t pml4_index(uint64_t virt) {
    return (virt >> 39) & 0x1FF;  // Bits 47-39
}
static uint64_t pdpt_index(uint64_t virt) {
    return (virt >> 30) & 0x1FF;  // Bits 38-30
}
static uint64_t pd_index(uint64_t virt) {
    return (virt >> 21) & 0x1FF;  // Bits 29-21
}
static uint64_t pt_index(uint64_t virt) {
    return (virt >> 12) & 0x1FF;  // Bits 20-12
}

void map_page(uint64_t virt, uint64_t phys,uint64_t flags){

    uint64_t i4 = pml4_index(virt);  // PML4 Index
    uint64_t i3 = pdpt_index(virt);  // PDPT Index
    uint64_t i2 = pd_index(virt);    // PD Index
    uint64_t i1 = pt_index(virt);    // PT Index


    /*
    pml4[i4]  → zeigt auf PDPT  (i4 = PML4 Index)
    pdpt[i3]  → zeigt auf PD    (i3 = PDPT Index)
    pd[i2]    → zeigt auf PT    (i2 = PD Index)
    pt[i1]    → physische Adresse (i1 = PT Index)*/

      // ← i4!
    // RICHTIGE Reihenfolge:
if (!(pml4[i4] & PAGE_PRESENT)) {
    uint64_t neu = pmm_alloc();  
    pml4[i4] = neu | PAGE_PRESENT | PAGE_WRITE;
}
uint64_t *pdpt = (uint64_t*)(pml4[i4] & ~0xFFF);

if (!(pdpt[i3] & PAGE_PRESENT)) {  
    uint64_t neu = pmm_alloc();  
    pdpt[i3] = neu | PAGE_PRESENT | PAGE_WRITE;
}
uint64_t *pd = (uint64_t*)(pdpt[i3] & ~0xFFF); 

if (!(pd[i2] & PAGE_PRESENT)) { 
    uint64_t neu = pmm_alloc();  
    pd[i2] = neu | PAGE_PRESENT | PAGE_WRITE;
    
}

uint64_t *pt = (uint64_t*)(pd[i2] & ~0xFFF);

pt[i1] = phys | flags; // LETZTER Schritt

}

void unmap_page(uint64_t virt) {
    // Indizes berechnen — kennst du schon
    // Bis PT navigieren — kennst du schon
    // Eintrag löschen:

    uint64_t i4 = pml4_index(virt);  // PML4 Index
    uint64_t i3 = pdpt_index(virt);  // PDPT Index
    uint64_t i2 = pd_index(virt);    // PD Index
    uint64_t i1 = pt_index(virt);    // PT Index

uint64_t *pdpt = (uint64_t*)(pml4[i4] & ~0xFFF);
uint64_t *pd = (uint64_t*)(pdpt[i3] & ~0xFFF); 
uint64_t *pt = (uint64_t*)(pd[i2] & ~0xFFF);
    pt[i1] = 0;
    // TLB flush:
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_init(BootInfo *info) {
    if (info->boot_type == 0) {
        // BIOS: Stage2 hat Paging-Tabellen bei 0x1000 aufgebaut
        __asm__ volatile("mov %0, %%cr3" : : "r"((uint64_t)pml4) : "memory");
    }
    // UEFI: UEFI hat eigenes Paging aktiv CR3 nicht anfassen
}

uint64_t virt_to_phys(uint64_t virt){
    uint64_t i4 = pml4_index(virt);  // PML4 Index
    uint64_t i3 = pdpt_index(virt);  // PDPT Index
    uint64_t i2 = pd_index(virt);    // PD Index
    uint64_t i1 = pt_index(virt);    // PT Index

    if(!(pml4[i4] & PAGE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t*)(pml4[i4] & ~0xFFF);        //man geht alle durch
    if(!(pdpt[i3] & PAGE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t*)(pdpt[i3] & ~0xFFF);
    if(!(pd[i2] & PAGE_PRESENT)) return 0; 
    uint64_t *pt = (uint64_t*)(pd[i2] & ~0xFFF);

    return (pt[i1] & ~0xFFF) + (virt & 0xFFF);
//      ↑ Page-Adresse      ↑ Offset innerhalb der Page

    
}