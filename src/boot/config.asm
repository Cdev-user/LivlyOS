; ============================================================
; src/boot/config.asm - Zentrale Konfiguration
; Diese Datei wird von stage2.asm per %include eingebunden
; ============================================================
;
; KERNEL_START_SECTOR: im DAP steht LBA = KERNEL_START_SECTOR - 1
; Muss zu KERNEL_SECTOR im Makefile passen: KERNEL_START_SECTOR = KERNEL_SECTOR + 1
;
KERNEL_START_SECTOR equ 129

KERNEL_LOAD_ADDRESS equ 0x100000   ; Kernel wird an 1MB geladen
KERNEL_MAX_SECTORS  equ 128        ; max 32KB Kernel (für jetzt)
