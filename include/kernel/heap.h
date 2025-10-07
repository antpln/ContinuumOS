#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

#define KERNEL_HEAP_START  0x00800000  // 8 MiB
#define KERNEL_HEAP_SIZE   0x00800000  // 8 MiB heap (adjustable)

// Heap statistics structure
typedef struct {
    uint32_t total_size;           // Total heap size
    uint32_t used_size;            // Total bytes allocated
    uint32_t free_size;            // Total bytes free
    uint32_t allocated_blocks;     // Number of allocated blocks
    uint32_t free_blocks;          // Number of free blocks
    uint32_t largest_free_block;   // Size of largest free block
    uint32_t overhead;             // Bytes used for metadata
} heap_stats_t;

void init_heap();
void* kmalloc(size_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, size_t size);
void get_heap_stats(heap_stats_t* stats);

#endif
