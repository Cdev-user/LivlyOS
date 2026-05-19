#include "../kernel/kernel.h"  
#include "pmm.h"


//Compleate rewrite 

//Linker fills it out
extern uint8_t __kernel_end[];


//  PMM state variables 
static uint8_t  *bitmap    = 0;   // pointer on the bitmap
static uint64_t  max_pages = 0;   // how mny pages the bit map has
static uint64_t  total_pages = 0;
static uint64_t  free_pages  = 0;
uint64_t         pmm_bitmap_end = 0; // exporting this to kmalloc so it can use it as heap start

//Bit Operations

static void bit_setzen(uint64_t page) {
    if (page >= max_pages) return;                      //if we try accesing memmory without in the bit map 
    bitmap[page / 8] |= (uint8_t)(1 << (page % 8));
    /*Explaination
    1.First we calculate the byte and bit in the bitmap
    BYte = page / 8 = 2 / 8 so page to is on byte 0
    bit = page % 8 = 2 % 8 = 6 so we know page 2 lays on the bitmap[2] with 6bits
    2.We write it 1 << bit creates a mask and then we write it with |=
    */
}

static void bit_loeschen(uint64_t page) {
    if (page >= max_pages) return;
    bitmap[page / 8] &= (uint8_t)~(1 << (page % 8));
}

static int bit_lesen(uint64_t page) {
    if (page >= max_pages) return 1;   
    return (bitmap[page / 8] >> (page % 8)) & 1;
}


//pmm reserve range
// reserves a phsikal memmory addr

void pmm_reserve_range(uint64_t addr, uint64_t length) {
    uint64_t start = addr / PAGE_SIZE;
    uint64_t end   = (addr + length + PAGE_SIZE - 1) / PAGE_SIZE;

    for(uint64_t p = start; p < end && p < max_pages;p++) {
        if(bit_lesen(p) == 0) {
            free_pages--;
        }
        bit_setzen(p);
    }
}

//PMM ALLOC
// searches free page and marks it as taken 
// search starts at 1 so we dont overwrite bios data
uint64_t pmm_alloc() {
    for (uint64_t i = 1; i < max_pages; i++) {
        if (bit_lesen(i) == 0) {
            bit_setzen(i);
            free_pages--;
            return i * PAGE_SIZE;
        }
    }
    return 0xFFFFFFFFFFFFFFFF;  // Out of memory
}

void pmm_free(uint64_t adresse) {
    uint64_t page = adresse / PAGE_SIZE;
    if (page == 0 || page >= max_pages) return;  // Not valid
    if (bit_lesen(page) == 0) return;             // Stopps Double free
    bit_loeschen(page);
    free_pages++;
}




//PMM init
//  has to phases 
/*
    1. we find the biggest ram adress in E820 and calculate the bitmap size
    2. We initalise the bitmap, we save where are free areas, and we look for 
    area that we cant use
*/


void pmm_init(BootInfo *info) {

    E820Entry *map = (E820Entry*)((uint64_t)info->e820_map_addr);

    //Stage 1 we look fot the highest memmory address
    uint64_t highest_addr = 0;


    //looks for highes address
    for(int i = 0; i < info->e820_count;i++){ 
        uint64_t end = map[i].base + map[i].length;
        if(end >= highest_addr) {
        highest_addr = end;
        }
    }

    // we round it up if highest_addr isnt page aliegned
    max_pages = (highest_addr + PAGE_SIZE -1) / PAGE_SIZE;

    // we round up with / 8
    //calculating bitmap size in bytes 
    uint64_t bitmap_bytes = (max_pages + 7) / 8;


    //Placing bitmap behind the kernel 
    //for safty we round up to the next Page
    uint64_t bitmap_addr = ((uint64_t)__kernel_end + PAGE_SIZE -1 ) & ~(uint64_t)(PAGE_SIZE - 1);



    bitmap = (uint8_t*)bitmap_addr;



    //pmm_bitmap_end = first addr after bitmap alligned 
    // we use it as our first heap start 
    pmm_bitmap_end = (bitmap_addr + bitmap_bytes + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    //bitmap init
    for(uint64_t i = 0; i < bitmap_bytes;i++) {
        bitmap[i] = 0xFF;
    }

    total_pages = 0;
    free_pages  = 0;
 
    
    //STAGE 2a
    for(int i = 0; i < info->e820_count;i++) {
        if(map[i].type != E820_FREE) continue;

        // first full page and we round up
        uint64_t start = (map[i].base + PAGE_SIZE - 1) / PAGE_SIZE;

        //last full page and we round down
        uint64_t end   = (map[i].base + map[i].length) / PAGE_SIZE;

        if (start >= max_pages) continue;
        if (end   >  max_pages) end = max_pages;

        for(uint64_t p = start; p < end;p++){
            bit_loeschen(p);
            total_pages++;
            free_pages++;
        }
    }



//Stage2 
//Reserving some stuff so that we dont overwrte important things

// 1.Page 0 (BIOS IVT and BDA, never touch that things)
pmm_reserve_range(0x0, PAGE_SIZE);

//2. Paging Tabelle die der bootloader abgelegt hat bei 
pmm_reserve_range(0x1000, 3 * PAGE_SIZE); // reserves PML4 etc.

// 3. Reserving the kernel code(0x100000 bis __kernel_end)
uint64_t kernel_size_aligned = (info->kernel_size + PAGE_SIZE - 1)& ~(uint64_t)(PAGE_SIZE - 1);

pmm_reserve_range(0x100000, kernel_size_aligned);

// 4. Reserving the bitmap because if not pmm_alloc would se the bitmap as free

pmm_reserve_range(bitmap_addr, pmm_bitmap_end - bitmap_addr);

// 5. Reserving the Heap
//Heap size first 1MB but can grow
#define HEAP_SIZE (1024 * 1024)  // 1 MB
    pmm_reserve_range(pmm_bitmap_end, HEAP_SIZE);

}


uint64_t pmm_get_free_pages()  { return free_pages; }
uint64_t pmm_get_total_pages() { return total_pages; }