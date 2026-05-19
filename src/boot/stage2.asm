[ORG 0x8000]
[BITS 16]
dw 0xB007                       ; Magic Number - Stage1 prüft ob das hier steht

%include "src/boot/config.asm"
; KERNEL_START_SECTOR = Kernel-LBA + 1 (siehe config.asm / Makefile)

; ========================================
; KONSTANTEN
; ========================================
E820_BUFFER     equ 0x9000
E820_COUNT      equ 0x8FFE
BOOT_INFO       equ 0x8F00
                                ; Layout:
                                ;   0x8F00 dw  - Anzahl E820-Einträge
                                ;   0x8F02 dd  - Adresse der E820-Map (= 0x9000)
                                ;   0x8F06 dd  - Gesamt-RAM in KB
                                ;   0x8F0A db  - Flags (Bit0=E820ok, Bit1=E801ok, Bit2=INT12ok)
                                ;   0x8F0B dd  - Kernel-Zieladresse (0x100000)
                                ;   0x8F0F db  - Kernel-Ziel ist freier RAM? (1=ja, 0=nein)
                                ;   0x8F10 db  - CPU-Flags
                                ;   0x8F11 dd  - Kernel-Größe in Bytes

GDT_CODE_SEL    equ 0x08
GDT_DATA_SEL    equ 0x10
GDT_CODE64_SEL  equ 0x18

PML4_ADDR       equ 0x1000
PDPT_ADDR       equ 0x2000
PD_ADDR         equ 0x3000

ERR_KEIN_CPUID          equ 0x01
ERR_CPU_ZU_ALT          equ 0x02
ERR_KEIN_LONG_MODE      equ 0x03
ERR_E820_FAIL           equ 0x10
ERR_KEIN_TYP1           equ 0x11
ERR_KERNEL_ZIEL_BELEGT  equ 0x12
ERR_ZU_WENIG_RAM        equ 0x13
ERR_KEINE_EINTRAEGE     equ 0x14
ERR_KERNEL_DISK         equ 0x20
ERR_KERNEL_MAGIC        equ 0x21
ERR_KERNEL_CHECKSUM     equ 0x22
ERR_KEIN_ENTRY          equ 0x40

BREADCRUMB_ADDR     equ 0x7FF0
BIOS_TICK_COUNTER   equ 0x046C
DISK_TIMEOUT_TICKS  equ 182

; ========================================
; BUGFIX 1: Makro MUSS vor start: stehen,
; sonst "macro not defined" beim ersten BREADCRUMB 0x01
; ========================================
%macro BREADCRUMB 1
    mov byte [BREADCRUMB_ADDR], %1
%endmacro

; ========================================
; START
; ========================================
start:
    mov al, [0x7000]
    mov [driveNumber], al

    mov ah, 0x00
    mov al, 0x03
    int 0x10

    BREADCRUMB 0x01

    call getE820map
    BREADCRUMB 0x02

    call checkCPU
    BREADCRUMB 0x03

    call loadKernel
    BREADCRUMB 0x05


    call setupGDT
    BREADCRUMB 0x06

    ;call setupVBE 

    call enterProtectedMode         ; kehrt NIEMALS zurück

; ========================================
; VARIABLEN
; ========================================
loopCounter     dw 0
edi_temp        dd 0
kernelSize      dd 0
kernelSectors   dw 0
kernelEntry     dd 0
esi_temp        dd 0
driveNumber     db 0
cpu_flags       db 0
cpu_max_func    dd 0
cpu_max_ext_func dd 0
disk_tick_start dd 0

; ========================================
; VBE Setup - mehrere Modi probieren
; ========================================
setupVBE:





.try_vbe



.no_vbe 



; ========================================
; LOOP DAP for LBA aka int 13 extention
; ========================================


loop_dap:
    db 0x10
    db 0
    dw 1              ; immer 1 Sektor
    dw 0x7E00
    dw 0x0000
    dq KERNEL_START_SECTOR - 1    ; LBA — wird im Loop erhöht
    
; ========================================
; E820 MEMORY MAP
; ========================================
getE820map:
    mov edi, E820_BUFFER
    xor ebx, ebx
    xor bp, bp
    xor ax, ax
    mov es, ax

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15

    jc  .e820_nicht_da
    cmp eax, 0x534D4150
    jne .e820_nicht_da

    inc bp
    add edi, 24

    test ebx, ebx
    jz  .analyse

.loop:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15

    jc  .loop_ende
    cmp eax, 0x534D4150
    jne .loop_ende

    inc bp
    add edi, 24

    test ebx, ebx
    jz  .analyse
    jmp .loop

.loop_ende:
    test bp, bp
    jz  .e820_nicht_da

.analyse:
    mov word [E820_COUNT], bp
    mov word [BOOT_INFO], bp
    mov dword [BOOT_INFO + 2],  E820_BUFFER
    mov dword [BOOT_INFO + 11], 0x100000
    mov byte  [BOOT_INFO + 10], 0x01

    mov si, msgE820ok
    call printString

    call analyseE820Map
    ret

.e820_nicht_da:
    mov si, msgE820fail
    call printString
    call fallback_e801
    ret

; ========================================
; E820-MAP ANALYSIEREN
; ========================================
analyseE820Map:
    mov byte [BOOT_INFO + 15], 0
    mov cx, [E820_COUNT]
    test cx, cx
    jz .keine_eintraege

    mov esi, E820_BUFFER
    xor edx, edx
    xor di, di

.schleife:
    mov eax, [esi + 16]
    cmp eax, 1
    jne .naechster

    inc di

    mov eax, [esi + 8]
    shr eax, 10
    add edx, eax

    mov eax, [esi + 0]
    cmp eax, 0x100000
    ja  .naechster

    mov ebx, [esi + 8]
    add ebx, eax
    cmp ebx, 0x100001
    jbe .naechster

    mov byte [BOOT_INFO + 15], 1

.naechster:
    add esi, 24
    loop .schleife

    mov dword [BOOT_INFO + 6], edx

    test di, di
    jz .kein_typ1

    mov si, msgTyp1ok
    call printString

    cmp byte [BOOT_INFO + 15], 1
    jne .kernel_ziel_fehler

    mov si, msgKernelZielOk
    call printString

    cmp edx, 1024
    jb  .zu_wenig_ram
    mov si, msgRamOk
    call printString
    ret

.zu_wenig_ram:
    mov si, msgZuWenigRam
    call printString
    mov al, ERR_ZU_WENIG_RAM
    call fatal_halt

.kein_typ1:
    mov si, msgKeinTyp1
    call printString
    mov al, ERR_KEIN_TYP1
    call fatal_halt

.kernel_ziel_fehler:
    mov si, msgKernelZielFehler
    call printString
    mov al, ERR_KERNEL_ZIEL_BELEGT
    call fatal_halt

.keine_eintraege:
    mov si, msgKeineEintraege
    call printString
    mov al, ERR_KEINE_EINTRAEGE
    call fatal_halt

; ========================================
; FALLBACK 1: INT 15h AX=E801h
; ========================================
fallback_e801:
    mov ax, 0xE801
    int 0x15
    jc  .e801_fehler

    test ax, ax
    jnz .e801_hat_ax
    mov ax, cx
    mov bx, dx

.e801_hat_ax:
    mov dword [E820_BUFFER + 0],  0
    mov dword [E820_BUFFER + 4],  0
    mov dword [E820_BUFFER + 8],  0x9F000
    mov dword [E820_BUFFER + 12], 0
    mov dword [E820_BUFFER + 16], 1
    mov dword [E820_BUFFER + 20], 1
    mov dword [E820_BUFFER + 24], 0x100000
    mov dword [E820_BUFFER + 28], 0
    movzx eax, ax
    shl eax, 10
    mov dword [E820_BUFFER + 32], eax
    mov dword [E820_BUFFER + 36], 0
    mov dword [E820_BUFFER + 40], 1
    mov dword [E820_BUFFER + 44], 1
    mov word [E820_COUNT], 2
    mov word [BOOT_INFO],  2
    or byte [BOOT_INFO + 10], 0x02
    mov si, msgE801ok
    call printString
    call analyseE820Map
    ret

.e801_fehler:
    mov si, msgE801fail
    call printString
    call fallback_int12
    ret

; ========================================
; FALLBACK 2: INT 12h
; ========================================
fallback_int12:
    int 0x12
    mov dword [E820_BUFFER + 0],  0
    mov dword [E820_BUFFER + 4],  0
    movzx eax, ax
    shl eax, 10
    mov dword [E820_BUFFER + 8],  eax
    mov dword [E820_BUFFER + 12], 0
    mov dword [E820_BUFFER + 16], 1
    mov dword [E820_BUFFER + 20], 1
    mov word [E820_COUNT], 1
    mov word [BOOT_INFO],  1
    or byte [BOOT_INFO + 10], 0x04
    mov si, msgInt12ok
    call printString
    call analyseE820Map
    ret

; ========================================
; CPU FEATURES DETECTION/CHECK
; ========================================
checkCPU:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x00200000
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    test eax, 0x00200000
    jz .kein_cpuid
    push ecx
    popfd

    mov eax, 0
    cpuid
    cmp eax, 1
    jb .cpuid_zu_alt
    mov [cpu_max_func], eax

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .kein_long_mode
    mov [cpu_max_ext_func], eax

    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .kein_long_mode
    or byte [cpu_flags], 0x01
    mov si, msgLongModeOk
    call printString

    test edx, (1 << 20)
    jz .nx_nicht_da
    or byte [cpu_flags], 0x02
    mov si, msgNXok
    call printString
    jmp .check_sse

.nx_nicht_da:
    mov si, msgNXfehlt
    call printString

.check_sse:
    cmp dword [cpu_max_func], 1
    jb .sse_nicht_da
    mov eax, 1
    cpuid
    test edx, (1 << 25)
    jz .sse_nicht_da
    or byte [cpu_flags], 0x04
    mov si, msgSSEok
    call printString
    test edx, (1 << 26)
    jz .cpu_check_fertig
    or byte [cpu_flags], 0x08
    mov si, msgSSE2ok
    call printString
    jmp .cpu_check_fertig

.sse_nicht_da:
    mov si, msgSSEfehlt
    call printString

.cpu_check_fertig:
    mov al, [cpu_flags]
    mov [BOOT_INFO + 16], al
    ret

.kein_cpuid:
    mov si, msgKeinCPUID
    call printString
    mov al, ERR_KEIN_CPUID
    call fatal_halt

.cpuid_zu_alt:
    mov si, msgCPUzuAlt
    call printString
    mov al, ERR_CPU_ZU_ALT
    call fatal_halt

.kein_long_mode:
    mov si, msgKeinLongMode
    call printString
    mov al, ERR_KEIN_LONG_MODE
    call fatal_halt

; ========================================
; KERNEL LADEN
; ========================================
KERNEL_MAGIC            equ 0xC0DEC0DE
KERNEL_DST              equ 0x100000
KERNEL_CHECKSUM_OFFSET  equ 16
loadKernel:
    mov si, msgKernelLaden
    call printString

    ; Schritt 1: ersten Sektor lesen für Magic-Check
    mov si, loop_dap
    mov ah, 0x42
    mov dl, [driveNumber]
    int 0x13
    jc .disk_fehler

    ; Schritt 2: Magic SOFORT prüfen
    cmp dword [0x7E00], KERNEL_MAGIC
    jne .magic_fehler

    mov si, msgMagicOk
    call printString

    ; Schritt 3: Header-Werte lesen
    mov eax, dword [0x7E04]
    mov [kernelSize], eax
    add eax, 511
    shr eax, 9
    mov [kernelSectors], ax

    mov eax, dword [0x7E08]
    mov [kernelEntry], eax

    ; Schritt 4: LBA für Loop vorbereiten
    inc dword [loop_dap + 8]

    ; Schritt 5: Unreal Mode aktivieren (FS bekommt 4GB Limit)
    mov si, msgUnrealStart
    call printString
    call enableUnrealMode
    mov si, msgUnrealOk
    call printString

    cli                         ;Fs cache protection

    ; Schritt 6: ersten Sektor nach 0x100000 kopieren
    xor ax, ax
    mov ds, ax
    mov esi, 0x7E00
    mov edi, KERNEL_DST
    mov ecx, 128
.first_copy:
    mov eax, dword [esi]
    mov dword [fs:edi], eax
    add esi, 4
    add edi, 4
    dec ecx
    jnz .first_copy

    ; Schritt 7: edi_temp und loopCounter setzen
    mov [edi_temp], edi
    mov ax, [kernelSectors]
    mov [loopCounter], ax
    dec word [loopCounter]    ; ersten Sektor schon kopiert

    BREADCRUMB 0x04

.lade_loop:
    sti
    call saveDiskTick

    mov si, loop_dap
    mov ah, 0x42
    mov dl, [driveNumber]
    int 0x13
    jc .disk_fehler
    call checkDiskTimeout
    cli
    xor ax, ax
    mov ds, ax
    mov esi, 0x7E00
    mov edi, [edi_temp]
    mov ecx, 128

.copy_inner:
    mov eax, dword [esi]
    mov dword [fs:edi], eax
    add esi, 4
    add edi, 4
    dec ecx
    jnz .copy_inner

    mov [edi_temp], edi
    inc dword [loop_dap + 8]

    dec word [loopCounter]
    jnz .lade_loop

    sti

    mov eax, [kernelSize]
    mov [BOOT_INFO + 17], eax

    mov ecx, [kernelSize]
    add ecx, 3
    shr ecx, 2
    mov edi, KERNEL_DST
    xor eax, eax

    ;jmp .checksum_skip    ; temporär!
    ;Note Klappt jetzt auch ohne

.checksum_loop:
    add eax, dword [fs:edi]
    add edi, 4
    dec ecx
    jnz .checksum_loop
    test eax, eax
    jnz .checksum_fehler

.checksum_skip:
    mov si, msgChecksumOk
    call printString
    mov si, msgKernelOk
    call printString
    ret

.checksum_fehler:
    mov si, msgChecksumFehler
    call printString
    mov al, ERR_KERNEL_CHECKSUM
    call fatal_halt

.disk_fehler:
    mov si, msgKernelDiskFehler
    call printString
    mov al, ERR_KERNEL_DISK
    call fatal_halt

.magic_fehler:
    mov si, msgKernelMagicFehler
    call printString
    mov al, ERR_KERNEL_MAGIC
    call fatal_halt

; ========================================
; UNREAL MODE aktivieren
; ========================================
; FIX #8: FS und GS werden jetzt ebenfalls mit dem 4GB-Deskriptor geladen.
; Nur DS wird nach dem Rücksprung in den Real Mode neu geladen
; (damit Real-Mode-Reads aus 0x7E00 funktionieren).
; ES, FS, GS behalten ihre PM-Deskriptoren → Unreal Mode für alle drei!
; WICHTIG: Nach diesem Call FS und GS NIE neu laden — sonst geht der
; Unreal Mode für den High-Memory-Copy verloren!
enableUnrealMode:
    cli
    lgdt [unreal_gdt_desc]
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax                ; PE=1, jetzt Protected Mode

    mov ax, 0x08
    mov ds, ax                  ; DS: Base=0, Limit=4GB (PM-Deskriptor)
    mov es, ax                  ; ES: Base=0, Limit=4GB
    mov fs, ax                  ; FIX: FS: Base=0, Limit=4GB
    mov gs, ax                  ; FIX: GS: Base=0, Limit=4GB

    and eax, 0xFFFFFFFE
    mov cr0, eax                ; PE=0, zurück zu Real Mode

    ; DS neu laden → Real-Mode-Deskriptor (Base=0, Limit=64KB)
    ; Das ist OK: Reads aus 0x7E00 brauchen kein 4GB-Limit.
    xor ax, ax
    mov ds, ax

    ; ES, FS, GS NICHT neu laden → behalten PM-Deskriptor (Unreal Mode)!

    sti
    ret

align 8
unreal_gdt:
    dq 0x0000000000000000
    dq 0x00CF92000000FFFF

unreal_gdt_desc:
    dw $ - unreal_gdt - 1
    dd unreal_gdt

; ========================================
; GDT - Global Descriptor Table
; ========================================
setupGDT:
    mov al, 0x80
    out 0x70, al
    cli
    lgdt [gdt_descriptor]
    mov si, msgGDTok
    call printString
    ret

align 8
gdt_start:

gdt_null:
    dq 0x0000000000000000

gdt_code:                       ; Selektor 0x08 – 32-Bit Code
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A
    db 0xCF
    db 0x00

gdt_data:                       ; Selektor 0x10 – 32-Bit Data
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x92
    db 0xCF
    db 0x00

gdt_code64:                     ; Selektor 0x18 – 64-Bit Long Mode
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 0x9A                     ; P=1, DPL=0, S=1, E=1, RW=1
    db 0xA0                     ; G=1, DB=0, L=1 ← Long Mode!
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ========================================
; FATAL HALT (Real Mode)
; ========================================
; IN: AL = Fehler-Code
fatal_halt:
    push ax

    mov si, msgFatalHalt
    call printString

    mov si, msgErrPrefix
    call printString

    pop ax
    mov ah, al
    shr al, 4
    call printHexNibble
    mov al, ah
    and al, 0x0F
    call printHexNibble

    mov si, msgCRLF
    call printString

    cli
.halt:
    hlt
    jmp .halt

printHexNibble:
    cmp al, 9
    jbe .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    ret

; ========================================
; DISK-TIMEOUT
; ========================================
saveDiskTick:
    push eax
    mov eax, dword [BIOS_TICK_COUNTER]
    mov dword [disk_tick_start], eax
    pop eax
    ret

checkDiskTimeout:
    push eax
    push ebx
    mov eax, dword [BIOS_TICK_COUNTER]
    mov ebx, dword [disk_tick_start]
    sub eax, ebx
    cmp eax, DISK_TIMEOUT_TICKS
    ja  .timeout
    pop ebx
    pop eax
    ret
.timeout:
    mov si, msgDiskTimeout
    call printString
    mov al, ERR_KERNEL_DISK
    call fatal_halt

; ========================================
; STRING AUSGABE (Real Mode)
; ========================================
printString:
.zeichen:
    lodsb
    test al, al
    jz .fertig
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .zeichen
.fertig:
    ret

; ========================================
; STRINGS
; ========================================
msgE820ok           db "E820 Map OK",               0x0D, 0x0A, 0
msgE820fail         db "E820 fehlgeschlagen...",     0x0D, 0x0A, 0
msgTyp1ok           db "Freier RAM gefunden",        0x0D, 0x0A, 0
msgKernelZielOk     db "Kernel-Ziel 0x100000 frei",  0x0D, 0x0A, 0
msgRamOk            db "RAM ausreichend",            0x0D, 0x0A, 0
msgZuWenigRam       db "FEHLER: Zu wenig RAM!",      0x0D, 0x0A, 0
msgKeinTyp1         db "FEHLER: Kein freier RAM!",   0x0D, 0x0A, 0
msgKernelZielFehler db "FEHLER: 0x100000 belegt!",   0x0D, 0x0A, 0
msgKeineEintraege   db "FEHLER: Keine Eintraege!",   0x0D, 0x0A, 0
msgE801ok           db "E801 Fallback OK",           0x0D, 0x0A, 0
msgE801fail         db "E801 fehlgeschlagen...",     0x0D, 0x0A, 0
msgInt12ok          db "INT12 Fallback OK",          0x0D, 0x0A, 0
msgLongModeOk       db "Long Mode verfuegbar",       0x0D, 0x0A, 0
msgNXok             db "NX-Bit verfuegbar",          0x0D, 0x0A, 0
msgNXfehlt          db "NX-Bit nicht vorhanden",     0x0D, 0x0A, 0
msgSSEok            db "SSE verfuegbar",             0x0D, 0x0A, 0
msgSSE2ok           db "SSE2 verfuegbar",            0x0D, 0x0A, 0
msgSSEfehlt         db "SSE nicht vorhanden",        0x0D, 0x0A, 0
msgKeinCPUID        db "FEHLER: Kein CPUID!",        0x0D, 0x0A, 0
msgCPUzuAlt         db "FEHLER: CPU zu alt!",        0x0D, 0x0A, 0
msgKeinLongMode     db "FEHLER: Kein Long Mode!",    0x0D, 0x0A, 0
msgKernelLaden      db "Lade Kernel...",             0x0D, 0x0A, 0
msgMagicOk          db "Kernel Magic OK",            0x0D, 0x0A, 0
msgKernelOk         db "Kernel geladen",             0x0D, 0x0A, 0
msgKernelDiskFehler db "FEHLER: Kernel Disk!",       0x0D, 0x0A, 0
msgKernelMagicFehler db "FEHLER: Kernel Magic!",     0x0D, 0x0A, 0
msgChecksumOk       db "Kernel Checksum OK",         0x0D, 0x0A, 0
msgChecksumFehler   db "FEHLER: Kernel korrupt!",    0x0D, 0x0A, 0
msgGDTok            db "GDT geladen",                0x0D, 0x0A, 0
msgUnrealStart      db "Unreal Mode start...",       0x0D, 0x0A, 0
msgUnrealOk         db "Unreal Mode OK",             0x0D, 0x0A, 0
msgDiskTimeout      db "FEHLER: Disk Timeout!",      0x0D, 0x0A, 0
msgFatalHalt        db "FATAL HALT ",                0
msgErrPrefix        db "ERR:",                       0
msgCRLF             db 0x0D, 0x0A,                   0
msgKeinEntry        db "FATAL: Kernel Entry = 0!",   0
msgPMFehler         db "FATAL: Protected Mode!",     0
msgVBEok   db "VBE OK", 0x0D, 0x0A, 0
msgVBEfail db "VBE fehlgeschlagen", 0x0D, 0x0A, 0
msgVBEsetFail db "FEHLER: VBE Set fehlgeschlagen!", 0x0D, 0x0A, 0

; ========================================
; PROTECTED MODE AKTIVIEREN
; ========================================
enterProtectedMode:
    cli
    mov eax, cr0
    or eax, 0x01
    mov cr0, eax
    jmp dword GDT_CODE_SEL:pm_entry


; ========================================
; AB HIER: 32-BIT PROTECTED MODE
; ========================================
[BITS 32]

pm_entry:
    mov ax, GDT_DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov byte [BREADCRUMB_ADDR], 0x07

    call setupPaging

    ; PAE (CR4 Bit 5) – Pflicht vor Long Mode
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; CR3 auf PML4
    mov eax, PML4_ADDR
    mov cr3, eax

    ; EFER LME-Bit (Bit 8)
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Paging an (CR0 Bit 31) → LMA rastet ein
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    jmp GDT_CODE64_SEL:lm_entry


; ========================================
; PAGING STRUKTUREN AUFBAUEN
; ========================================
setupPaging:
    ; Alle drei Tabellen nullen
    mov edi, PML4_ADDR
    xor eax, eax
    mov ecx, 3 * 1024
    rep stosd

    ; PML4[0] → PDPT
    mov dword [PML4_ADDR], PDPT_ADDR | 0x03

    ; PDPT[0] → PD
    mov dword [PDPT_ADDR], PD_ADDR | 0x03

    ; PD[0..511] → 512 × 2MB Pages = 1GB Identity Map
    mov edi, PD_ADDR
    mov eax, 0x00000083         ; erste 2MB: P=1, RW=1, PS=1
    mov ecx, 512
.pd_fill:
    mov dword [edi], eax
    mov dword [edi + 4], 0      ; obere 32 Bit = 0
    add edi, 8
    add eax, 0x200000           ; nächste 2MB
    dec ecx
    jnz .pd_fill

    ret


; ========================================
; VGA AUSGABE - 32-BIT
; ========================================
; IN: ESI = String, BL = Farbe
pm_print:
    mov edi, 0xB8000
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, bl
    mov word [edi], ax
    add edi, 2
    jmp .loop
.done:
    ret

; IN: ESI = Fehlermeldung
pm_fatal:
    mov bl, 0x4F                ; weiß auf rot
    call pm_print
    cli
.halt:
    hlt
    jmp .halt


; ========================================
; AB HIER: 64-BIT LONG MODE
; ========================================
[BITS 64]

lm_entry:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov rsp, 0x90000            ; 16-Byte aligned (System V ABI)

    mov byte [BREADCRUMB_ADDR], 0x08

    ; Kernel aufrufen (System V 64-Bit Convention)
    ;   RDI = Boot-Info-Struktur (0x8F00)
    ;   RSI = E820-Map           (0x9000)
    ;   RDX = Gesamt-RAM in KB
    mov rdi, BOOT_INFO
    mov rsi, E820_BUFFER
    mov edx, dword [BOOT_INFO + 6]

    mov byte [BOOT_INFO + 32], 0

    mov eax, dword [kernelEntry]
    test rax, rax
    jz .kein_entry

    jmp rax                     ; Sprung zum Kernel – kein CALL!

.kein_entry:
    lea rsi, [rel msgKeinEntry]
    call lm_fatal


; ========================================
; VGA AUSGABE - 64-BIT
; ========================================
; IN: RSI = String, BL = Farbe
lm_print:
    mov rdi, 0xB8000
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, bl
    mov word [rdi], ax
    add rdi, 2
    jmp .loop
.done:
    ret

; IN: RSI = Fehlermeldung
lm_fatal:
    mov bl, 0x4F                ; weiß auf rot
    call lm_print
    cli
.halt:
    hlt
    jmp .halt