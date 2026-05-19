#include "kernel.h"
#include "../kernel/io.h"
#include "keyboard.h"
#include "../kernel/console.h"



static char de_normal[] = {
    0, '^', '1','2','3','4','5','6','7','8','9','0','-','=',0,
    0, 'q','w','e','r','t','z','u','i','o','p',0,'+','\n',
    0, 'a','s','d','f','g','h','j','k','l',0,0,'^',
    0, '<','y','x','c','v','b','n','m',',','.','-',0,
    0, 0, ' '
};

static char de_shift[] = {
    0, 0, '!','"', 0,'$','%','&','/','(',')','=','?','`',0,
    0, 'Q','W','E','R','T','Z','U','I','O','P',0,'*','\n',
    0, 'A','S','D','F','G','H','J','K','L',0,0,0,
    0, '>','Y','X','C','V','B','N','M',';',':','_',0,
    0, 0, ' '
};

#define KB_BUFFER_SIZE 1096
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64

static char letztes_zeichen = 0;
static uint8_t shift_gedrueckt = 0;
static uint8_t ctrl_gedrueckt  = 0;
static uint8_t extended = 0; 

static uint8_t kb_buffer[KB_BUFFER_SIZE];
static int kb_read_pos  = 0;  // where wile be readed next time
static int kb_write_pos = 0;  // where wile be written the next time





static void ps2_wait_write(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
    }
}

static void ps2_wait_read(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(PS2_STATUS) & 0x01) return;
    }
}

void keyboard_init(void) {
    kprintf("KB init start\n");
    
    /* PIC Mask prüfen - ist IRQ1 maskiert? */
    uint8_t mask_before = inb(0x21);
    kprintf("PIC mask before: %x\n", mask_before);
    
    /* PS/2 Init wie vorher... */
    ps2_wait_write();
    outb(PS2_CMD, 0xAD);
    ps2_wait_write();
    outb(PS2_CMD, 0xA7);
    inb(PS2_DATA);
    
    /* Config lesen + ausgeben */
    ps2_wait_write();
    outb(PS2_CMD, 0x20);
    ps2_wait_read();
    uint8_t config = inb(PS2_DATA);
    kprintf("PS/2 config: %x\n", config);
    
    config |= 0x01;
    config |= 0x40;
    config &= ~0x10;
    
    ps2_wait_write();
    outb(PS2_CMD, 0x60);
    ps2_wait_write();
    outb(PS2_DATA, config);
    
    ps2_wait_write();
    outb(PS2_CMD, 0xAE);
    
    /* Keyboard Reset + Response checken */
    ps2_wait_write();
    outb(PS2_DATA, 0xFF);
    ps2_wait_read();
    uint8_t resp = inb(PS2_DATA);
    kprintf("KB reset response: %x\n", resp);
    
    /* IRQ1 im PIC explizit unmasken */
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1);          // Bit 1 löschen = IRQ1 unmasked
    outb(0x21, mask);
    kprintf("PIC mask after: %x\n", inb(0x21));
}







void keyboard_handler() {
 
    uint8_t scancode = inb(KEYBOARD_PORT);

    if (scancode == 0xE0) { extended = 1; return; }

    // Extended (Key Arrows)
    if (extended) {
        extended = 0;
        if (scancode >= 0x80) return;       //ignores fake-shift 0xAA/0xB6
        switch (scancode) {
        case 0x48: keyboard_push(shift_gedrueckt ? KEY_SHIFT_UP    : KEY_UP);    return;
        case 0x50: keyboard_push(shift_gedrueckt ? KEY_SHIFT_DOWN  : KEY_DOWN);  return;
        case 0x4B: keyboard_push(shift_gedrueckt ? KEY_SHIFT_LEFT  : KEY_LEFT);  return;
        case 0x4D: keyboard_push(shift_gedrueckt ? KEY_SHIFT_RIGHT : KEY_RIGHT); return;
        default:   return;
        }
    }

    // Modifier: Shift
    if (scancode == 0x2A || scancode == 0x36) { shift_gedrueckt = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_gedrueckt = 0; return; }

    // Modifier: Ctrl
    if (scancode == 0x1D) { ctrl_gedrueckt = 1; return; }
    if (scancode == 0x9D) { ctrl_gedrueckt = 0; return; }


    // Release-Events
    if (scancode >= 0x80) return;

    // ESC
    if (scancode == 0x01) { keyboard_push(0x1B); return; }

    // Backspace
    if (scancode == 0x0E) { keyboard_push('\b'); return; }
    // Ctrl+S/C/V/Z/Y
    if (ctrl_gedrueckt) {
        switch (scancode) {
            case 0x1F: keyboard_push(KEY_CTRL_S); return;
            case 0x2E: keyboard_push(KEY_CTRL_C); return;
            case 0x2F: keyboard_push(KEY_CTRL_V); return;
            case 0x2C: keyboard_push(KEY_CTRL_Y); return;
            case 0x15: keyboard_push(KEY_CTRL_Z); return;
        }
        return;
    }

    // Normale Character
    char c = shift_gedrueckt ? de_shift[scancode] : de_normal[scancode];
    if (c != 0) { letztes_zeichen = c; keyboard_push(c); }
}
char keyboard_getchar(){
    return letztes_zeichen;
}

void keyboard_push(uint8_t c) {
    kb_buffer[kb_write_pos] = c;
    kb_write_pos = (kb_write_pos + 1) % KB_BUFFER_SIZE;
}

uint8_t keyboard_read() {
    while (kb_read_pos == kb_write_pos);  // waits until there is something
    
    uint8_t c = kb_buffer[kb_read_pos];
    kb_read_pos = (kb_read_pos + 1) % KB_BUFFER_SIZE;
    return c;
}