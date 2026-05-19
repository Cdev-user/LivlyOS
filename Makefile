SHELL := /usr/bin/bash
CC      = x86_64-linux-gnu-gcc
LD      = x86_64-linux-gnu-ld
ASM     = nasm
OBJCOPY = x86_64-linux-gnu-objcopy
NM      := $(patsubst %objcopy,%nm,$(OBJCOPY))
PYTHON  ?= python3


ASMFLAGS = -f bin
CFLAGS   = -ffreestanding -nostdlib -nostdinc -m64 -c -I src/kernel \
           -Wall -Wextra -Wno-unused-parameter \
           -Werror=implicit-function-declaration \
           -Werror=return-type
LDFLAGS  = -T linker.ld -nostdlib

# ─── UEFI ─────────────────────────────────────────────────
GNUEFI_DIR := C:/dev/gnu-efi

EFI_CC := x86_64-linux-gnu-gcc
EFI_CFLAGS := -ffreestanding -fPIE -fshort-wchar \
              -fno-stack-check -fno-stack-protector \
              -mno-red-zone -mno-avx \
              -maccumulate-outgoing-args \
              -fno-strict-aliasing -fno-merge-all-constants \
              -funsigned-char -std=c11 \
              -Wno-pointer-sign -Wno-error=pragmas \
              -IC:/dev/gnu-efi/inc \
              -IC:/dev/gnu-efi/inc/x86_64 \
              -IC:/dev/gnu-efi/inc/protocol \
              -DGNU_EFI_USE_MS_ABI -DUSE_GNU_EFI \
              -DCONFIG_x86_64 \
              -O2 -Wall

EFI_LD      := x86_64-linux-gnu-ld
EFI_LDFLAGS := -nostdlib -z nocombreloc \
               -T $(GNUEFI_DIR)/gnuefi/elf_x86_64_efi.lds \
               -shared -Bsymbolic
EFI_LIBS    := -L $(GNUEFI_DIR)/x86_64/lib \
               -L $(GNUEFI_DIR)/x86_64/gnuefi \
               -lefi -lgnuefi

DISK_SECTORS := 204800
KERNEL_SECTOR = 128

.PHONY: all clean

all: build/disk.img

# ─── BIOS Bootloader ──────────────────────────────────────
build/stage1.bin: src/boot/stage1.asm
	$(ASM) $(ASMFLAGS) $< -o $@

build/stage2.bin: src/boot/stage2.asm src/boot/config.asm
	$(ASM) $(ASMFLAGS) src/boot/stage2.asm -o $@

# ─── UEFI ─────────────────────────────────────────────────
build/uefi_boot.o: src/boot/UEFI/uefi_boot.c src/boot/UEFI/uefi_boot.h
	$(EFI_CC) $(EFI_CFLAGS) -c $< -o $@

build/uefi_boot.so: build/uefi_boot.o
	$(EFI_LD) $(EFI_LDFLAGS) \
	    $(GNUEFI_DIR)/x86_64/gnuefi/crt0-efi-x86_64.o $< \
	    -o $@ $(EFI_LIBS)

build/BOOTX64.EFI: build/uefi_boot.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .rodata \
	           -j .rel -j .rela -j .rel.* -j .rela.* \
	           -j .rel* -j .rela* -j .areloc -j .reloc \
	           --target=efi-app-x86_64 $< $@

# ─── Kernel Objekte ───────────────────────────────────────
build/kernel_objs/interrupts.o: src/boot/interrupts.asm
	$(ASM) -f elf64 $< -o $@

build/kernel_objs/idt.o:         src/kernel/idt.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/gdt.o:         src/kernel/gdt.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/vga.o:         src/kernel/vga.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/timer.o:       src/kernel/timer.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/keyboard.o:    src/drivers/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/pmm.o:         src/mm/pmm.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/vmm.o:         src/mm/vmm.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/kmalloc.o:     src/mm/kmalloc.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/process.o:     src/proc/process.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/schedular.o:   src/proc/schedular.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/shell.o:       src/kernel/shell/shell.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/ata.o:         src/drivers/fs/ata.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/fat.o:         src/drivers/fs/fat.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/stress_test.o: src/kernel/stress_test.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/editor.o:      src/Apps/Editor/editor.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/framebuffer.o: src/drivers/graphics/framebuffer.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/font.o: src/drivers/graphics/font.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/fb_console.o: src/drivers/graphics/fb_console.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/console.o: src/kernel/console.c
	$(CC) $(CFLAGS) $< -o $@

build/kernel_objs/kernel.o:      src/kernel/kernel.c
	$(CC) $(CFLAGS) $< -o $@

# ─── Kernel linken ────────────────────────────────────────
KERNEL_OBJS = \
    build/kernel_objs/kernel.o      \
    build/kernel_objs/idt.o         \
    build/kernel_objs/interrupts.o  \
    build/kernel_objs/gdt.o         \
    build/kernel_objs/vga.o         \
    build/kernel_objs/timer.o       \
    build/kernel_objs/keyboard.o    \
    build/kernel_objs/pmm.o         \
    build/kernel_objs/vmm.o         \
    build/kernel_objs/kmalloc.o     \
    build/kernel_objs/process.o     \
    build/kernel_objs/schedular.o   \
    build/kernel_objs/shell.o       \
    build/kernel_objs/ata.o         \
    build/kernel_objs/fat.o         \
    build/kernel_objs/stress_test.o \
    build/kernel_objs/editor.o		\
	build/kernel_objs/framebuffer.o \
	build/kernel_objs/fb_console.o	\
	build/kernel_objs/console.o

build/kernel.elf: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) -o $@

build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) tools/patch_header.py $@ $< "$(NM)"
	@touch $@

# ─── Multiboot Disk Image (BIOS + UEFI) ───────────────────
build/disk.img: build/stage1.bin build/stage2.bin build/BOOTX64.EFI build/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=$(DISK_SECTORS) status=none
	sgdisk -n 1:2048:0 -t 1:EF00 -c 1:"ESP" $@
	mformat -i $@@@1048576 -F ::
	mmd   -i $@@@1048576 ::/EFI
	mmd   -i $@@@1048576 ::/EFI/BOOT
	mcopy -i $@@@1048576 build/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@@@1048576 build/kernel.bin  ::/EFI/BOOT/kernel.bin
	dd if=build/stage1.bin of=$@ bs=1 count=446 conv=notrunc status=none
	dd if=build/stage2.bin of=$@ bs=512 seek=34 conv=notrunc status=none
	dd if=build/kernel.bin of=$@ bs=512 seek=$(KERNEL_SECTOR) conv=notrunc status=none
	@echo ">>> Disk image created successfully"

# ─── Clean ────────────────────────────────────────────────
clean:
	rm -f build/stage1.bin build/stage2.bin
	rm -f build/kernel.elf build/kernel.bin
	rm -f build/disk.img build/disk.vdi
	rm -f build/uefi_boot.o build/uefi_boot.so build/BOOTX64.EFI
	rm -f $(KERNEL_OBJS)
