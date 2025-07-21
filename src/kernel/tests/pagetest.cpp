#include <kernel/tests/pagetest.h>
#include <kernel/paging.h>
#include <kernel/memory.h>
#include <kernel/debug.h>
#include <stdio.h>

void paging_test() {
    test("Paging Test: Mapping and Unmapping\n");
    uint32_t vaddr = 0x400000; // Arbitrary virtual address
    void* frame = PhysicalMemoryManager::allocate_frame();
    if (!frame) {
        PANIC("Paging Test: Failed to allocate frame\n");
        return;
    }
    vmm_map(vaddr, (uint32_t)frame, 1); // Map RW
    test("Mapped vaddr 0x%x to paddr 0x%x\n", vaddr, (uint32_t)frame);
    // Optionally, test access here if possible
    // Unmap by remapping to 0
    vmm_map(vaddr, 0, 1);
    test("Unmapped vaddr 0x%x\n", vaddr);
    PhysicalMemoryManager::free_frame(frame);
    test("Paging Test: Completed\n");
}