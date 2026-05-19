#include "kmalloc.h"
#include "../kernel/kernel.h"
#include "../kernel/vga.h"

// Zeiger auf den ersten Block im Heap
static Block *heap_start = 0;


void kmalloc_init() {
    // Heap initialisieren — einen großen freien Block anlegen
    heap_start = (Block *)pmm_bitmap_end;

    heap_start->size = HEAP_SIZE - sizeof(Block);
    heap_start->used = 0;   // frei oder belegt?
    heap_start->next = NULL;   // gibt es einen nächsten Block?
}


void *kmalloc(uint64_t size) {
    // Ersten freien Block suchen der groß genug ist
    // Was ist die Abbruchbedingung der Suche?

    if(heap_start == 0) return 0;


    Block *current = heap_start;
    while(current != NULL){

        if(current->used == 0 && current->size >= size){
            if(current->size > size + sizeof(Block)) {
        // neuen Block nach den benötigten Bytes anlegen
                Block *neu = (Block*)((uint8_t*)(current + 1) + size);           // wo liegt der neue Block?
                neu->size = current->size - size - sizeof(Block);            // wie groß ist der Rest?
                neu->used = 0;              // frei oder belegt?
                neu->next = current->next;  // verketten
                current->size = size;       // aktuellen Block verkleinern
                current->next = neu;        // current zeigt jetzt auf neu
            }
            current->used = 1;
            return current + 1;
        }
        current = current->next;
    }
    return NULL;
}

void merge_blocks(Block *block) {
    // Merge mit nächstem Block 
    while(block->next != 0 && block->next->used == 0) {
        block->size += sizeof(Block) + block->next->size;
        block->next  = block->next->next;
    }
}


void kfree(void *ptr) {
    if(!ptr) return;
    // Wie findest du den Block-Header aus dem ptr?
    Block *block = (Block*)ptr - 1;
    block->used = 0;
    merge_blocks(block);
}