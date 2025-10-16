#include <kernel/pci.h>
#include <kernel/port_io.h>
#include <kernel/process.h>
#include <stdio.h>
#include <string.h>
#include <sys/events.h>

#define MAX_PCI_DEVICES 64
#define MAX_PCI_LISTENERS 16

static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

// Process listener structure for PCI events
typedef struct {
    Process* proc;
    uint16_t vendor_id;  // 0xFFFF = match any
    uint16_t device_id;  // 0xFFFF = match any
} pci_listener_t;

static pci_listener_t pci_listeners[MAX_PCI_LISTENERS];
static int pci_listener_count = 0;

// Read a 32-bit value from PCI configuration space
uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (uint32_t)(
        (1 << 31) |                    // Enable bit
        ((uint32_t)bus << 16) |        // Bus number
        ((uint32_t)device << 11) |     // Device number
        ((uint32_t)function << 8) |    // Function number
        (offset & 0xFC)                // Register offset (must be 4-byte aligned)
    );
    
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

// Read a 16-bit value from PCI configuration space
uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, device, function, offset & 0xFC);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

// Read an 8-bit value from PCI configuration space
uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t dword = pci_read_config_dword(bus, device, function, offset & 0xFC);
    return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}

// Write a 32-bit value to PCI configuration space
void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        (offset & 0xFC)
    );
    
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

// Write a 16-bit value to PCI configuration space
void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t dword = pci_read_config_dword(bus, device, function, offset & 0xFC);
    uint8_t shift = (offset & 2) * 8;
    dword = (dword & ~(0xFFFF << shift)) | ((uint32_t)value << shift);
    pci_write_config_dword(bus, device, function, offset & 0xFC, dword);
}

// Write an 8-bit value to PCI configuration space
void pci_write_config_byte(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t dword = pci_read_config_dword(bus, device, function, offset & 0xFC);
    uint8_t shift = (offset & 3) * 8;
    dword = (dword & ~(0xFF << shift)) | ((uint32_t)value << shift);
    pci_write_config_dword(bus, device, function, offset & 0xFC, dword);
}

// Check if a device exists at the given location
static bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_read_config_word(bus, device, function, PCI_VENDOR_ID);
    return (vendor_id != 0xFFFF);
}

// Read device information
static void pci_read_device_info(pci_device_t* dev, uint8_t bus, uint8_t device, uint8_t function) {
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    
    dev->vendor_id = pci_read_config_word(bus, device, function, PCI_VENDOR_ID);
    dev->device_id = pci_read_config_word(bus, device, function, PCI_DEVICE_ID);
    dev->class_code = pci_read_config_byte(bus, device, function, PCI_CLASS);
    dev->subclass = pci_read_config_byte(bus, device, function, PCI_SUBCLASS);
    dev->prog_if = pci_read_config_byte(bus, device, function, PCI_PROG_IF);
    dev->revision_id = pci_read_config_byte(bus, device, function, PCI_REVISION_ID);
    dev->header_type = pci_read_config_byte(bus, device, function, PCI_HEADER_TYPE);
    dev->interrupt_line = pci_read_config_byte(bus, device, function, PCI_INTERRUPT_LINE);
    dev->interrupt_pin = pci_read_config_byte(bus, device, function, PCI_INTERRUPT_PIN);
    
    // Read BARs
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read_config_dword(bus, device, function, PCI_BAR0 + (i * 4));
    }
}

// Scan for PCI devices
void pci_scan_bus(void) {
    pci_device_count = 0;
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            // Check function 0 first
            if (!pci_device_exists(bus, device, 0)) {
                continue;
            }
            
            // Read header type to check if multi-function
            uint8_t header_type = pci_read_config_byte(bus, device, 0, PCI_HEADER_TYPE);
            uint8_t max_functions = (header_type & 0x80) ? 8 : 1;
            
            for (uint8_t function = 0; function < max_functions; function++) {
                if (!pci_device_exists(bus, device, function)) {
                    continue;
                }
                
                if (pci_device_count >= MAX_PCI_DEVICES) {
                    printf("PCI: Too many devices, reached limit of %d\n", MAX_PCI_DEVICES);
                    return;
                }
                
                pci_read_device_info(&pci_devices[pci_device_count], bus, device, function);
                pci_device_count++;
            }
        }
    }
}

// Get class name string
const char* pci_class_to_string(uint8_t class_code) {
    switch (class_code) {
        case PCI_CLASS_UNCLASSIFIED: return "Unclassified";
        case PCI_CLASS_MASS_STORAGE: return "Mass Storage";
        case PCI_CLASS_NETWORK: return "Network";
        case PCI_CLASS_DISPLAY: return "Display";
        case PCI_CLASS_MULTIMEDIA: return "Multimedia";
        case PCI_CLASS_MEMORY: return "Memory";
        case PCI_CLASS_BRIDGE: return "Bridge";
        case PCI_CLASS_SIMPLE_COMM: return "Communication";
        case PCI_CLASS_BASE_SYSTEM: return "Base System";
        case PCI_CLASS_INPUT: return "Input";
        case PCI_CLASS_DOCKING: return "Docking";
        case PCI_CLASS_PROCESSOR: return "Processor";
        case PCI_CLASS_SERIAL_BUS: return "Serial Bus";
        case PCI_CLASS_WIRELESS: return "Wireless";
        case PCI_CLASS_INTELLIGENT_IO: return "Intelligent I/O";
        case PCI_CLASS_SATELLITE: return "Satellite";
        case PCI_CLASS_ENCRYPTION: return "Encryption";
        case PCI_CLASS_SIGNAL_PROCESSING: return "Signal Processing";
        case PCI_CLASS_COPROCESSOR: return "Coprocessor";
        default: return "Unknown";
    }
}

// Get subclass name string
const char* pci_subclass_to_string(uint8_t class_code, uint8_t subclass) {
    if (class_code == PCI_CLASS_NETWORK) {
        switch (subclass) {
            case PCI_SUBCLASS_NET_ETHERNET: return "Ethernet";
            case PCI_SUBCLASS_NET_TOKEN_RING: return "Token Ring";
            case PCI_SUBCLASS_NET_FDDI: return "FDDI";
            case PCI_SUBCLASS_NET_ATM: return "ATM";
            case PCI_SUBCLASS_NET_ISDN: return "ISDN";
            case PCI_SUBCLASS_NET_OTHER: return "Other Network";
            default: return "Unknown Network";
        }
    } else if (class_code == PCI_CLASS_DISPLAY) {
        switch (subclass) {
            case 0x00: return "VGA";
            case 0x01: return "XGA";
            case 0x02: return "3D";
            case 0x80: return "Other Display";
            default: return "Unknown Display";
        }
    } else if (class_code == PCI_CLASS_MASS_STORAGE) {
        switch (subclass) {
            case 0x00: return "SCSI";
            case 0x01: return "IDE";
            case 0x02: return "Floppy";
            case 0x03: return "IPI";
            case 0x04: return "RAID";
            case 0x05: return "ATA";
            case 0x06: return "SATA";
            case 0x07: return "SAS";
            case 0x08: return "NVMe";
            case 0x80: return "Other Storage";
            default: return "Unknown Storage";
        }
    } else if (class_code == PCI_CLASS_BRIDGE) {
        switch (subclass) {
            case 0x00: return "Host Bridge";
            case 0x01: return "ISA Bridge";
            case 0x02: return "EISA Bridge";
            case 0x03: return "MCA Bridge";
            case 0x04: return "PCI-to-PCI Bridge";
            case 0x05: return "PCMCIA Bridge";
            case 0x06: return "NuBus Bridge";
            case 0x07: return "CardBus Bridge";
            case 0x08: return "RACEway Bridge";
            case 0x80: return "Other Bridge";
            default: return "Unknown Bridge";
        }
    }
    
    return "";
}

// List all detected PCI devices
void pci_list_devices(void) {
    printf("\n=== PCI Devices ===\n");
    printf("Bus Dev Fn Vendor Device Class                      Subclass\n");
    printf("----------------------------------------------------------------------\n");
    
    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t* dev = &pci_devices[i];
        const char* class_name = pci_class_to_string(dev->class_code);
        const char* subclass_name = pci_subclass_to_string(dev->class_code, dev->subclass);
        
        printf("%02x  %02x  %x  %04x   %04x   %-24s %s\n",
               dev->bus,
               dev->device,
               dev->function,
               dev->vendor_id,
               dev->device_id,
               class_name,
               subclass_name);
        
        // Show BARs if they're set
        bool has_bars = false;
        for (int j = 0; j < 6; j++) {
            if (dev->bar[j] != 0 && dev->bar[j] != 0xFFFFFFFF) {
                has_bars = true;
                break;
            }
        }
        
        if (has_bars) {
            printf("       BARs: ");
            for (int j = 0; j < 6; j++) {
                if (dev->bar[j] != 0 && dev->bar[j] != 0xFFFFFFFF) {
                    printf("BAR%d=0x%08x ", j, dev->bar[j]);
                }
            }
            printf("\n");
        }
        
        if (dev->interrupt_line != 0xFF) {
            printf("       IRQ: %d", dev->interrupt_line);
            if (dev->interrupt_pin != 0) {
                printf(" (PIN: %c)", 'A' + dev->interrupt_pin - 1);
            }
            printf("\n");
        }
    }
    
    printf("----------------------------------------------------------------------\n");
    printf("Total: %d device(s)\n\n", pci_device_count);
}

// Find a device by vendor and device ID
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Find a device by class and subclass
pci_device_t* pci_find_device_by_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && 
            pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Get device count
int pci_get_device_count(void) {
    return pci_device_count;
}

// Get device by index
pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}

// Initialize PCI subsystem
void pci_init(void) {
    printf("PCI: Initializing PCI subsystem...\n");
    pci_listener_count = 0;
    pci_scan_bus();
    printf("PCI: Found %d device(s)\n", pci_device_count);
}

// Register a process to receive PCI events
void pci_register_process_listener(Process* proc, uint16_t vendor_id, uint16_t device_id) {
    if (!proc) {
        return;
    }
    
    if (pci_listener_count >= MAX_PCI_LISTENERS) {
        printf("PCI: Too many listeners, cannot register process %s\n", proc->name);
        return;
    }
    
    // Check if already registered
    for (int i = 0; i < pci_listener_count; i++) {
        if (pci_listeners[i].proc == proc) {
            // Update filter
            pci_listeners[i].vendor_id = vendor_id;
            pci_listeners[i].device_id = device_id;
            return;
        }
    }
    
    // Add new listener
    pci_listeners[pci_listener_count].proc = proc;
    pci_listeners[pci_listener_count].vendor_id = vendor_id;
    pci_listeners[pci_listener_count].device_id = device_id;
    pci_listener_count++;
    
    // Notify about existing devices that match
    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t* dev = &pci_devices[i];
        
        // Check if device matches filter
        if ((vendor_id == 0xFFFF || dev->vendor_id == vendor_id) &&
            (device_id == 0xFFFF || dev->device_id == device_id)) {
            
            IOEvent event;
            event.type = EVENT_PCI;
            event.data.pci.bus = dev->bus;
            event.data.pci.device = dev->device;
            event.data.pci.function = dev->function;
            event.data.pci.vendor_id = dev->vendor_id;
            event.data.pci.device_id = dev->device_id;
            event.data.pci.class_code = dev->class_code;
            event.data.pci.subclass = dev->subclass;
            event.data.pci.event_type = PCI_EVENT_DEVICE_ADDED;
            
            push_io_event(proc, event);
        }
    }
}

// Unregister a process from receiving PCI events
void pci_unregister_process_listener(Process* proc) {
    if (!proc) {
        return;
    }
    
    for (int i = 0; i < pci_listener_count; i++) {
        if (pci_listeners[i].proc == proc) {
            // Remove by shifting remaining listeners
            for (int j = i; j < pci_listener_count - 1; j++) {
                pci_listeners[j] = pci_listeners[j + 1];
            }
            pci_listener_count--;
            return;
        }
    }
}

// Helper function to send event to all matching listeners
static void pci_send_event_to_listeners(pci_device_t* dev, int event_type) {
    for (int i = 0; i < pci_listener_count; i++) {
        pci_listener_t* listener = &pci_listeners[i];
        
        // Check if device matches filter
        if ((listener->vendor_id == 0xFFFF || dev->vendor_id == listener->vendor_id) &&
            (listener->device_id == 0xFFFF || dev->device_id == listener->device_id)) {
            
            IOEvent event;
            event.type = EVENT_PCI;
            event.data.pci.bus = dev->bus;
            event.data.pci.device = dev->device;
            event.data.pci.function = dev->function;
            event.data.pci.vendor_id = dev->vendor_id;
            event.data.pci.device_id = dev->device_id;
            event.data.pci.class_code = dev->class_code;
            event.data.pci.subclass = dev->subclass;
            event.data.pci.event_type = event_type;
            
            push_io_event(listener->proc, event);
        }
    }
}

// Notify that a device is ready (initialized by driver)
void pci_notify_device_ready(uint8_t bus, uint8_t device, uint8_t function) {
    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t* dev = &pci_devices[i];
        if (dev->bus == bus && dev->device == device && dev->function == function) {
            pci_send_event_to_listeners(dev, PCI_EVENT_DEVICE_READY);
            return;
        }
    }
}

// Notify that a device has generated an interrupt
void pci_notify_interrupt(uint8_t bus, uint8_t device, uint8_t function) {
    for (int i = 0; i < pci_device_count; i++) {
        pci_device_t* dev = &pci_devices[i];
        if (dev->bus == bus && dev->device == device && dev->function == function) {
            pci_send_event_to_listeners(dev, PCI_EVENT_INTERRUPT);
            return;
        }
    }
}
