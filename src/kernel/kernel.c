#include "kernel.h"
#include "gdt.h"  
#include "idt.h"   
#include "timer.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/kmalloc.h"
#include "../proc/schedular.h"
#include "shell/shell.h"
#include "../drivers/fs/fat.h"
#include "stress_test.h"
#include "../drivers/graphics/framebuffer.h" 
#include "../drivers/graphics/fb_console.h"
#include "console.h"
#include "../drivers/keyboard.h"
// ============================================================
// Kernel-Header bei 0x100000
// __attribute__((section(".header"))) = Linker legt das
// als ERSTES ins Binary, noch vor Kernel_main
// ============================================================
__attribute__((section(".header")))
KernelHeader kernel_header = {
    .magic       = KERNEL_MAGIC,    // 0xC0DEC0DE - Stage2 prüft das
    .kernel_size = 0,               // wird vom Makefile nach dem Linken eingetragen
    .entry_point = 0,               // wird vom Makefile nach dem Linken eingetragen
    .version     = 0x00000001,      // Version 0.0.0.1
    .checksum    = 0,               // später
    .reserved    = {0}
};



void sleep_ms(uint64_t ms) {
    uint64_t start = get_ms();
    while (get_ms() - start < ms);
}
void sleep_s(uint64_t sekunden) {
    uint64_t start = get_ms();
    while (get_ms() - start < sekunden * 1000);
}


void shell_process(){
    kprintf("Inside shell_process!\n");   
    shell_init();
}
void idle_process() {
    while(1) {
        __asm__ volatile("hlt");
    }
}









// ============================================================
// Kernel Entry Point
// info = Zeiger auf BootInfo-Struktur bei 0x8F00
//============================================================void Kernel_main(BootInfo *info) {
void Kernel_main(BootInfo *info) {
    console_init(info);
    console_clear();
    
    console_print("LivlyOS\n");
    kprintf("Boot type: %d\n", info->boot_type);
    kprintf("Framebuffer: 0x%x\n", info->fb_addr);
    
    gdt_init();
    idt_init();
    timer_init();
    keyboard_init(); 
    pmm_init(info);
    vmm_init(info);
    kmalloc_init();
    kprintf("Alle Inits durch!\n");
    
    //schedular initalising
    schedular_init();
    fat_init();
    
    
scheduler_add("shell", shell_process, 0);
scheduler_add("idle", idle_process, 255);
scheduler_add("reaper", reaper_process, 2);

kprintf("Vor scheduler start\n");

PCB *first = get_current_process();
if (first) {
    __asm__ volatile(
        "pushq %4\n\t"        /* ss */
        "pushq %3\n\t"        /* rsp */
        "pushq %2\n\t"        /* rflags */
        "pushq %1\n\t"        /* cs */
        "pushq %0\n\t"        /* rip */
        "iretq\n\t"
        :
        : "r"((uint64_t)first->regs.rip),
          "r"((uint64_t)first->regs.cs),
          "r"((uint64_t)first->regs.rflags),
          "r"((uint64_t)first->regs.rsp),
          "r"((uint64_t)first->regs.ss)
        : "memory"
    );
}
while (1) { __asm__ volatile("hlt"); }

}



