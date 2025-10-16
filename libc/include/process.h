#ifndef LIBC_PROCESS_H
#define LIBC_PROCESS_H

#include <stdint.h>
#include <sys/events.h>

#ifdef __cplusplus
extern "C" {
#endif

// Yield execution to the scheduler
void yield();
// Yield and wait for a specific event (hook)
void yield_for_event(int hook_type, uint64_t trigger_value);
// Start a new process
int start_process(const char* name, void (*entry)(), int speculative, uint32_t stack_size);
// Poll for an IO event without blocking (returns 1 if event populated, 0 otherwise)
int process_poll_event(IOEvent* event);
// Wait for an IO event (blocks cooperatively until an event arrives)
int process_wait_event(IOEvent* event);
// Terminate the current process with the given status code
void process_exit(int status);

// PCI event handling
// Register to receive PCI events (vendor_id=0xFFFF and device_id=0xFFFF for all devices)
void pci_register_listener(uint16_t vendor_id, uint16_t device_id);
// Unregister from receiving PCI events
void pci_unregister_listener();

#ifdef __cplusplus
}
#endif

#endif // LIBC_PROCESS_H
