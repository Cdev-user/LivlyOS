#include "idt.h"
#include "kernel.h"
#include "gdt.h"
#include "timer.h"
#include "io.h"
#include "../drivers/keyboard.h"
#include "../proc/schedular.h"
// ─────────────────────────────────────────────────────────────
// 8259A PIC Neuprogrammierung (Remapping)
// ICW = Initialization Command Word
// ─────────────────────────────────────────────────────────────
#define PIC1_CMD  0x20   // Master PIC Kommando-Port
#define PIC1_DATA 0x21   // Master PIC Daten-Port
#define PIC2_CMD  0xA0   // Slave PIC Kommando-Port
#define PIC2_DATA 0xA1   // Slave PIC Daten-Port

#define PIC_EOI   0x20   // End-of-Interrupt Signal

static void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    // Aktuelle Masken retten (welche IRQs aktuell maskiert sind)
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1: Initialisierung starten, ICW4 wird folgen (0x11)
    outb(PIC1_CMD,  0x11);
    outb(PIC2_CMD,  0x11);

    // ICW2: Neuer Basisvektor
    // Master-IRQ 0 → INT master_offset (32)
    outb(PIC1_DATA, master_offset);
    // Slave-IRQ  8 → INT slave_offset  (40)
    outb(PIC2_DATA, slave_offset);

    // ICW3: Kaskadierung
    // Master: Slave hängt an IRQ-Leitung 2 (Bit 2 gesetzt = 0x04)
    outb(PIC1_DATA, 0x04);
    // Slave: Eigene Kaskaden-ID = 2
    outb(PIC2_DATA, 0x02);

    // ICW4: 8086/88-Modus aktivieren
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Ursprüngliche Masken wiederherstellen
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

// ─────────────────────────────────────────────────────────────
// IDT-Tabelle: 256 × 16 Byte = 4096 Byte
// static = liegt im BSS/Data-Segment, wird nicht auf dem Stack angelegt
// ─────────────────────────────────────────────────────────────
static IDTEntry idt[256];
static IDTR     idtr;

// ─────────────────────────────────────────────────────────────
// ISR-Funktionszeiger-Tabelle (korrekte Typisierung)
// ─────────────────────────────────────────────────────────────
void (*isr_table[256])() = {
    isr0,   isr1,   isr2,   isr3,   isr4,   isr5,   isr6,   isr7,
    isr8,   isr9,   isr10,  isr11,  isr12,  isr13,  isr14,  isr15,
    isr16,  isr17,  isr18,  isr19,  isr20,  isr21,  isr22,  isr23,
    isr24,  isr25,  isr26,  isr27,  isr28,  isr29,  isr30,  isr31,
    isr32,  isr33,  isr34,  isr35,  isr36,  isr37,  isr38,  isr39,
    isr40,  isr41,  isr42,  isr43,  isr44,  isr45,  isr46,  isr47,
    isr48,  isr49,  isr50,  isr51,  isr52,  isr53,  isr54,  isr55,
    isr56,  isr57,  isr58,  isr59,  isr60,  isr61,  isr62,  isr63,
    isr64,  isr65,  isr66,  isr67,  isr68,  isr69,  isr70,  isr71,
    isr72,  isr73,  isr74,  isr75,  isr76,  isr77,  isr78,  isr79,
    isr80,  isr81,  isr82,  isr83,  isr84,  isr85,  isr86,  isr87,
    isr88,  isr89,  isr90,  isr91,  isr92,  isr93,  isr94,  isr95,
    isr96,  isr97,  isr98,  isr99,  isr100, isr101, isr102, isr103,
    isr104, isr105, isr106, isr107, isr108, isr109, isr110, isr111,
    isr112, isr113, isr114, isr115, isr116, isr117, isr118, isr119,
    isr120, isr121, isr122, isr123, isr124, isr125, isr126, isr127,
    isr128, isr129, isr130, isr131, isr132, isr133, isr134, isr135,
    isr136, isr137, isr138, isr139, isr140, isr141, isr142, isr143,
    isr144, isr145, isr146, isr147, isr148, isr149, isr150, isr151,
    isr152, isr153, isr154, isr155, isr156, isr157, isr158, isr159,
    isr160, isr161, isr162, isr163, isr164, isr165, isr166, isr167,
    isr168, isr169, isr170, isr171, isr172, isr173, isr174, isr175,
    isr176, isr177, isr178, isr179, isr180, isr181, isr182, isr183,
    isr184, isr185, isr186, isr187, isr188, isr189, isr190, isr191,
    isr192, isr193, isr194, isr195, isr196, isr197, isr198, isr199,
    isr200, isr201, isr202, isr203, isr204, isr205, isr206, isr207,
    isr208, isr209, isr210, isr211, isr212, isr213, isr214, isr215,
    isr216, isr217, isr218, isr219, isr220, isr221, isr222, isr223,
    isr224, isr225, isr226, isr227, isr228, isr229, isr230, isr231,
    isr232, isr233, isr234, isr235, isr236, isr237, isr238, isr239,
    isr240, isr241, isr242, isr243, isr244, isr245, isr246, isr247,
    isr248, isr249, isr250, isr251, isr252, isr253, isr254, isr255
};

// ─────────────────────────────────────────────────────────────
// idt_set_entry — trägt einen Handler in die IDT ein
//
// index        : Interrupt-Nummer (0–255)
// handler_addr : 64-Bit virtuelle Adresse des ASM-Stubs
// flags        : Gate-Typ + DPL + Present-Bit
//                0x8E = Interrupt Gate, DPL=0, Present
//                0x8F = Trap Gate,      DPL=0, Present
//                Unterschied: Interrupt Gate löscht IF (CLI),
//                             Trap Gate lässt IF unverändert
// ─────────────────────────────────────────────────────────────
void idt_set_entry(uint8_t index, uint64_t handler_addr, uint8_t flags) {
    idt[index].selector    = 0x08;                          // Kernel-Code-Segment in der GDT
    idt[index].IST         = 0;                             // IST = 0 → benutzt normalen Kernel-Stack
    idt[index].reserved    = 0;                             // muss laut Intel-Manual 0 sein
    idt[index].flags       = flags;

    // Handler-Adresse auf drei Felder aufteilen
    idt[index].handler_low  = (uint16_t)(handler_addr & 0xFFFF);
    idt[index].handler_mid  = (uint16_t)((handler_addr >> 16) & 0xFFFF);
    idt[index].handler_high = (uint32_t)(handler_addr >> 32);   // obere 32 Bit
}

// ─────────────────────────────────────────────────────────────
// idt_init — initialisiert die gesamte IDT
// ─────────────────────────────────────────────────────────────
void idt_init() {
    // 1) PIC umprogrammieren BEVOR irgendein Interrupt feuern kann
    //    Master-Offset 32 (0x20), Slave-Offset 40 (0x28)
    pic_remap(32, 40);

    // 2) Alle 256 IDT-Einträge mit den ASM-Stubs befüllen
    //    NUR EINE Schleife — kein zweites Überschreiben!
    for (int i = 0; i < 256; i++) {
        idt_set_entry(i, (uint64_t)isr_table[i], 0x8E);
    }

    // 3) IDTR aufbauen
    //    limit = Größe der Tabelle in Byte minus 1
    idtr.limit = (uint16_t)(256 * sizeof(IDTEntry) - 1);
    idtr.base  = (uint64_t)idt;

    // 4) IDT laden — ab jetzt kennt die CPU unsere Handler
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

// ─────────────────────────────────────────────────────────────
// Hex-Ausgabe Hilfsfunktion für den VGA-Puffer
// Schreibt "0xNN" ab Position pos in den VGA-Puffer
// ─────────────────────────────────────────────────────────────
static void vga_print_hex(volatile uint8_t *vga, int pos, uint64_t val,
                          int digits, uint8_t color) {
    // "0x" Prefix
    vga[(pos + 0) * 2]     = '0';
    vga[(pos + 0) * 2 + 1] = color;
    vga[(pos + 1) * 2]     = 'x';
    vga[(pos + 1) * 2 + 1] = color;
    // Ziffern von links nach rechts
    for (int d = digits - 1; d >= 0; d--) {
        uint8_t nibble = (val >> (d * 4)) & 0xF;
        char    c      = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        vga[(pos + 2 + (digits - 1 - d)) * 2]     = c;
        vga[(pos + 2 + (digits - 1 - d)) * 2 + 1] = color;
    }
}

// Exception-Namen für die ersten 32 CPU-Exceptions
static const char *exception_names[32] = {
    "Division by Zero",        // 0  #DE
    "Debug",                   // 1  #DB
    "NMI",                     // 2
    "Breakpoint",              // 3  #BP
    "Overflow",                // 4  #OF
    "Bound Range Exceeded",    // 5  #BR
    "Invalid Opcode",          // 6  #UD
    "Device Not Available",    // 7  #NM
    "Double Fault",            // 8  #DF
    "Coprocessor Seg Overrun", // 9
    "Invalid TSS",             // 10 #TS
    "Segment Not Present",     // 11 #NP
    "Stack-Segment Fault",     // 12 #SS
    "General Protection",      // 13 #GP
    "Page Fault",              // 14 #PF
    "Reserved",                // 15
    "x87 FP Exception",        // 16 #MF
    "Alignment Check",         // 17 #AC
    "Machine Check",           // 18 #MC
    "SIMD FP Exception",       // 19 #XM
    "Virtualization",          // 20 #VE
    "Control Protection",      // 21 #CP
    "Reserved", "Reserved",    // 22–23
    "Reserved", "Reserved",    // 24–25
    "Reserved", "Reserved",    // 26–27
    "Hypervisor Injection",    // 28 #HV
    "VMM Communication",       // 29 #VC
    "Security Exception",      // 30 #SX
    "Reserved"                 // 31
};

// ─────────────────────────────────────────────────────────────
// exception_handler — wird von allen ISR-Stubs aufgerufen
//
// Für CPU-Exceptions (0–31): Anzeige + Halt
// Für IRQs (32–47):          EOI an PIC senden + zurückkehren
// ─────────────────────────────────────────────────────────────
void exception_handler(InterruptFrame *frame) {
    volatile uint8_t *vga = (volatile uint8_t *)0xB8000;

    if (frame->int_num < 32) {
        // ── CPU Exception ──────────────────────────────────────
        // Zeile 0: roten Balken mit "!!! EXCEPTION !!!"
        const char *msg = "!!! EXCEPTION !!!";
        for (int i = 0; msg[i]; i++) {
            vga[i * 2]     = msg[i];
            vga[i * 2 + 1] = 0x4F;   // Weiß auf Rot
        }

        // Zeile 1 (Offset 160 Bytes): Exception-Nummer als Hex
        vga_print_hex(vga + 160, 0, frame->int_num, 2, 0x4F);

        // Exception-Name (wenn bekannt)
        const char *name = exception_names[frame->int_num];
        for (int i = 0; name[i]; i++) {
            vga[160 + (6 + i) * 2]     = name[i];
            vga[160 + (6 + i) * 2 + 1] = 0x4F;
        }

        // Zeile 2: RIP anzeigen (wo der Fehler passierte)
        const char *rip_label = "RIP:";
        for (int i = 0; rip_label[i]; i++) {
            vga[320 + i * 2]     = rip_label[i];
            vga[320 + i * 2 + 1] = 0x4F;
        }
        vga_print_hex(vga + 320, 4, frame->rip, 16, 0x4F);

        // Zeile 3: Error-Code
        const char *ec_label = "ERR:";
        for (int i = 0; ec_label[i]; i++) {
            vga[480 + i * 2]     = ec_label[i];
            vga[480 + i * 2 + 1] = 0x4F;
        }
        vga_print_hex(vga + 480, 4, frame->error_code, 8, 0x4F);

        // System anhalten — kein iretq
        for (;;) {
            __asm__ volatile("hlt");
        }

    } else if (frame->int_num < 48) {
        if (frame->int_num == 32) {
            system_ticks++;
            schedule(frame);   // ← frame direkt weitergeben!
        }

        // ── Keybord driver ────────────────────────────
        if (frame->int_num == 33) {  // ← IRQ1 = Keyboard
        keyboard_handler();      // ← das hier
        }




        // ── Hardware-Interrupt (IRQ) ────────────────────────────
        // WICHTIG: EOI (End of Interrupt) an den PIC senden,
        // sonst sendet der PIC KEINE weiteren IRQs mehr!
        if (frame->int_num >= 40) {
            // Slave-PIC benötigt EOI wenn IRQ >= 8 (INT >= 40)
            outb(PIC2_CMD, PIC_EOI);
        }
        // Master-PIC bekommt immer ein EOI
        outb(PIC1_CMD, PIC_EOI);

        // Hier kannst du später IRQ-spezifische Handler aufrufen
        // z.B. if (frame->int_num == 32) pit_handler();
        //      if (frame->int_num == 33) keyboard_handler();
    }
    // INT 48–255: Software-Interrupts / Syscalls → hier noch nicht behandelt
}