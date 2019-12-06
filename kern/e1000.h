
#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// E1000 is Intel-82540EM

#define E1000_VEND     0x8086  // Vendor ID for Intel 
#define E1000_DEV      0x100E  // Device ID for the e1000 Qemu, Bochs, and VirtualBox emmulated NICs
#define QEMU_MAC_ADDR  0x563412005452

extern int pci_e1000_attach(struct pci_func *pcif);
extern int e1000_transmit(const char *buf, size_t len);
extern int e1000_receive(char *buf, size_t len);
extern uint64_t e1000_read_mac_addr(void);
extern uint8_t e1000_irq_line;

#endif  // SOL >= 6
