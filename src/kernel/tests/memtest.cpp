#include <kernel/tests/memtest.h>
#include <kernel/memory.h>

bool MemoryTester::test_allocation() {
    void* frame = PhysicalMemoryManager::allocate_frame();
    if (!frame) return false;
    
    // Test if we can write to and read from the allocated memory
    uint32_t* ptr = (uint32_t*)frame;
    *ptr = TEST_PATTERN;
    
    bool success = (*ptr == TEST_PATTERN);
    PhysicalMemoryManager::free_frame(frame);
    return success;
}

bool MemoryTester::test_free() {
    void* frame1 = PhysicalMemoryManager::allocate_frame();
    if (!frame1) return false;
    
    PhysicalMemoryManager::free_frame(frame1);
    
    // Try to allocate again - should get the same frame
    void* frame2 = PhysicalMemoryManager::allocate_frame();
    bool success = (frame1 == frame2);
    
    PhysicalMemoryManager::free_frame(frame2);
    return success;
}

bool MemoryTester::test_multiple_allocations() {
    const int NUM_ALLOCATIONS = 10;
    void* frames[NUM_ALLOCATIONS];
    
    // Allocate multiple frames
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        frames[i] = PhysicalMemoryManager::allocate_frame();
        if (!frames[i]) return false;
        
        // Write unique pattern to each frame
        uint32_t* ptr = (uint32_t*)frames[i];
        *ptr = TEST_PATTERN + i;
    }
    
    // Verify patterns
    bool success = true;
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        uint32_t* ptr = (uint32_t*)frames[i];
        if (*ptr != TEST_PATTERN + i) {
            success = false;
            break;
        }
    }
    
    // Free all frames
    for (int i = 0; i < NUM_ALLOCATIONS; i++) {
        PhysicalMemoryManager::free_frame(frames[i]);
    }
    
    return success;
}

bool MemoryTester::test_boundary_conditions() {
    // Try to allocate up to MAX_FRAMES frames
    const size_t MAX_FRAMES = 1024; // Adjust as needed for your environment
    void* frames[MAX_FRAMES];
    size_t allocated = 0;
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        frames[i] = PhysicalMemoryManager::allocate_frame();
        if (!frames[i]) break;
        allocated++;
    }
    bool success = (allocated > 0);
    // Free all frames
    for (size_t i = 0; i < allocated; i++) {
        PhysicalMemoryManager::free_frame(frames[i]);
    }
    return success;
}