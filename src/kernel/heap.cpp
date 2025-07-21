#include <kernel/heap.h>
#include <kernel/memory.h>  // For KERNEL_HEAP_START and KERNEL_HEAP_SIZE
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <kernel/debug.h>

// Heap block header structure.
typedef struct heap_block {
    size_t size;                 // Size of the block (excluding the header)
    struct heap_block* next;     // Pointer to the next block in the free list
    uint8_t free;                // 1 if the block is free, 0 if it's allocated
} heap_block_t;

// Global pointer to the start of the heap
static heap_block_t* free_list = NULL;  

// Align size to 16 bytes
static size_t align16(size_t size) {
    return (size + 15) & ~((size_t)15);
}

// Initialize the heap (must be called before using kmalloc)
void init_heap() {
    debug("[HEAP] Initializing heap at 0x%x, size 0x%x", KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    
    free_list = (heap_block_t*)KERNEL_HEAP_START;
    free_list->size = KERNEL_HEAP_SIZE - sizeof(heap_block_t);
    free_list->next = NULL;
    free_list->free = 1;
}

// Allocate memory from the heap
void* kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size = align16(size); // Ensure 16-byte alignment

    if (!free_list) {
        error("[HEAP] Error: Heap is not initialized!");
        return NULL;
    }

    heap_block_t* current = free_list;
    heap_block_t* previous = NULL;

    // Find a free block that is big enough
    while (current != NULL) {
        if (current->free && current->size >= size) {
            break;
        }
        previous = current;
        current = current->next;
    }

    if (current == NULL) {
        error("[HEAP] Error: No free block large enough for %d bytes!", size);
        return NULL;
    }

    // Check if we can split the block
    if (current->size >= size + sizeof(heap_block_t) + 16) {
        uintptr_t block_addr = (uintptr_t)current;
        heap_block_t* new_block = (heap_block_t*)(block_addr + sizeof(heap_block_t) + size);
        new_block->size = current->size - size - sizeof(heap_block_t);
        new_block->next = current->next;
        new_block->free = 1;
        current->size = size;
        current->next = new_block;
    }

    current->free = 0; // Mark block as used
    void* alloc_addr = (void*)((uintptr_t)current + sizeof(heap_block_t));

    debug("[HEAP] Allocated %d bytes at 0x%x", size, (uint32_t)alloc_addr);
    return alloc_addr;
}

// Free allocated memory
void kfree(void* ptr) {
    if (!ptr) return;

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    block->free = 1;
    debug("[HEAP] Freed block at 0x%x (size: %d bytes)", (uint32_t)ptr, block->size);

    // Try to merge with next block if free
    if (block->next && block->next->free) {
        block->size += block->next->size + sizeof(heap_block_t);
        block->next = block->next->next;
    }
}

// Reallocate memory from the heap
void* krealloc(void* ptr, size_t size) {
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    if (!ptr) {
        return kmalloc(size);
    }

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    if (block->size >= size) {
        return ptr; // The current block is already large enough
    }

    void* new_ptr = kmalloc(size);
    if (!new_ptr) {
        return NULL; // Allocation failed
    }

    memcpy(new_ptr, ptr, block->size); // Copy old data to new block
    kfree(ptr); // Free the old block

    return new_ptr;
}