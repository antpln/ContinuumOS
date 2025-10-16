#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

// For the PDE/PTE arrays
#define PAGE_SIZE    4096
#define PDE_ENTRIES  1024
#define PTE_ENTRIES  1024

#ifdef __cplusplus
extern "C" {
#endif

void vmm_init();
void vmm_enable();

/*
 * vmm_map: Example function to map one page [virt -> phys].
 * In identity mapping, virt == phys, so you may not need this.
 * But we show it for completeness.
 */
void vmm_map(uint32_t virtual_addr, uint32_t physical_addr, int rw);
void vmm_map_range(uint32_t virtual_addr, uint32_t physical_addr, uint32_t size, int rw);

#ifdef __cplusplus
}
#endif

#endif
