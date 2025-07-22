use crate::heap;
use crate::test;

pub fn run() {
    test!("Running Heap (kmalloc/kfree) Test...");
    unsafe {
        let ptr1 = heap::kmalloc(64);
        test!("Allocated 64 bytes at {:p}", ptr1);
        let ptr2 = heap::kmalloc(128);
        test!("Allocated 128 bytes at {:p}", ptr2);
        let ptr3 = heap::kmalloc(32);
        test!("Allocated 32 bytes at {:p}", ptr3);
        if ptr1.is_null() || ptr2.is_null() || ptr3.is_null() {
            panic!("Heap allocation failed");
        }
        if ptr1 < ptr2 && ptr2 < ptr3 {
            test!("Allocations do not overlap and are correctly ordered.");
        } else {
            panic!("Allocations overlap or are out of order!");
        }
        heap::kfree(ptr2);
        test!("Freed second allocation at {:p}", ptr2);
        let ptr4 = heap::kmalloc(64);
        test!("Allocated 64 bytes at {:p}", ptr4);
        if ptr4 == ptr2 {
            test!("Freed memory was reused correctly.");
        } else {
            panic!("Freed memory was not reused properly!");
        }
        heap::kfree(ptr1);
        heap::kfree(ptr3);
        heap::kfree(ptr4);
        test!("Freed all allocations.");
        let ptr5 = heap::kmalloc(128);
        test!("Allocated 128 bytes at {:p}", ptr5);
        if ptr5 == ptr1 {
            test!("Free block merging works correctly.");
        } else {
            panic!("Free block merging failed!");
        }
        heap::kfree(ptr5);
        test!("Heap test completed.");
    }
}
