pub mod heaptest;
pub mod memtest;
pub mod pagetest;

use crate::success;
use crate::test;

pub fn run_tests() {
    test!("Running memory manager tests...");
    if !memtest::test_allocation() {
        panic!("Memory allocation test failed!");
    } else {
        success!("Memory allocation test passed!");
    }

    if !memtest::test_free() {
        panic!("Memory free test failed!");
    } else {
        success!("Memory free test passed!");
    }

    if !memtest::test_multiple_allocations() {
        panic!("Memory multiple allocations test failed!");
    } else {
        success!("Memory multiple allocations test passed!");
    }

    pagetest::run();
    heaptest::run();
}
