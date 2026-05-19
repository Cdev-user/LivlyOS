# LivlyOS

A 64-bit operating system built from scratch, supporting both Legacy BIOS and UEFI boot modes.

## Overview

LivlyOS is an educational OS project focused on understanding and implementing core kernel concepts including process scheduling, memory management, interrupt handling, and driver support.

## Features

- Dual bootloader support (BIOS and UEFI)
- 64-bit kernel architecture
- Process scheduler with state management
- Memory management (Physical and Virtual)
- Interrupt and exception handling
- Keyboard driver
- VGA and framebuffer graphics support
- Basic shell interface
- FAT filesystem support
- ATA driver

## Building

Prerequisites:
- `x86_64-linux-gnu-gcc` and `x86_64-linux-gnu-ld`
- `nasm` (Netwide Assembler)
- `python3`
- `mformat`, `mmd`, `mcopy` (from mtools)
- `sgdisk`

Build the disk image:

```bash
make all 
```
Build the vdi image for VM:

```bash
"/c/Program Files/Oracle/VirtualBox/VBoxManage.exe" convertfromraw build/disk.img build/livlyOs.vdi --format VDI
```
The file VBoxManage.exe may lies in a other path.


Clean build artifacts:

```bash
make clean
```

## Project Structure

```
src/
  boot/          - Bootloader code (BIOS and UEFI)
  kernel/        - Core kernel components
  drivers/       - Device drivers
  mm/            - Memory management (PMM, VMM, kmalloc)
  proc/          - Process management and scheduling
  Apps/          - User-space applications
tools/           - Build utilities
```

## Current Status

The project is in active development. See `Improvements.md` for the current roadmap and planned features.
