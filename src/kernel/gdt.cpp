// gdt.cpp
#include <stdint.h>
#include "kernel/gdt.h"
#include "kernel/debug.h"


// We'll define 5 segments: Null, Kernel Code, Kernel Data, User Code, User Data.
static GDTEntry gdt[5];
static GDTPtr   gdt_ptr;


// Set one GDT entry in our table.
static void set_gdt_entry(int index, 
                          uint32_t base, 
                          uint32_t limit,
                          uint8_t access,
                          uint8_t gran)
{
    // Limit
    gdt[index].limit_low    = (limit & 0xFFFF);
    gdt[index].granularity  = (limit >> 16) & 0x0F;

    // Base
    gdt[index].base_low     = (base & 0xFFFF);
    gdt[index].base_mid     = (base >> 16) & 0xFF;
    gdt[index].base_high    = (base >> 24) & 0xFF;

    gdt[index].access       = access;
    gdt[index].granularity |= (gran & 0xF0);
}

extern "C" void gdt_flush(uint32_t gdt_ptr_addr)
{
    asm volatile (
        // 1) Load the GDT pointer
        "lgdt (%0)\n\t"

        // 2) Reload data segment registers with 0x10 (index 2 in your GDT)
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"

        // 3) Far jump to reload CS with 0x08 (index 1 in your GDT)
        "pushl $0x08\n\t"               // code segment selector
        "pushl $1f\n\t"
        "retf\n\t"                      // far return => loads CS=0x08

        "1:\n\t"
        // Now we're back with CS=0x08
        :
        : "r" (gdt_ptr_addr)            // input operand: pointer to GDT descriptor
        : "memory", "eax"
    );
}


// Initialize our GDT with five entries: 
// 0 = null, 
// 1 = kernel code, 
// 2 = kernel data, 
// 3 = user code, 
// 4 = user data.
void init_gdt()
{
    // 1) Null segment
    set_gdt_entry(0, 0, 0, 0, 0);

    // 2) Kernel code (index 1)
    // Base=0, Limit=4GB => 0xFFFFF, 
    // Access=0x9A=10011010b (present=1, ring=0, code=1),
    // Gran=0xCF=11001111b (4KB gran, 32-bit op size).
    set_gdt_entry(1, 
                  0, 
                  0xFFFFF,   // 4 GB limit in pages
                  0x9A,      // ring0 code, present, exec/read
                  0xCF);     // page granularity, 32-bit, limit high=0xF

    // 3) Kernel data (index 2)
    // Access=0x92=10010010b (present=1, ring=0, data=1, writable=1),
    set_gdt_entry(2, 
                  0, 
                  0xFFFFF,
                  0x92,
                  0xCF);

    // 4) User code (index 3)
    // Same limit & gran, but ring=3 => Access=0xFA=11111010b
    set_gdt_entry(3, 
                  0, 
                  0xFFFFF,
                  0xFA, // ring3 code
                  0xCF);

    // 5) User data (index 4)
    // ring=3 => Access=0xF2=11110010b
    set_gdt_entry(4, 
                  0, 
                  0xFFFFF,
                  0xF2, // ring3 data
                  0xCF);

    // Populate the GDTPtr
    gdt_ptr.limit = (sizeof(gdt) - 1);
    gdt_ptr.base  = (uint32_t)&gdt;
    debug("[GDT] Base=0x%x, Limit=0x%x\n", gdt_ptr.base, gdt_ptr.limit);

    // Flush the GDT with our assembly function
    gdt_flush((uint32_t)&gdt_ptr);
}
