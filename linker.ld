/* ============================================================
   Livly OS - Linker Skript
   Nur für den Kernel (kernel.elf / kernel.bin)
   Stage1 und Stage2 werden direkt mit NASM -f bin gebaut,
   die brauchen kein Linker-Skript!
   ============================================================ */

ENTRY(Kernel_main)
/* Kernel_main = die C-Funktion die zuerst aufgerufen wird
   Der Name muss exakt mit dem C-Code übereinstimmen */

SECTIONS {

    . = 0x100000;
    /* Startadresse = 1MB (0x100000)
       Stage2 lädt den Kernel genau hierhin.
       Der Kernel muss also auch denken er liegt bei 0x100000.
       Das nennt sich VMA (Virtual Memory Address) */

    .header : {
        /* NEU: ALIGN(4) = auf 4-Byte-Grenze ausrichten
           verhindert Ausrichtungsfehler beim Lesen */
        . = ALIGN(4);
        *(.header)
        /* .header Sektion = für spätere Magic Number oder Kernel-Header
           Muss als ERSTES im Binary liegen damit Stage2 ihn finden kann */
    }

    .text : {
        . = ALIGN(16);
        /* Code auf 16-Byte ausrichten (gut für CPU Prefetch) */
        *(.text)
        *(.text.*)
        /* *(.text.*) = auch Subsektionen wie .text.startup mitnehmen
           GCC erzeugt manchmal .text.Kernel_main statt .text */
    }

    .rodata : {
        /* NEU: Read-Only Daten (String-Literale, const-Variablen)
           GCC legt diese automatisch in .rodata, nicht .data
           Ohne das hier würden alle String-Literale fehlen! */
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        . = ALIGN(4);
        *(.data)
        *(.data.*)
    }

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        /* NEU: Symbol __bss_start markiert Anfang des BSS
           Der Kernel kann damit BSS selbst auf 0 setzen:
           memset(__bss_start, 0, __bss_end - __bss_start) */

        *(.bss)
        *(.bss.*)
        *(COMMON)
        /* COMMON = nicht-initialisierte globale Variablen
           GCC legt diese manchmal in COMMON statt .bss */

        __bss_end = .;
        /* NEU: Symbol __bss_end markiert Ende des BSS */
    }

    __kernel_end = .;
    /* NEU: Gesamtgröße des Kernels berechnen:
       Kernel-Größe = __kernel_end - 0x100000
       Stage2 kann das in die Boot-Info-Struktur schreiben */
}