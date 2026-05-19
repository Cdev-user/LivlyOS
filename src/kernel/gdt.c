// gdt.c
#include "gdt.h"

// 7 normale Einträge + 1 TSS-Eintrag (der zählt als 2)
static GDTEntry gdt[8];
static TSSEntry tss_entry;    // TSS-Deskriptor in der GDT (16 Byte)
static TSS      tss;          // Das eigentliche TSS im Speicher
static GDTR     gdtr;

// Hilfsfunktion — normalen Deskriptor setzen
static void gdt_set_entry(int index, uint8_t access, uint8_t granularity) {
    gdt[index].limit_low   = 0xFFFF;
    gdt[index].base_low    = 0;
    gdt[index].base_mid    = 0;
    gdt[index].access      = access;
    gdt[index].granularity = granularity;
    gdt[index].base_high   = 0;
}

// TSS-Deskriptor setzen (64-Bit = 16 Bytes = 2 GDT-Slots)
static void gdt_set_tss(uint64_t base, uint32_t limit) {
    tss_entry.limit_low   = (uint16_t)(limit & 0xFFFF);
    tss_entry.base_low    = (uint16_t)(base & 0xFFFF);
    tss_entry.base_mid    = (uint8_t)((base >> 16) & 0xFF);
    tss_entry.access      = 0x89;   // P=1, DPL=0, Type=9 (64-Bit TSS available)
    tss_entry.granularity = 0x00;
    tss_entry.base_high   = (uint8_t)((base >> 24) & 0xFF);
    tss_entry.base_upper  = (uint32_t)(base >> 32);
    tss_entry.reserved    = 0;

    // TSS-Deskriptor sitzt an Index 7 (= Offset 0x38)
    // Da er 16 Bytes groß ist, schreiben wir ihn direkt als TSSEntry
    // in den Speicher wo Index 7+8 sitzen würden
    *((TSSEntry*)(&gdt[7])) = tss_entry;
}

void gdt_init() {
    // Index 0: Null (CPU-Pflicht)
    gdt[0] = (GDTEntry){0};

    //                        access  gran
    // Index 1: Ring 0 Code   0x9A    0xA0  → P=1 DPL=0 S=1 E=1 RW=1 | G=1 L=1
    gdt_set_entry(1, 0x9A, 0xA0);

    // Index 2: Ring 0 Data   0x92    0xA0  → P=1 DPL=0 S=1 E=0 RW=1
    gdt_set_entry(2, 0x92, 0xA0);

    // Index 3: Ring 1 Code   0xBA    0xA0  → DPL=1 (Bits 6-5 = 01)
    gdt_set_entry(3, 0xBA, 0xA0);

    // Index 4: Ring 1 Data   0xB2    0xA0
    gdt_set_entry(4, 0xB2, 0xA0);

    // Index 5: Ring 3 Code   0xFA    0xA0  → DPL=3 (Bits 6-5 = 11)
    gdt_set_entry(5, 0xFA, 0xA0);

    // Index 6: Ring 3 Data   0xF2    0xA0
    gdt_set_entry(6, 0xF2, 0xA0);

    // Index 7+8: TSS (16 Byte Deskriptor)
    // TSS nullen
    TSS *t = &tss;
    for (int i = 0; i < (int)sizeof(TSS); i++)
        ((uint8_t*)t)[i] = 0;
    tss.iomap_base = sizeof(TSS);  // kein I/O-Map

    gdt_set_tss((uint64_t)&tss, sizeof(TSS) - 1);

    // GDTR aufbauen
    // sizeof(gdt) = 7 × 8 Bytes normale Einträge
    // + 16 Bytes TSS = 72 Bytes gesamt
    gdtr.limit = (7 * sizeof(GDTEntry)) + sizeof(TSSEntry) - 1;
    gdtr.base  = (uint64_t)&gdt[0];

    __asm__ volatile(
        "lgdt %0\n\t"
        "pushq $0x08\n\t"               // Ring 0 Code Selektor
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"                     // Far Return → CS = 0x08
        "1:\n\t"
        "movw $0x10, %%ax\n\t"          // Ring 0 Data
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%ss\n\t"
        "xorw %%ax, %%ax\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        // TSS laden
        "movw $0x38, %%ax\n\t"
        "ltr %%ax\n\t"                  // TSS in TR-Register laden
        : : "m"(gdtr) : "rax", "memory"
    );
}

// Wird beim Kontextwechsel aufgerufen:
// Setzt den Kernel-Stack den die CPU beim Ring-3 → Ring-0 Wechsel benutzt
void tss_set_kernel_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}