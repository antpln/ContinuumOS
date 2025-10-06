#include "kernel/paging.h"
#include <stdint.h>
#include <string.h>
#include "kernel/isr.h"
#include "kernel/debug.h"
#include "kernel/memory.h" // For PMM

// Define heap boundaries to avoid conflicts with paging
#define KERNEL_HEAP_START 0x00800000  // Heap starts at 8 MiB
#define KERNEL_HEAP_SIZE  0x00800000  // Heap spans 8 MiB (up to 16 MiB)

#define IDENTITY_MAP_SIZE_MB 32
#define IDENTITY_TABLES (IDENTITY_MAP_SIZE_MB / 4)

// Page directory and page tables allocated using PMM
static uint32_t* kernel_page_directory = nullptr;
static uint32_t* kernel_page_tables[IDENTITY_TABLES] = { nullptr };

// Page fault handler
void page_fault_handler(registers_t *registers) {
    uint32_t fault_addr;
    asm("mov %%cr2, %0" : "=r"(fault_addr));
    
    error("[VMM] Page Fault at 0x%x", fault_addr);
    error("[VMM] Page info: 0x%x", registers->eip);
    error("[VMM] Page fault caused by %s access",
           (registers->err_code & 0x1) ? "write" : "read");
    error("[VMM] Page fault %s",
           (registers->err_code & 0x2) ? "protection" : "non-present");
    error("[VMM] Page fault in %s mode",
           (registers->err_code & 0x4) ? "user" : "supervisor");
    error("[VMM] Page fault caused by %s operation",
           (registers->err_code & 0x8) ? "instruction fetch" : "data access");

    for (;;) asm("hlt");
}

void vmm_init()
{
    debug("[VMM] Initializing paging (identity map 0..16 MiB)");

    // Register the page fault handler
    register_interrupt_handler(14, page_fault_handler);

    // Allocate page directory and tables using PMM
    kernel_page_directory = (uint32_t*)PhysicalMemoryManager::allocate_frame();
    memset(kernel_page_directory, 0, PAGE_SIZE);

    for (int table_idx = 0; table_idx < IDENTITY_TABLES; ++table_idx) {
        kernel_page_tables[table_idx] = (uint32_t*)PhysicalMemoryManager::allocate_frame();
        memset(kernel_page_tables[table_idx], 0, PAGE_SIZE);

        for (uint32_t i = 0; i < 1024; ++i) {
            uint32_t phys_addr = (table_idx * 0x400000) + (i * 0x1000);
            kernel_page_tables[table_idx][i] = (phys_addr & 0xFFFFF000) | 0x03; // Present + RW
        }

        kernel_page_directory[table_idx] = ((uint32_t)kernel_page_tables[table_idx] & 0xFFFFF000) | 0x03;
        debug("[VMM] PDE[%d] = 0x%x", table_idx, kernel_page_directory[table_idx]);
    }

    debug("[VMM] First 4 entries of page_table0:");
    for (int i = 0; i < 4; ++i) {
        debug("  PT0[%d] = 0x%x", i, kernel_page_tables[0][i]);
    }

    debug("[VMM] Identity mapped MB=%d tables=%d", IDENTITY_MAP_SIZE_MB, IDENTITY_TABLES);
    debug("[VMM] PDE @ 0x%x", (uint32_t)kernel_page_directory);
}

void vmm_enable()
{
    debug("[VMM] Enabling paging...");

    asm volatile("cli");

    // Load CR3 (physical address of page directory)
    uint32_t pde_phys = (uint32_t)kernel_page_directory;
    debug("[VMM] Loading CR3 with 0x%x", pde_phys);
    asm volatile("mov %0, %%cr3" :: "r"(pde_phys));

    // Enable paging in CR0
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    debug("[VMM] Old CR0 = 0x%x", cr0);

    cr0 |= 0x80000000;  // Set PG bit
    cr0 |= 0x00000001;  // Ensure PE bit is set
    debug("[VMM] New CR0 = 0x%x", cr0);

    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");

    // Optional far jump
    asm volatile(
        "ljmp $0x08, $1f\n"
        "1:\n"
    );

    success("[VMM] Paging enabled successfully.");
}

void vmm_map(uint32_t virtual_addr, uint32_t physical_addr, int rw)
{
    debug("[VMM] Mapping vaddr=0x%x to paddr=0x%x, rw=%d", virtual_addr, physical_addr, rw);

    uint32_t pd_index = (virtual_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    uint32_t pde_val = kernel_page_directory[pd_index];
    if ((pde_val & 1) == 0) {
        error("[VMM] PDE[%d] not present!", pd_index);
        return;
    }

    uint32_t pt_phys_base = pde_val & 0xFFFFF000;
    uint32_t* pt_virt_base = (uint32_t*)pt_phys_base; // Identity-mapped

    uint32_t flags = (rw ? 0x3 : 0x1);
    pt_virt_base[pt_index] = (physical_addr & 0xFFFFF000) | flags;

    debug("[VMM] PT[%d] = 0x%x", pt_index, pt_virt_base[pt_index]);

    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
    success("[VMM] Mapping done.");
}
