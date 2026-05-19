/*
 * LivlyOS UEFI-Bootloader
 */

#ifndef USE_GNU_EFI
#error "Bitte mit -DUSE_GNU_EFI bauen."
#endif

#include "uefi_boot.h"
#include <efiprot.h>

#define KERNEL_LOAD_PHYS   0x100000ULL
#define KERNEL_FILE_PATH   L"\\EFI\\BOOT\\kernel.bin"
#define E820_MAX_ENTRIES   128
#define KERNEL_MAX_PAGES   2048

#define KERNEL_HDR_MAGIC_OFF   0
#define KERNEL_HDR_SIZE_OFF    1
#define KERNEL_HDR_ENTRY_OFF   2

static void __attribute__((noreturn))
jump_to_kernel(UINT64 entry, BootInfo *info)
{
    __asm__ __volatile__(
        "cli\n\t"
        "mov %1, %%rdi\n\t"
        "xor %%rsi, %%rsi\n\t"
        "xor %%rdx, %%rdx\n\t"
        "jmp *%0\n\t"
        : : "r"(entry), "r"(info) : "memory"
    );
    while (1);
}




static UINT32 uefi_type_to_e820(UINT32 t)
{
    switch (t) {
        case EfiConventionalMemory:   return E820_FREE;
        case EfiACPIReclaimMemory:    return E820_ACPI;
        case EfiACPIMemoryNVS:        return E820_NVS;
        case EfiUnusableMemory:       return E820_DEFEKT;
        default:                      return E820_RESERVED;
    }
}

static EFI_HANDLE find_our_image_handle(void)
{
    EFI_GUID li_guid = LOADED_IMAGE_PROTOCOL;
    EFI_HANDLE *handles = NULL;
    UINTN num = 0;

    EFI_STATUS st = BS->LocateHandleBuffer(ByProtocol, &li_guid,
                                            NULL, &num, &handles);
    if (EFI_ERROR(st) || num == 0) return NULL;
    Print(L"%u LoadedImage Handles\r\n", (UINTN)num);

    UINT64 our_addr = (UINT64)(UINTN)&find_our_image_handle;

    for (UINTN i = 0; i < num; i++) {
        EFI_LOADED_IMAGE_PROTOCOL *li = NULL;
        if (EFI_ERROR(BS->HandleProtocol(handles[i], &li_guid, (void**)&li)))
            continue;
        UINT64 base = (UINT64)(UINTN)li->ImageBase;
        UINT64 size = (UINT64)li->ImageSize;
        if (our_addr >= base && our_addr < base + size) {
            Print(L"Unser Handle: #%u\r\n", (UINTN)i);
            return handles[i];
        }
    }
    return handles[0];
}

static EFI_STATUS find_kernel_fs(EFI_FILE **out_root)
{
    EFI_GUID fs_guid = SIMPLE_FILE_SYSTEM_PROTOCOL;
    EFI_HANDLE *handles = NULL;
    UINTN num_handles = 0;

    EFI_STATUS st = BS->LocateHandleBuffer(ByProtocol, &fs_guid,
                                            NULL, &num_handles, &handles);
    if (EFI_ERROR(st)) return st;
    Print(L"FileSystems: %u\r\n", (UINTN)num_handles);

    for (UINTN i = 0; i < num_handles; i++) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *vol = NULL;
        st = BS->HandleProtocol(handles[i], &fs_guid, (void**)&vol);
        if (EFI_ERROR(st)) continue;

        EFI_FILE *root = NULL;
        st = vol->OpenVolume(vol, &root);
        if (EFI_ERROR(st)) continue;

        EFI_FILE *file = NULL;
        st = root->Open(root, &file, KERNEL_FILE_PATH, EFI_FILE_MODE_READ, 0);
        if (!EFI_ERROR(st)) {
            file->Close(file);
            Print(L"kernel.bin auf FS #%u\r\n", (UINTN)i);
            *out_root = root;
            return EFI_SUCCESS;
        }
        root->Close(root);
    }
    return EFI_NOT_FOUND;
}

/* Sammelt alle GOP Modi in einen Buffer, schreibt Pointer+Count in BootInfo */
static void collect_gop_modes(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, BootInfo *info)
{
    UINTN max_mode = gop->Mode->MaxMode;
    
    /* Buffer für alle Modi allokieren */
    VideoMode *modes = NULL;
    EFI_STATUS st = BS->AllocatePool(EfiLoaderData,
                                      sizeof(VideoMode) * max_mode,
                                      (void **)&modes);
    if (EFI_ERROR(st)) {
        Print(L"Mode-Buffer Alloc failed\r\n");
        return;
    }
    
    UINTN count = 0;
    for (UINTN i = 0; i < max_mode; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mi = NULL;
        UINTN info_size = 0;
        
        st = gop->QueryMode(gop, i, &info_size, &mi);
        if (EFI_ERROR(st)) continue;
        
        modes[count].width  = (UINT16)mi->HorizontalResolution;
        modes[count].height = (UINT16)mi->VerticalResolution;
        modes[count].pitch  = (UINT16)(mi->PixelsPerScanLine * 4);
        modes[count].bpp    = 32;
        
        switch (mi->PixelFormat) {
            case PixelRedGreenBlueReserved8BitPerColor: modes[count].pixel_format = 0; break;
            case PixelBlueGreenRedReserved8BitPerColor: modes[count].pixel_format = 1; break;
            case PixelBitMask:                          modes[count].pixel_format = 2; break;
            case PixelBltOnly:                          modes[count].pixel_format = 3; break;
            default:                                    modes[count].pixel_format = 0xFF; break;
        }
        
        count++;
        BS->FreePool(mi);
    }
    
    info->video_modes_addr = (UINT32)(UINTN)modes;
    info->video_modes_count = (UINT16)count;
    info->current_mode_idx = (UINT16)gop->Mode->Mode;
    
    Print(L"%u GOP Modi gesammelt\r\n", (UINTN)count);
}


static EFI_STATUS read_kernel_file(EFI_FILE *root, void *dest, UINTN *out_size)
{
    EFI_FILE *f = NULL;
    EFI_STATUS st = root->Open(root, &f, KERNEL_FILE_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    UINTN maxb = KERNEL_MAX_PAGES * 4096;
    UINTN total = 0;
    for (;;) {
        UINTN chunk = maxb - total;
        if (chunk > 1024 * 1024) chunk = 1024 * 1024;
        if (chunk == 0) { f->Close(f); return EFI_LOAD_ERROR; }
        st = f->Read(f, &chunk, (UINT8 *)dest + total);
        if (EFI_ERROR(st)) { f->Close(f); return st; }
        if (chunk == 0) break;
        total += chunk;
    }
    f->Close(f);
    *out_size = total;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    Print(L"LivlyOS UEFI Bootloader\r\n");

    static UINT8 bootinfo_buf[64];
    BootInfo *info = (BootInfo *)bootinfo_buf;
    ZeroMem(info, sizeof(BootInfo));
    info->boot_type = 1;
    info->kernel_addr = (UINT32)KERNEL_LOAD_PHYS;
    info->kernel_mem_ok = 1;
    info->cpu_flags = (UINT8)(CPU_HAS_LONGMODE | CPU_HAS_SSE | CPU_HAS_SSE2);

    EFI_FILE *root = NULL;
    EFI_STATUS st = find_kernel_fs(&root);
    if (EFI_ERROR(st)) { while(1){} return st; }

    EFI_PHYSICAL_ADDRESS kphys = KERNEL_LOAD_PHYS;
    st = BS->AllocatePages(AllocateAddress, EfiLoaderData,
                            KERNEL_MAX_PAGES, &kphys);
    if (EFI_ERROR(st)) { Print(L"AllocPages (%r)\r\n", st); while(1){} return st; }

    void *kbuf = (void *)(UINTN)KERNEL_LOAD_PHYS;
    UINTN ksize = 0;
    st = read_kernel_file(root, kbuf, &ksize);
    if (EFI_ERROR(st)) { while(1){} return st; }

    /* GOP aktivieren + Modi auflisten */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS gst = BS->LocateProtocol(&gop_guid, NULL, (void **)&gop);
    if (!EFI_ERROR(gst) && gop && gop->Mode) {
        collect_gop_modes(gop, info);    /* ← NEU */
        
        info->fb_addr = (UINT32)(UINTN)gop->Mode->FrameBufferBase;
        info->fb_width = (UINT16)gop->Mode->Info->HorizontalResolution;
        info->fb_height = (UINT16)gop->Mode->Info->VerticalResolution;
        info->fb_pitch = (UINT16)(gop->Mode->Info->PixelsPerScanLine * 4);
        info->fb_bpp = 32;
        Print(L"FB: %ux%u @ 0x%x\r\n",
              (UINTN)info->fb_width, (UINTN)info->fb_height,
              (UINTN)info->fb_addr);
    }

    /* ...rest wie gehabt... */

    UINT32 *kh = (UINT32 *)kbuf;
    if (kh[KERNEL_HDR_MAGIC_OFF] != KERNEL_MAGIC) {
        Print(L"Magic falsch!\r\n"); while(1){} return EFI_LOAD_ERROR;
    }
    info->kernel_size = kh[KERNEL_HDR_SIZE_OFF];
    UINT64 entry = (UINT64)kh[KERNEL_HDR_ENTRY_OFF];
    Print(L"Entry: 0x%x\r\n", (UINTN)entry);

    E820Entry *emap = NULL;
    BS->AllocatePool(EfiLoaderData, sizeof(E820Entry) * E820_MAX_ENTRIES,
                      (void **)&emap);

    UINTN mmap_sz = 0, mmap_key = 0, desc_sz = 0;
    UINT32 desc_ver = 0;
    UINT8 *mmap_buf = NULL;

    BS->GetMemoryMap(&mmap_sz, NULL, &mmap_key, &desc_sz, &desc_ver);
    UINTN mmap_alloc = mmap_sz + 4 * desc_sz;
    BS->AllocatePool(EfiLoaderData, mmap_alloc, (void **)&mmap_buf);
    BS->GetMemoryMap(&mmap_sz, (EFI_MEMORY_DESCRIPTOR *)mmap_buf,
                      &mmap_key, &desc_sz, &desc_ver);

    UINTN nent = 0;
    UINT64 total_kb = 0;
    for (UINTN off = 0; off + desc_sz <= mmap_sz && nent < E820_MAX_ENTRIES;
         off += desc_sz)
    {
        EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)(mmap_buf + off);
        UINT64 len = d->NumberOfPages * 4096ULL;
        emap[nent].base = d->PhysicalStart;
        emap[nent].length = len;
        emap[nent].type = uefi_type_to_e820(d->Type);
        emap[nent].attrib = 0;
        if (emap[nent].type == E820_FREE) total_kb += len >> 10;
        nent++;
    }

    info->e820_count = (UINT16)nent;
    info->e820_map_addr = (UINT32)(UINTN)emap;
    info->total_ram_kb = (UINT32)(total_kb > 0xFFFFFFFFU ? 0xFFFFFFFFU : total_kb);
    info->ram_source = 0x01;
    Print(L"RAM: %u MB\r\n", (UINTN)(info->total_ram_kb / 1024));

    EFI_HANDLE real_ih = find_our_image_handle();
    if (!real_ih) { while(1){} return EFI_LOAD_ERROR; }

    mmap_sz = mmap_alloc;
    BS->GetMemoryMap(&mmap_sz, (EFI_MEMORY_DESCRIPTOR *)mmap_buf,
                      &mmap_key, &desc_sz, &desc_ver);

    Print(L"ExitBootServices...\r\n");
    st = BS->ExitBootServices(real_ih, mmap_key);
    if (EFI_ERROR(st)) {
        mmap_sz = mmap_alloc;
        BS->GetMemoryMap(&mmap_sz, (EFI_MEMORY_DESCRIPTOR *)mmap_buf,
                          &mmap_key, &desc_sz, &desc_ver);
        st = BS->ExitBootServices(real_ih, mmap_key);
        if (EFI_ERROR(st)) { while(1){} return st; }
    }

    jump_to_kernel(entry, info);
    return EFI_SUCCESS;
}