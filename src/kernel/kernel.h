#ifndef KERNEL_H
#define KERNEL_H
#include "../lib/stlib/types.h"


// ============================================================
// Kernel-Header
// Liegt bei 0x100000 - Stage2 liest ihn vor dem Start
//
// WICHTIG: Muss exakt 32 Bytes groß sein damit Kernel_main
// bei einer bekannten Adresse (0x100020) anfängt
// ============================================================
#define KERNEL_MAGIC 0xC0DEC0DE     // Stage2 prüft genau diesen Wert

typedef struct {
    uint32_t magic;         // +0  muss 0xC0DEC0DE sein
    uint32_t kernel_size;   // +4  Größe in Bytes (Linker schreibt das)
    uint32_t entry_point;   // +8  Adresse von Kernel_main (Linker schreibt das)
    uint32_t version;       // +12 z.B. 0x00000001 = Version 0.0.0.1
    uint32_t checksum;      // +16 momentan 0, später berechnen
    uint8_t  reserved[12];  // +20 Auffüllen auf 32 Bytes
                            // Gesamt: 4+4+4+4+4+12 = 32 Bytes
} __attribute__((packed)) KernelHeader;

/*
 * BootInfo — muss zu stage2.asm ([BOOT_INFO+]) und UEFI uefi_boot.c passen.
 * sizeof == 33 (packed). Offsets = Byte-Index ab 0x8F00 im BIOS-Pfad.
 */
#define BOOT_INFO_OFF_E820_COUNT    0
#define BOOT_INFO_OFF_E820_MAP     2
#define BOOT_INFO_OFF_TOTAL_RAM_KB 6
#define BOOT_INFO_OFF_RAM_SOURCE   10
#define BOOT_INFO_OFF_KERNEL_ADDR  11
#define BOOT_INFO_OFF_KERNEL_MEM_OK 15
#define BOOT_INFO_OFF_CPU_FLAGS    16
#define BOOT_INFO_OFF_KERNEL_SIZE  17
#define BOOT_INFO_OFF_FB_ADDR      21
#define BOOT_INFO_OFF_FB_WIDTH     25
#define BOOT_INFO_OFF_FB_HEIGHT    27
#define BOOT_INFO_OFF_FB_PITCH     29
#define BOOT_INFO_OFF_FB_BPP       31
#define BOOT_INFO_OFF_BOOT_TYPE    32

typedef struct {
    uint16_t e820_count;        /* +0 */
    uint32_t e820_map_addr;     /* +2 */
    uint32_t total_ram_kb;      /* +6 */
    uint8_t  ram_source;        /* +10 Bit0 E820, Bit1 E801, Bit2 INT12 (BIOS); UEFI: 0x01 */
    uint32_t kernel_addr;       /* +11 typ. 0x00100000 */
    uint8_t  kernel_mem_ok;     /* +15 */
    uint8_t  cpu_flags;         /* +16 siehe CPU_HAS_* */
    uint32_t kernel_size;       /* +17 Bytes (wie patch_header) */

    uint32_t fb_addr;           /* +21 */
    uint16_t fb_width;          /* +25 */
    uint16_t fb_height;         /* +27 */
    uint16_t fb_pitch;          /* +29 */
    uint8_t  fb_bpp;            /* +31 */

    uint8_t  boot_type;         /* +32 0=BIOS (stage2), 1=UEFI */

    uint32_t video_modes_addr;  /* +33 */
    uint16_t video_modes_count; /* +37 */
    uint16_t current_mode_idx;  /* 41*/

} __attribute__((packed)) BootInfo;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(BootInfo) == 41, "BootInfo muss 41 Bytes sein (stage2/UEFI)");
#endif

// cpu_flags Bits - kannst du im Kernel so prüfen:
// if (info->cpu_flags & CPU_HAS_NX) { ... }
#define CPU_HAS_LONGMODE  (1 << 0)
#define CPU_HAS_NX        (1 << 1)
#define CPU_HAS_SSE       (1 << 2)
#define CPU_HAS_SSE2      (1 << 3)


// E820-Eintrag - für wenn du die Map durchgehst
// Liegt bei info->e820_map_addr, je 24 Bytes pro Eintrag
typedef struct {
    uint64_t base;      // Startadresse
    uint64_t length;    // Länge in Bytes
    uint32_t type;      // 1=frei, 2=reserviert, 3=ACPI, 4=NVS
    uint32_t attrib;    // ACPI 3.0 Attribut
} __attribute__((packed)) E820Entry;

// E820 Typen
#define E820_FREE       1
#define E820_RESERVED   2
#define E820_ACPI       3
#define E820_NVS        4
#define E820_DEFEKT     5


//Video info from GOP (Graphics Output Protocol)
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;        // Bytes per scanline
    uint8_t  bpp;          // Bits per pixel
    uint8_t  pixel_format; // 0=RGBR, 1=BGRR, 2=MASK, 3=BLT
} __attribute__((packed)) VideoMode;

void sleep_ms(uint64_t ms);

#endif