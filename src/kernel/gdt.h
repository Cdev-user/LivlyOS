// gdt.h
#ifndef GDT_H
#define GDT_H

#include "kernel.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) GDTEntry;

// TSS ist in 64-Bit 16 Bytes groß (zwei GDT-Slots!)
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) TSSEntry;

// Task State Segment — braucht der CPU für Ring-Wechsel
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;       // Kernel-Stack wenn aus Ring 3 → Ring 0 gewechselt wird
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];     // Interrupt Stack Table (für kritische Exceptions)
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) TSS;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) GDTR;

// Selektoren — jeder Eintrag ist 8 Bytes
// Bits 0-1 = RPL (Requested Privilege Level)
#define SEL_KERNEL_CODE  0x08   // Ring 0 Code
#define SEL_KERNEL_DATA  0x10   // Ring 0 Data
#define SEL_RING1_CODE   0x18   // Ring 1 Code  (KI architektur)
#define SEL_RING1_DATA   0x20   // Ring 1 Data  (KI architektur)
#define SEL_USER_CODE    0x28   // Ring 3 Code  → RPL muss 3 sein: 0x28 | 3 = 0x2B
#define SEL_USER_DATA    0x30   // Ring 3 Data  → RPL muss 3 sein: 0x30 | 3 = 0x33
#define SEL_TSS          0x38   // TSS (belegt 2 Slots → nächster Selektor wäre 0x48)

void gdt_init();
void tss_set_kernel_stack(uint64_t rsp0);  // wird beim Task-Wechsel aufgerufen

#endif