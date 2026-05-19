[BITS 16]
[ORG 0x7C00]

cli                         ; Interrupts ausschalten (sicherer Start)

mov [driveNumber], dl       ; Boot-Laufwerk aus DL speichern (BIOS übergibt das)

xor ax, ax                  ; AX = 0
mov ds, ax                  ; Daten-Segment = 0
mov es, ax                  ; Extra-Segment = 0
mov fs, ax
mov gs, ax
mov ss, ax                  ; Stack-Segment = 0
mov sp, 0x7C00              ; Stack zeigt unter den Bootloader

jmp 0x0000:fix_cs           ; Far Jump → CS wird sicher auf 0 gesetzt

fix_cs:
    sti                     ; Interrupts wieder aktivieren

    call resetDisk          ; BIOS Disk reset (wichtig vor lesen)
    call checkA20           ; A20-Line prüfen/aktivieren
    call laodStage2         ; Stage2 von Disk laden

    cmp word [0x8000], 0xB007   ; Prüfen ob Stage2 gültig ist (Magic)
    jne stage2_invalid 
    mov si, msgStage2OK
    call printString

    mov al, [driveNumber]
    mov [0x7000], al
    jmp 0x8002                 ; Stage2 starten (skip Magic)

; ================= A20 =================

checkA20:
    mov si, msgA20Check         ; "A20 check..." ausgeben
    call printString

    mov ax, 0x2402              ; BIOS: A20 Status abfragen
    int 0x15

    jc enableA20                ; Fehler → versuche aktivieren

    cmp al, 1                   ; AL=1 → A20 aktiv
    je a20_enabled

    jmp enableA20               ; sonst aktivieren

a20_enabled:
    mov si, msgA20On
    call printString
    ret                         ; zurück zum Aufrufer

enableA20:
    mov si, msgTrying           ; "Enabling A20..."
    call printString

    mov ax, 0x2401              ; BIOS: A20 aktivieren
    int 0x15

    jc a20_Fallback             ; wenn Fehler → Fallback Methode

    mov cx, 1000
    call waitLoop               ; kleine Verzögerung

    call verifyA20              ; prüfen ob A20 jetzt aktiv
    cmp ax, 0
    je a20_Fallback             ; wenn nicht → Fallback

    mov si, msgA20Success       
    call printString

    ret

a20_Fallback:
    mov si, msgA20Fallback1     ; Hinweis: Fallback wird benutzt
    call printString

    in al, 0x92                 ; Fast A20 Port lesen
    or al, 0x02                 ; Bit 1 setzen → A20 aktivieren
    and al, 0xFE                ; Bit 0 löschen (kein Reset!)
    out 0x92, al                ; zurückschreiben

    mov cx, 1000
    call waitLoop               ; warten

    call verifyA20              ; nochmal prüfen
    cmp ax, 0
    je a20_error                ; wenn immer noch aus → Fehler

    mov si, msgA20Success       
    call printString

    ret                        ; Debug Stop

a20_error:
    mov si, msgA20Error
    call printString
    call hang                   ; System stoppen

; Prüft ob A20 aktiv ist (Wraparound-Test)

verifyA20:
    mov byte [0x0500], 0x00
    push es
    mov ax, 0xFFFF
    mov es, ax
    mov byte [es:0x0510], 0xFF   ; physisch 0xFFFF0 + 0x510 = 0x100500
                                  ; → wraps zu 0x00500 wenn A20 aus
    cmp byte [0x0500], 0xFF
    je .aus
    pop es
    mov ax, 1
    ret
.aus:
    pop es
    mov ax, 0
    ret

waitLoop:
.loop:
    loop .loop                  ; CX runterzählen → Delay
    ret

; ================= SYSTEM =================

hang:
    hlt                         ; CPU schlafen legen
    jmp hang                    ; Endlosschleife

; String-Ausgabe über BIOS
printString:
.print_char:
    lodsb                       ; nächstes Zeichen aus [SI]
    cmp al, 0                   ; 0 = String Ende
    je .done

    mov ah, 0x0E                ; Teletype Output
    mov bh, 0x00
    mov bl, 0x07                ; Farbe
    int 0x10                    ; BIOS Video

    jmp .print_char

.done:
    ret

; ================= DISK =================

resetDisk:
    mov ah, 0x00                ; BIOS Reset Disk
    mov dl, [driveNumber]
    int 0x13
    jc diskerror                ; Fehler?
    ret

diskerror:
    mov si, msgDiskerror
    call printString
    call hang

; INT 13h Extensions (AH=42h), LBA ab 1.
; Puffer: Segment 0x0800, Offset 0 -> Basis physikalisch 0x8000 (wie [ORG 0x8000] in stage2).
; So passen bis zu 128 Sektoren (64 KiB) in EINEM Segment (0:0x8000+64k waere ungueltig).
dap_stage2:
    db 0x10                 ; DAP-Groesse
    db 0
    dw 64                   ; Sektoren (32 KiB - mehr als genug für stage2)
    dw 0x0000               ; Offset Puffer (in Segment 0x0800)
    dw 0x0800               ; Segment: 0x0800:0 = phys. 0x8000
    dq 34                   ; Start-LBA 34 (nach GPT primary header+entries)
    
laodStage2:
    xor ax, ax
    mov es, ax
    mov byte [.retry], 0

.versuch:
    mov si, dap_stage2
    mov ah, 0x42
    mov dl, [driveNumber]
    int 0x13
    jnc .fertig

    inc byte [.retry]
    cmp byte [.retry], 3
    je disk_read_error
    call resetDisk
    jmp .versuch

.retry db 0

.fertig:
    ret

disk_read_error:
    mov si, msgLoadStage2err
    call printString
    call hang

stage2_invalid:
    mov si, msgInvalidStage2
    call printString
    call hang

; ================= DATA =================

driveNumber db 0               ; Boot-Laufwerk speichern

msgA20Check      db "A20 check...",0x0D,0x0A,0
msgTrying        db "Enabling A20...",0x0D,0x0A,0
msgA20Success    db "A20 enabled!",0x0D,0x0A,0
msgA20Fallback1  db "A20 enable failed,FALLBACK1...",0x0D,0x0A,0
msgA20On         db "A20 aktiv",0x0D,0x0A,0
msgA20Error      db "Error on A20!",0x0D,0x0A,0
msgDiskerror     db "Disk reset error",0x0D,0x0A,0
msgLoadStage2err db "Error loading stage2",0x0D,0x0A,0
msgInvalidStage2 db "Stage2 invalid",0x0D,0x0A,0
msgStage2OK      db "Stage2 OK, jumping...",0x0D,0x0A,0

times 510-($-$$) db 0         ; auf 512 Bytes auffüllen
dw 0xAA55                     ; Boot-Signatur