#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/log.h>
// LAB 6: Your driver code here

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
#define REG_CTRL     0x00000  /* Device Control - RW */
#define REG_STATUS   0x00008  /* Device Status - RO */
#define REG_EECD     0x00010  /* EEPROM/Flash Control - RW */
#define REG_EERD     0x00014  /* EEPROM Read - RW */

#define REG_RAL      0x05400
#define REG_RAH      0x05404
#define REG_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define REG_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define REG_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define REG_RDH      0x02810  /* RX Descriptor Head - RW */
#define REG_RDT      0x02818  /* RX Descriptor Tail - RW */
#define REG_RCTL     0x00100  /* RX Control - RW */

#define REG_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define REG_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define REG_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define REG_TDH      0x03810  /* TX Descriptor Head - RW */
#define REG_TDT      0x03818  /* TX Descripotr Tail - RW */
#define REG_TCTL     0x00400  /* TX Control - RW */
#define REG_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define REG_TXCW     0x00178  /* TX Configuration Word - RW */
#define REG_RXCW     0x00180  /* RX Configuration Word - RO */

#define REG_EEARBC   0x01024  /* EEPROM Auto Read Bus Control */
#define REG_EEWR     0x0102C  /* EEPROM Write Register - RW */

#define REG_MTA      0x05200  /* Multicast Table Array - RW Array */
#define REG_IMS      0x000D0  /* Interrupt Mask Set - RW */


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
#define E1000_NRXDESC 128
#define E1000_MAXPACK 1518

volatile uint32_t *e1000;
uint8_t e1000_irq_line;

__attribute__((__aligned__(sizeof(struct e1000_tx_desc))))
static volatile struct e1000_tx_desc tx_descs[E1000_NTXDESC];
static volatile uint8_t tx_buf[E1000_NTXDESC][E1000_MAXPACK];

__attribute__((__aligned__(sizeof(struct e1000_rx_desc))))
static volatile struct e1000_rx_desc rx_descs[E1000_NRXDESC];
static volatile uint8_t rx_buf[E1000_NRXDESC][E1000_MAXPACK];


static void tx_init();
static void rx_init();

int
pci_e1000_attach(struct pci_func *pcif) 
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    e1000_irq_line = pcif->irq_line;
    tx_init();
    rx_init();
    return 1;
}

#define TCTL_EN             (1 << 1)
#define TCTL_PSP            (1 << 3)
#define TCTL_CT_SHIFT       4
#define TCTL_COLD_SHIFT     12

#define TIPG_IPGT_SHIFT     0
#define TIPG_IPGR1_SHIFT    10
#define TIPG_IPGR2_SHIFT    20

#define TX_CMD_EOP     (1 << 0)
#define TX_CMD_IFCS    (1 << 1)
#define TX_CMD_RS      (1 << 3)
#define TX_STA_DD      (1 << 0)

static volatile uint32_t *reg_tdh, *reg_tdt;

static void 
tx_init() 
{
    // tx descs init
    for (int i = 0; i < E1000_NTXDESC; ++i) {
        tx_descs[i].status |= TX_STA_DD;
    }

    // setting TDBAL, TDLEN
    e1000[REG_TDBAL >> 2] = (uint32_t)PADDR((void*)tx_descs);
    e1000[REG_TDLEN >> 2] = sizeof(tx_descs);

    // TDH = TDT = 0
    reg_tdh = &e1000[REG_TDH >> 2];
    reg_tdt = &e1000[REG_TDT >> 2];
    *reg_tdh = 0;
    *reg_tdt = 0;

    // TCTL setting
    e1000[REG_TCTL >> 2] = 0
        | TCTL_EN 
        | TCTL_PSP 
        | (0x10 << TCTL_CT_SHIFT) 
        | (0x40 << TCTL_COLD_SHIFT)
        ;

    // IPG setting
    e1000[REG_TIPG >> 2] = 0
        | (10 << TIPG_IPGT_SHIFT) 
        | (8 << TIPG_IPGR1_SHIFT) 
        | (6 << TIPG_IPGR2_SHIFT)
        ;

    // test
    // e1000_transmit("hello JOS", sizeof("hello JOS"));
}

int 
e1000_transmit(const char *buf, size_t len) 
{
    assert(len <= E1000_MAXPACK);
    
    uint32_t tmp_reg_tdt = *reg_tdt;

    // queue if full
    if (!(tx_descs[tmp_reg_tdt].status & TX_STA_DD))
        return -1;

    // copy memory
    memmove((void*)tx_buf[tmp_reg_tdt], buf, len);

    // setting desc
    tx_descs[tmp_reg_tdt].cmd = TX_CMD_RS | TX_CMD_EOP | TX_CMD_IFCS; // EOP !!!!
    tx_descs[tmp_reg_tdt].status = 0;
    tx_descs[tmp_reg_tdt].addr = (uint64_t)PADDR((void*)tx_buf[tmp_reg_tdt]);
    tx_descs[tmp_reg_tdt].length = len;

    // update TDT
    *reg_tdt = (tmp_reg_tdt + 1) % E1000_NTXDESC;

    while(!(tx_descs[tmp_reg_tdt].status & TX_STA_DD))
        /* spinning */;

    return 0;
}

#define RAH_AS              (1 << 31)

#define RCTL_EN             (1 << 1)
#define RCTL_LBM_SHIFT      6
#define RCTL_RDMTS_SHIFT    8
#define RCTL_BAM            (1 << 15)
#define RCTL_BSIZE_SHIFT    16
#define RCTL_SECRC          (1 << 26)

#define IMS_LSC             (1 << 2)
#define IMS_RXSEQ           (1 << 3)
#define IMS_RXDMT           (1 << 4)
#define IMS_RXO             (1 << 6)
#define IMS_RXT             (1 << 7)

#define RX_STA_DD      (1 << 0)
#define RX_STA_EOP     (1 << 1)

static volatile uint32_t *reg_rdh, *reg_rdt;

static void 
rx_init() 
{
    // init rx descs
    for (int i = 0; i < E1000_NRXDESC; ++i) {
        rx_descs[i].addr = (uint32_t)PADDR((void*)rx_buf[i]);
        rx_descs[i].length = E1000_MAXPACK;
        rx_descs[i].status = 0;
    }

    // setting RAH:RAL
    e1000[REG_RAL >> 2] = QEMU_MAC_ADDR & 0xFFFFFFFF;
    e1000[REG_RAH >> 2] = QEMU_MAC_ADDR >> 32 | RAH_AS;

    // setting MTA: Multicast Table Array
    memset((void*)(e1000 + REG_MTA), 0, 128 * sizeof(uint32_t));

    // // setting IMS: Interrupt Mask Set
    // e1000[REG_IMS >> 2] = 
    //     0
    //     | IMS_LSC
    //     | IMS_RXSEQ
    //     | IMS_RXDMT
    //     | IMS_RXO 
    //     | IMS_RXT 
    //     ;

    // setting RDBAL, RDLEN
    e1000[REG_RDBAL >> 2] = (uint32_t)PADDR((void*)rx_descs);
    e1000[REG_RDLEN >> 2] = sizeof(rx_descs);

    // setting RDH, RHT
    reg_rdh = &e1000[REG_RDH >> 2];
    reg_rdt = &e1000[REG_RDT >> 2];
    *reg_rdh = 0;
    *reg_rdt = E1000_NRXDESC - 1;

    // setting RCTL
    e1000[REG_RCTL >> 2] =
        0
        | RCTL_EN
        | (0 << RCTL_LBM_SHIFT)
        | (3 << RCTL_RDMTS_SHIFT)
        | RCTL_BAM
        | (0 << RCTL_BSIZE_SHIFT)
        | RCTL_SECRC
        ;
}

int
e1000_receive(char *buf, size_t len)
{
    uint32_t tmp_reg_rdt = (*reg_rdt + 1) % E1000_NRXDESC;

    // queue is empty
    if (!(rx_descs[tmp_reg_rdt].status & RX_STA_DD))
        return -1;

    // one buffer, one packet
    assert(rx_descs[tmp_reg_rdt].status & RX_STA_EOP);
    assert(KADDR(rx_descs[tmp_reg_rdt].addr) == rx_buf[tmp_reg_rdt]);

    // memory copy
    len = MIN(len, rx_descs[tmp_reg_rdt].length);
    memmove(buf, KADDR(rx_descs[tmp_reg_rdt].addr), len);

    // set desc
    rx_descs[tmp_reg_rdt].status = 0;
    rx_descs[tmp_reg_rdt].addr = (uint32_t)PADDR((void*)rx_buf[tmp_reg_rdt]);
    rx_descs[tmp_reg_rdt].length = E1000_MAXPACK;

    // update register
    *reg_rdt = tmp_reg_rdt;

    return (int)len;
}