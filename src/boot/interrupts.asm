; ============================================================
; interrupts.asm  —  ISR-Stubs für alle 256 Interrupts
; Wird mit:  nasm -f elf64 interrupts.asm -o interrupts.o
; verlinkt zusammen mit kernel.o
; ============================================================

[BITS 64]
[DEFAULT REL]

; C-Handler Deklaration — definiert in idt.c
extern exception_handler

; ============================================================
; MAKROS
; ============================================================

; ISR ohne Error-Code: CPU pusht KEINEN Error-Code.
; Wir pushen 0 als Dummy damit der Stack immer gleich aussieht.
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0        ; Dummy Error-Code (damit Stack einheitlich)
    push qword %1       ; Interrupt-Nummer
    jmp isr_common
%endmacro

; ISR mit Error-Code: CPU hat bereits Error-Code gepusht.
; Wir pushen nur die Interrupt-Nummer dazu.
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push qword %1       ; Interrupt-Nummer (Error-Code liegt schon drüber)
    jmp isr_common
%endmacro

; ============================================================
; CPU EXCEPTIONS (0–31)
; Welche einen Error-Code pushen steht im Intel-Manual Vol.3 §6.13
; ============================================================
ISR_NOERRCODE  0    ; #DE  Division by Zero
ISR_NOERRCODE  1    ; #DB  Debug
ISR_NOERRCODE  2    ;      NMI (Non-Maskable Interrupt)
ISR_NOERRCODE  3    ; #BP  Breakpoint
ISR_NOERRCODE  4    ; #OF  Overflow
ISR_NOERRCODE  5    ; #BR  Bound Range Exceeded
ISR_NOERRCODE  6    ; #UD  Invalid Opcode
ISR_NOERRCODE  7    ; #NM  Device Not Available (FPU)
ISR_ERRCODE    8    ; #DF  Double Fault        ← Error-Code immer 0
ISR_NOERRCODE  9    ;      Coprocessor Segment Overrun (veraltet)
ISR_ERRCODE   10    ; #TS  Invalid TSS
ISR_ERRCODE   11    ; #NP  Segment Not Present
ISR_ERRCODE   12    ; #SS  Stack-Segment Fault
ISR_ERRCODE   13    ; #GP  General Protection Fault  ← der häufigste!
ISR_ERRCODE   14    ; #PF  Page Fault
ISR_NOERRCODE 15    ;      reserviert
ISR_NOERRCODE 16    ; #MF  x87 Floating-Point Exception
ISR_ERRCODE   17    ; #AC  Alignment Check
ISR_NOERRCODE 18    ; #MC  Machine Check
ISR_NOERRCODE 19    ; #XM  SIMD Floating-Point Exception
ISR_NOERRCODE 20    ; #VE  Virtualization Exception
ISR_ERRCODE   21    ; #CP  Control Protection Exception
ISR_NOERRCODE 22    ; reserviert
ISR_NOERRCODE 23    ; reserviert
ISR_NOERRCODE 24    ; reserviert
ISR_NOERRCODE 25    ; reserviert
ISR_NOERRCODE 26    ; reserviert
ISR_NOERRCODE 27    ; reserviert
ISR_NOERRCODE 28    ; #HV  Hypervisor Injection
ISR_ERRCODE   29    ; #VC  VMM Communication Exception
ISR_ERRCODE   30    ; #SX  Security Exception
ISR_NOERRCODE 31    ; reserviert

; ============================================================
; IRQs (32–47) — Hardware-Interrupts vom PIC
; Kommen erst wenn du den PIC initialisierst, für jetzt
; braucht man die Stubs trotzdem damit die IDT vollständig ist.
; ============================================================
%assign i 32
%rep 16
    ISR_NOERRCODE i
    %assign i i+1
%endrep

; ============================================================
; Rest (48–255) — Software-Interrupts, Syscalls, etc.
; ============================================================
%assign i 48
%rep 208
    ISR_NOERRCODE i
    %assign i i+1
%endrep

; ============================================================
; GEMEINSAMER HANDLER
; Wird von allen Stubs angesprungen.
; Stack-Layout wenn wir hier ankommen (von RSP aufwärts):
;   [RSP+ 0]  int_num     (von Stub gepusht)
;   [RSP+ 8]  error_code  (von CPU oder Dummy)
;   [RSP+16]  RIP         (von CPU gepusht)
;   [RSP+24]  CS
;   [RSP+32]  RFLAGS
;   [RSP+40]  RSP (alter)
;   [RSP+48]  SS
; ============================================================
isr_common:
    ; Alle Caller-saved und Callee-saved Register sichern
    ; Reihenfolge MUSS mit InterruptFrame in idt.h übereinstimmen!
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; RSP zeigt jetzt auf r15 = Anfang des InterruptFrame
    ; System V ABI: erstes Argument in RDI
    mov rdi, rsp            ; RDI = Zeiger auf InterruptFrame → C bekommt ihn

    ; Stack ausrichten: System V ABI verlangt 16-Byte-Alignment vor CALL.
    ; Wenn wir hier ankommen ist RSP immer ungerade-aligned (odd number of pushes).
    ; sub rsp, 8 macht es 16-Byte-aligned.
    sub rsp, 8
    call exception_handler
    add rsp, 8

    ; Register wiederherstellen (umgekehrte Reihenfolge)
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; int_num und error_code vom Stack räumen
    add rsp, 16

    iretq               ; 64-Bit Interrupt Return