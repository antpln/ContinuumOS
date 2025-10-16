#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// PCI Configuration Space Registers
#define PCI_CONFIG_ADDRESS    0x0CF8
#define PCI_CONFIG_DATA       0x0CFC

// PCI Configuration Space Offsets
#define PCI_VENDOR_ID         0x00
#define PCI_DEVICE_ID         0x02
#define PCI_COMMAND           0x04
#define PCI_STATUS            0x06
#define PCI_REVISION_ID       0x08
#define PCI_PROG_IF           0x09
#define PCI_SUBCLASS          0x0A
#define PCI_CLASS             0x0B
#define PCI_CACHE_LINE_SIZE   0x0C
#define PCI_LATENCY_TIMER     0x0D
#define PCI_HEADER_TYPE       0x0E
#define PCI_BIST              0x0F
#define PCI_BAR0              0x10
#define PCI_BAR1              0x14
#define PCI_BAR2              0x18
#define PCI_BAR3              0x1C
#define PCI_BAR4              0x20
#define PCI_BAR5              0x24
#define PCI_INTERRUPT_LINE    0x3C
#define PCI_INTERRUPT_PIN     0x3D

// PCI Class Codes
#define PCI_CLASS_UNCLASSIFIED        0x00
#define PCI_CLASS_MASS_STORAGE        0x01
#define PCI_CLASS_NETWORK             0x02
#define PCI_CLASS_DISPLAY             0x03
#define PCI_CLASS_MULTIMEDIA          0x04
#define PCI_CLASS_MEMORY              0x05
#define PCI_CLASS_BRIDGE              0x06
#define PCI_CLASS_SIMPLE_COMM         0x07
#define PCI_CLASS_BASE_SYSTEM         0x08
#define PCI_CLASS_INPUT               0x09
#define PCI_CLASS_DOCKING             0x0A
#define PCI_CLASS_PROCESSOR           0x0B
#define PCI_CLASS_SERIAL_BUS          0x0C
#define PCI_CLASS_WIRELESS            0x0D
#define PCI_CLASS_INTELLIGENT_IO      0x0E
#define PCI_CLASS_SATELLITE           0x0F
#define PCI_CLASS_ENCRYPTION          0x10
#define PCI_CLASS_SIGNAL_PROCESSING   0x11
#define PCI_CLASS_COPROCESSOR         0x40

// PCI Network Controller Subclasses
#define PCI_SUBCLASS_NET_ETHERNET     0x00
#define PCI_SUBCLASS_NET_TOKEN_RING   0x01
#define PCI_SUBCLASS_NET_FDDI         0x02
#define PCI_SUBCLASS_NET_ATM          0x03
#define PCI_SUBCLASS_NET_ISDN         0x04
#define PCI_SUBCLASS_NET_OTHER        0x80

// PCI Device Structure
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision_id;
    uint8_t  header_type;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint32_t bar[6];
} pci_device_t;

// PCI Functions
void pci_init(void);
uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

// Device scanning
void pci_scan_bus(void);
void pci_list_devices(void);
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pci_find_device_by_class(uint8_t class_code, uint8_t subclass);
int pci_get_device_count(void);
pci_device_t* pci_get_device(int index);

// Device class to string
const char* pci_class_to_string(uint8_t class_code);
const char* pci_subclass_to_string(uint8_t class_code, uint8_t subclass);

// PCI event handling for processes
struct Process;
void pci_register_process_listener(struct Process* proc, uint16_t vendor_id, uint16_t device_id);
void pci_unregister_process_listener(struct Process* proc);
void pci_notify_device_ready(uint8_t bus, uint8_t device, uint8_t function);
void pci_notify_interrupt(uint8_t bus, uint8_t device, uint8_t function);

#ifdef __cplusplus
}
#endif

#endif // _KERNEL_PCI_H
