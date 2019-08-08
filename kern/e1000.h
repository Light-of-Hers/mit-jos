
#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// E1000 is Intel-82540EM

#define E1000_VEND     0x8086  // Vendor ID for Intel 
#define E1000_DEV      0x100E  // Device ID for the e1000 Qemu, Bochs, and VirtualBox emmulated NICs

/* Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and  should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */
#define E1000_REG_CTRL     0x00000  /* Device Control - RW */
#define E1000_REG_CTRL_DUP 0x00004  /* Device Control Duplicate (Shadow) - RW */
#define E1000_REG_STATUS   0x00008  /* Device Status - RO */
#define E1000_REG_EECD     0x00010  /* EEPROM/Flash Control - RW */
#define E1000_REG_EERD     0x00014  /* EEPROM Read - RW */
#define E1000_REG_CTRL_EXT 0x00018  /* Extended Device Control - RW */
#define E1000_REG_FLA      0x0001C  /* Flash Access - RW */
#define E1000_REG_MDIC     0x00020  /* MDI Control - RW */
#define E1000_REG_SCTL     0x00024  /* SerDes Control - RW */
#define E1000_REG_FEXTNVM  0x00028  /* Future Extended NVM register */
#define E1000_REG_FCAL     0x00028  /* Flow Control Address Low - RW */
#define E1000_REG_FCAH     0x0002C  /* Flow Control Address High -RW */
#define E1000_REG_FCT      0x00030  /* Flow Control Type - RW */
#define E1000_REG_VET      0x00038  /* VLAN Ether Type - RW */
#define E1000_REG_ICR      0x000C0  /* Interrupt Cause Read - R/clr */
#define E1000_REG_ITR      0x000C4  /* Interrupt Throttling Rate - RW */
#define E1000_REG_ICS      0x000C8  /* Interrupt Cause Set - WO */
#define E1000_REG_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_REG_IMC      0x000D8  /* Interrupt Mask Clear - WO */
#define E1000_REG_IAM      0x000E0  /* Interrupt Acknowledge Auto Mask */
#define E1000_REG_RCTL     0x00100  /* RX Control - RW */
#define E1000_REG_RDTR1    0x02820  /* RX Delay Timer (1) - RW */
#define E1000_REG_RDBAL1   0x02900  /* RX Descriptor Base Address Low (1) - RW */
#define E1000_REG_RDBAH1   0x02904  /* RX Descriptor Base Address High (1) - RW */
#define E1000_REG_RDLEN1   0x02908  /* RX Descriptor Length (1) - RW */
#define E1000_REG_RDH1     0x02910  /* RX Descriptor Head (1) - RW */
#define E1000_REG_RDT1     0x02918  /* RX Descriptor Tail (1) - RW */
#define E1000_REG_FCTTV    0x00170  /* Flow Control Transmit Timer Value - RW */
#define E1000_REG_TXCW     0x00178  /* TX Configuration Word - RW */
#define E1000_REG_RXCW     0x00180  /* RX Configuration Word - RO */
#define E1000_REG_TCTL     0x00400  /* TX Control - RW */
#define E1000_REG_TCTL_EXT 0x00404  /* Extended TX Control - RW */
#define E1000_REG_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_REG_TBT      0x00448  /* TX Burst Timer - RW */
#define E1000_REG_AIT      0x00458  /* Adaptive Interframe Spacing Throttle - RW */
#define E1000_REG_LEDCTL   0x00E00  /* LED Control - RW */
#define E1000_REG_EXTCNF_CTRL  0x00F00  /* Extended Configuration Control */
#define E1000_REG_EXTCNF_SIZE  0x00F08  /* Extended Configuration Size */
#define E1000_REG_PHY_CTRL     0x00F10  /* PHY Control Register in CSR */
#define FEXTNVM_SW_CONFIG  0x0001
#define E1000_REG_PBA      0x01000  /* Packet Buffer Allocation - RW */
#define E1000_REG_PBS      0x01008  /* Packet Buffer Size */
#define E1000_REG_EEMNGCTL 0x01010  /* MNG EEprom Control */
#define E1000_REG_FLASH_UPDATES 1000
#define E1000_REG_EEARBC   0x01024  /* EEPROM Auto Read Bus Control */
#define E1000_REG_FLASHT   0x01028  /* FLASH Timer Register */
#define E1000_REG_EEWR     0x0102C  /* EEPROM Write Register - RW */
#define E1000_REG_FLSWCTL  0x01030  /* FLASH control register */
#define E1000_REG_FLSWDATA 0x01034  /* FLASH data register */
#define E1000_REG_FLSWCNT  0x01038  /* FLASH Access Counter */
#define E1000_REG_FLOP     0x0103C  /* FLASH Opcode Register */
#define E1000_REG_ERT      0x02008  /* Early Rx Threshold - RW */
#define E1000_REG_FCRTL    0x02160  /* Flow Control Receive Threshold Low - RW */
#define E1000_REG_FCRTH    0x02168  /* Flow Control Receive Threshold High - RW */
#define E1000_REG_PSRCTL   0x02170  /* Packet Split Receive Control - RW */
#define E1000_REG_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_REG_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_REG_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_REG_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_REG_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_REG_RDTR     0x02820  /* RX Delay Timer - RW */
#define E1000_REG_RDBAL0   E1000_REG_RDBAL /* RX Desc Base Address Low (0) - RW */
#define E1000_REG_RDBAH0   E1000_REG_RDBAH /* RX Desc Base Address High (0) - RW */
#define E1000_REG_RDLEN0   E1000_REG_RDLEN /* RX Desc Length (0) - RW */
#define E1000_REG_RDH0     E1000_REG_RDH   /* RX Desc Head (0) - RW */
#define E1000_REG_RDT0     E1000_REG_RDT   /* RX Desc Tail (0) - RW */
#define E1000_REG_RDTR0    E1000_REG_RDTR  /* RX Delay Timer (0) - RW */
#define E1000_REG_RXDCTL   0x02828  /* RX Descriptor Control queue 0 - RW */
#define E1000_REG_RXDCTL1  0x02928  /* RX Descriptor Control queue 1 - RW */
#define E1000_REG_RADV     0x0282C  /* RX Interrupt Absolute Delay Timer - RW */
#define E1000_REG_RSRPD    0x02C00  /* RX Small Packet Detect - RW */
#define E1000_REG_RAID     0x02C08  /* Receive Ack Interrupt Delay - RW */
#define E1000_REG_TXDMAC   0x03000  /* TX DMA Control - RW */
#define E1000_REG_KABGTXD  0x03004  /* AFE Band Gap Transmit Ref Data */
#define E1000_REG_TDFH     0x03410  /* TX Data FIFO Head - RW */
#define E1000_REG_TDFT     0x03418  /* TX Data FIFO Tail - RW */
#define E1000_REG_TDFHS    0x03420  /* TX Data FIFO Head Saved - RW */
#define E1000_REG_TDFTS    0x03428  /* TX Data FIFO Tail Saved - RW */
#define E1000_REG_TDFPC    0x03430  /* TX Data FIFO Packet Count - RW */
#define E1000_REG_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_REG_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_REG_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_REG_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_REG_TDT      0x03818  /* TX Descripotr Tail - RW */
#define E1000_REG_TIDV     0x03820  /* TX Interrupt Delay Value - RW */
#define E1000_REG_TXDCTL   0x03828  /* TX Descriptor Control - RW */
#define E1000_REG_TADV     0x0382C  /* TX Interrupt Absolute Delay Val - RW */
#define E1000_REG_TSPMT    0x03830  /* TCP Segmentation PAD & Min Threshold - RW */
#define E1000_REG_TARC0    0x03840  /* TX Arbitration Count (0) */
#define E1000_REG_TDBAL1   0x03900  /* TX Desc Base Address Low (1) - RW */
#define E1000_REG_TDBAH1   0x03904  /* TX Desc Base Address High (1) - RW */
#define E1000_REG_TDLEN1   0x03908  /* TX Desc Length (1) - RW */
#define E1000_REG_TDH1     0x03910  /* TX Desc Head (1) - RW */
#define E1000_REG_TDT1     0x03918  /* TX Desc Tail (1) - RW */
#define E1000_REG_TXDCTL1  0x03928  /* TX Descriptor Control (1) - RW */
#define E1000_REG_TARC1    0x03940  /* TX Arbitration Count (1) */
#define E1000_REG_CRCERRS  0x04000  /* CRC Error Count - R/clr */
#define E1000_REG_ALGNERRC 0x04004  /* Alignment Error Count - R/clr */
#define E1000_REG_SYMERRS  0x04008  /* Symbol Error Count - R/clr */
#define E1000_REG_RXERRC   0x0400C  /* Receive Error Count - R/clr */
#define E1000_REG_MPC      0x04010  /* Missed Packet Count - R/clr */
#define E1000_REG_SCC      0x04014  /* Single Collision Count - R/clr */
#define E1000_REG_ECOL     0x04018  /* Excessive Collision Count - R/clr */
#define E1000_REG_MCC      0x0401C  /* Multiple Collision Count - R/clr */
#define E1000_REG_LATECOL  0x04020  /* Late Collision Count - R/clr */
#define E1000_REG_COLC     0x04028  /* Collision Count - R/clr */
#define E1000_REG_DC       0x04030  /* Defer Count - R/clr */
#define E1000_REG_TNCRS    0x04034  /* TX-No CRS - R/clr */
#define E1000_REG_SEC      0x04038  /* Sequence Error Count - R/clr */
#define E1000_REG_CEXTERR  0x0403C  /* Carrier Extension Error Count - R/clr */
#define E1000_REG_RLEC     0x04040  /* Receive Length Error Count - R/clr */
#define E1000_REG_XONRXC   0x04048  /* XON RX Count - R/clr */
#define E1000_REG_XONTXC   0x0404C  /* XON TX Count - R/clr */
#define E1000_REG_XOFFRXC  0x04050  /* XOFF RX Count - R/clr */
#define E1000_REG_XOFFTXC  0x04054  /* XOFF TX Count - R/clr */
#define E1000_REG_FCRUC    0x04058  /* Flow Control RX Unsupported Count- R/clr */
#define E1000_REG_PRC64    0x0405C  /* Packets RX (64 bytes) - R/clr */
#define E1000_REG_PRC127   0x04060  /* Packets RX (65-127 bytes) - R/clr */
#define E1000_REG_PRC255   0x04064  /* Packets RX (128-255 bytes) - R/clr */
#define E1000_REG_PRC511   0x04068  /* Packets RX (255-511 bytes) - R/clr */
#define E1000_REG_PRC1023  0x0406C  /* Packets RX (512-1023 bytes) - R/clr */
#define E1000_REG_PRC1522  0x04070  /* Packets RX (1024-1522 bytes) - R/clr */
#define E1000_REG_GPRC     0x04074  /* Good Packets RX Count - R/clr */
#define E1000_REG_BPRC     0x04078  /* Broadcast Packets RX Count - R/clr */
#define E1000_REG_MPRC     0x0407C  /* Multicast Packets RX Count - R/clr */
#define E1000_REG_GPTC     0x04080  /* Good Packets TX Count - R/clr */
#define E1000_REG_GORCL    0x04088  /* Good Octets RX Count Low - R/clr */
#define E1000_REG_GORCH    0x0408C  /* Good Octets RX Count High - R/clr */
#define E1000_REG_GOTCL    0x04090  /* Good Octets TX Count Low - R/clr */
#define E1000_REG_GOTCH    0x04094  /* Good Octets TX Count High - R/clr */
#define E1000_REG_RNBC     0x040A0  /* RX No Buffers Count - R/clr */
#define E1000_REG_RUC      0x040A4  /* RX Undersize Count - R/clr */
#define E1000_REG_RFC      0x040A8  /* RX Fragment Count - R/clr */
#define E1000_REG_ROC      0x040AC  /* RX Oversize Count - R/clr */
#define E1000_REG_RJC      0x040B0  /* RX Jabber Count - R/clr */
#define E1000_REG_MGTPRC   0x040B4  /* Management Packets RX Count - R/clr */
#define E1000_REG_MGTPDC   0x040B8  /* Management Packets Dropped Count - R/clr */
#define E1000_REG_MGTPTC   0x040BC  /* Management Packets TX Count - R/clr */
#define E1000_REG_TORL     0x040C0  /* Total Octets RX Low - R/clr */
#define E1000_REG_TORH     0x040C4  /* Total Octets RX High - R/clr */
#define E1000_REG_TOTL     0x040C8  /* Total Octets TX Low - R/clr */
#define E1000_REG_TOTH     0x040CC  /* Total Octets TX High - R/clr */
#define E1000_REG_TPR      0x040D0  /* Total Packets RX - R/clr */
#define E1000_REG_TPT      0x040D4  /* Total Packets TX - R/clr */
#define E1000_REG_PTC64    0x040D8  /* Packets TX (64 bytes) - R/clr */
#define E1000_REG_PTC127   0x040DC  /* Packets TX (65-127 bytes) - R/clr */
#define E1000_REG_PTC255   0x040E0  /* Packets TX (128-255 bytes) - R/clr */
#define E1000_REG_PTC511   0x040E4  /* Packets TX (256-511 bytes) - R/clr */
#define E1000_REG_PTC1023  0x040E8  /* Packets TX (512-1023 bytes) - R/clr */
#define E1000_REG_PTC1522  0x040EC  /* Packets TX (1024-1522 Bytes) - R/clr */
#define E1000_REG_MPTC     0x040F0  /* Multicast Packets TX Count - R/clr */
#define E1000_REG_BPTC     0x040F4  /* Broadcast Packets TX Count - R/clr */
#define E1000_REG_TSCTC    0x040F8  /* TCP Segmentation Context TX - R/clr */
#define E1000_REG_TSCTFC   0x040FC  /* TCP Segmentation Context TX Fail - R/clr */
#define E1000_REG_IAC      0x04100  /* Interrupt Assertion Count */
#define E1000_REG_ICRXPTC  0x04104  /* Interrupt Cause Rx Packet Timer Expire Count */
#define E1000_REG_ICRXATC  0x04108  /* Interrupt Cause Rx Absolute Timer Expire Count */
#define E1000_REG_ICTXPTC  0x0410C  /* Interrupt Cause Tx Packet Timer Expire Count */
#define E1000_REG_ICTXATC  0x04110  /* Interrupt Cause Tx Absolute Timer Expire Count */
#define E1000_REG_ICTXQEC  0x04118  /* Interrupt Cause Tx Queue Empty Count */
#define E1000_REG_ICTXQMTC 0x0411C  /* Interrupt Cause Tx Queue Minimum Threshold Count */
#define E1000_REG_ICRXDMTC 0x04120  /* Interrupt Cause Rx Descriptor Minimum Threshold Count */
#define E1000_REG_ICRXOC   0x04124  /* Interrupt Cause Receiver Overrun Count */
#define E1000_REG_RXCSUM   0x05000  /* RX Checksum Control - RW */
#define E1000_REG_RFCTL    0x05008  /* Receive Filter Control*/
#define E1000_REG_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_REG_RA       0x05400  /* Receive Address - RW Array */
#define E1000_REG_VFTA     0x05600  /* VLAN Filter Table Array - RW Array */
#define E1000_REG_WUC      0x05800  /* Wakeup Control - RW */
#define E1000_REG_WUFC     0x05808  /* Wakeup Filter Control - RW */
#define E1000_REG_WUS      0x05810  /* Wakeup Status - RO */
#define E1000_REG_MANC     0x05820  /* Management Control - RW */
#define E1000_REG_IPAV     0x05838  /* IP Address Valid - RW */
#define E1000_REG_IP4AT    0x05840  /* IPv4 Address Table - RW Array */
#define E1000_REG_IP6AT    0x05880  /* IPv6 Address Table - RW Array */
#define E1000_REG_WUPL     0x05900  /* Wakeup Packet Length - RW */
#define E1000_REG_WUPM     0x05A00  /* Wakeup Packet Memory - RO A */
#define E1000_REG_FFLT     0x05F00  /* Flexible Filter Length Table - RW Array */
#define E1000_REG_HOST_IF  0x08800  /* Host Interface */
#define E1000_REG_FFMT     0x09000  /* Flexible Filter Mask Table - RW Array */
#define E1000_REG_FFVT     0x09800  /* Flexible Filter Value Table - RW Array */

#define E1000_REG_KUMCTRLSTA 0x00034 /* MAC-PHY interface - RW */
#define E1000_REG_MDPHYA     0x0003C  /* PHY address - RW */
#define E1000_REG_MANC2H     0x05860  /* Managment Control To Host - RW */
#define E1000_REG_SW_FW_SYNC 0x05B5C /* Software-Firmware Synchronization - RW */

#define E1000_REG_GCR       0x05B00 /* PCI-Ex Control */
#define E1000_REG_GSCL_1    0x05B10 /* PCI-Ex Statistic Control #1 */
#define E1000_REG_GSCL_2    0x05B14 /* PCI-Ex Statistic Control #2 */
#define E1000_REG_GSCL_3    0x05B18 /* PCI-Ex Statistic Control #3 */
#define E1000_REG_GSCL_4    0x05B1C /* PCI-Ex Statistic Control #4 */
#define E1000_REG_FACTPS    0x05B30 /* Function Active and Power State to MNG */
#define E1000_REG_SWSM      0x05B50 /* SW Semaphore */
#define E1000_REG_FWSM      0x05B54 /* FW Semaphore */
#define E1000_REG_FFLT_DBG  0x05F04 /* Debug Register */
#define E1000_REG_HICR      0x08F00 /* Host Inteface Control */

/* RSS registers */
#define E1000_REG_CPUVEC    0x02C10 /* CPU Vector Register - RW */
#define E1000_REG_MRQC      0x05818 /* Multiple Receive Control - RW */
#define E1000_REG_RETA      0x05C00 /* Redirection Table - RW Array */
#define E1000_REG_RSSRK     0x05C80 /* RSS Random Key - RW Array */
#define E1000_REG_RSSIM     0x05864 /* RSS Interrupt Mask */
#define E1000_REG_RSSIR     0x05868 /* RSS Interrupt Request */


struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __attribute__((packed));

#define E1000_NTXDESC 32
#define E1000_NRXDESC 8
#define E1000_MAXPACK 1518

int pci_e1000_attach(struct pci_func *pcif);
int e1000_transmit(const char *buf, size_t size);

#endif  // SOL >= 6
