#ifndef UEFI_BOOT_H
#define UEFI_BOOT_H

#ifdef USE_GNU_EFI
  #include <efi.h>
  #include <efilib.h>
#else
  typedef unsigned char      UINT8;
  typedef unsigned short     UINT16;
  typedef unsigned int       UINT32;
  typedef unsigned long long UINT64;
  typedef unsigned long long UINTN;
  typedef unsigned long long EFI_STATUS;
  typedef void              *EFI_HANDLE;
  typedef unsigned short     CHAR16;
  typedef struct { UINTN _skip; } EFI_SYSTEM_TABLE;
#endif

/* ============================================================
 * BootInfo — muss EXAKT zu kernel.h passen!
 * 41 Bytes packed.
 * ============================================================ */

#define KERNEL_MAGIC 0xC0DEC0DE

typedef struct {
    UINT16 width;
    UINT16 height;
    UINT16 pitch;
    UINT8  bpp;
    UINT8  pixel_format;
} __attribute__((packed)) VideoMode;

typedef struct {
    UINT16 e820_count;          /* +0  */
    UINT32 e820_map_addr;       /* +2  */
    UINT32 total_ram_kb;        /* +6  */
    UINT8  ram_source;          /* +10 */
    UINT32 kernel_addr;         /* +11 */
    UINT8  kernel_mem_ok;       /* +15 */
    UINT8  cpu_flags;           /* +16 */
    UINT32 kernel_size;         /* +17 */
    UINT32 fb_addr;             /* +21 */
    UINT16 fb_width;            /* +25 */
    UINT16 fb_height;           /* +27 */
    UINT16 fb_pitch;            /* +29 */
    UINT8  fb_bpp;              /* +31 */
    UINT8  boot_type;           /* +32 */
    UINT32 video_modes_addr;    /* +33 */
    UINT16 video_modes_count;   /* +37 */
    UINT16 current_mode_idx;    /* +39 */
} __attribute__((packed)) BootInfo;

_Static_assert(sizeof(BootInfo) == 41, "BootInfo muss 41 Bytes sein!");

#define CPU_HAS_LONGMODE  (1 << 0)
#define CPU_HAS_NX        (1 << 1)
#define CPU_HAS_SSE       (1 << 2)
#define CPU_HAS_SSE2      (1 << 3)

typedef struct {
    UINT64 base;
    UINT64 length;
    UINT32 type;
    UINT32 attrib;
} __attribute__((packed)) E820Entry;

#define E820_FREE       1
#define E820_RESERVED   2
#define E820_ACPI       3
#define E820_NVS        4
#define E820_DEFEKT     5

#endif /* UEFI_BOOT_H */