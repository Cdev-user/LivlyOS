#ifndef KMALLOC_H
#define KMALLOC_H

#include "../kernel/kernel.h"
#include "pmm.h"

#define HEAP_SIZE  0x100000

typedef struct Block {
    uint64_t size;
    uint32_t used;
    struct Block *next;
    uint64_t neu;
}Block;




void kmalloc_init();
void *kmalloc(uint64_t size);
void kfree(void *ptr);
void merge_blocks();

#endif