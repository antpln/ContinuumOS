#include <stdio.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/tests/heaptest.h>

void heap_test() {
    test("\n[TEST] Running Heap (kmalloc/kfree) Test...\n");

    // Allocate three blocks
    void* ptr1 = kmalloc(64);
    test("[TEST] Allocated 64 bytes at %p\n", ptr1);

    void* ptr2 = kmalloc(128);
    test("[TEST] Allocated 128 bytes at %p\n", ptr2);

    void* ptr3 = kmalloc(32);
    test("[TEST] Allocated 32 bytes at %p\n", ptr3);

    // Check for overlapping allocations
    if (ptr1 && ptr2 && ptr3) {
        if (ptr1 < ptr2 && ptr2 < ptr3) {
            test("[PASS] Allocations do not overlap and are correctly ordered.\n");
        } else {
            PANIC("[FAIL] Allocations overlap or are out of order!\n");

        }
    } else {
        return;
    }

    // Free the second allocation and reallocate a smaller one
    kfree(ptr2);
    test("[TEST] Freed second allocation at %p\n", ptr2);

    void* ptr4 = kmalloc(64);
    test("[TEST] Allocated 64 bytes at %p\n", ptr4);
                
    // Check if freed memory is reused
    if (ptr4 == ptr2) {
        test("[PASS] Freed memory was reused correctly.\n");
    } else {
        PANIC("[FAIL] Freed memory was not reused properly!\n");
    }

    // Free all allocations
    kfree(ptr1);
    kfree(ptr3);
    kfree(ptr4);
    test("[TEST] Freed all allocations.\n");

    // Check merging
    void* ptr5 = kmalloc(128);
    test("[TEST] Allocated 128 bytes at %p\n", ptr5);

    if (ptr5 == ptr1) {
        test("[PASS] Free block merging works correctly.\n");
    } else {
        PANIC("[FAIL] Free block merging failed!\n");
    }

    test("[TEST] Heap test completed.\n");
}
