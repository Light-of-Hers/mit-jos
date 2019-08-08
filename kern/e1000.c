#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/log.h>
// LAB 6: Your driver code here

volatile uint32_t *e1000;

__attribute__((__aligned__(sizeof(struct e1000_tx_desc))))
static volatile struct e1000_tx_desc tx_descs[E1000_NTXDESC];

static volatile uint8_t tx_buf[E1000_NTXDESC][E1000_MAXPACK];

static volatile uint32_t *reg_tdbal;
static volatile uint32_t *reg_tdlen;
static volatile uint32_t *reg_tdh;
static volatile uint32_t *reg_tdt;

#define TCTL_EN         (1 << 1)
#define TCTL_PSP        (1 << 3)
#define TCTL_CT_SHIFT   4
#define TCTL_COLD_SHIFT 12

#define TIPG_IPGT_SHIFT     0
#define TIPG_IPGR1_SHIFT    10
#define TIPG_IPGR2_SHIFT    20

#define CMD_EOP (1 << 0)
#define CMD_IFCS (1 << 1)
#define CMD_RS (1 << 3)

#define STA_DD (1 << 0)

static volatile struct e1000_reg_tctl_t {
    uint32_t reserved1      : 1;
    uint32_t en             : 1;
    uint32_t reserved2      : 1;
    uint32_t psp            : 1;
    uint32_t ct             : 8;
    uint32_t cold           : 10;
    uint32_t swxoff         : 1;
    uint32_t reserved3      : 1;
    uint32_t rtlc           : 1;
    uint32_t nrtu_reserved  : 1;
    uint32_t reserved4      : 6;
} __attribute__((packed)) *reg_tctl;

static volatile struct e1000_reg_tipg_t {
    uint32_t ipgt       : 10;
    uint32_t ipgr1      : 10;
    uint32_t ipgr2      : 10;
    uint32_t reserved   : 2;  
} __attribute__((packed)) *reg_tipg;

static void tx_init();

int
pci_e1000_attach(struct pci_func *pcif) 
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    tx_init();
    return 1;
}

int 
e1000_transmit(const char *buf, size_t len) 
{
    assert(len <= E1000_MAXPACK);
    
    uint32_t tmp_reg_tdt = *reg_tdt;

    if (!(tx_descs[tmp_reg_tdt].status & STA_DD))
        return -1;

    // copy memory
    memmove((void*)tx_buf[tmp_reg_tdt], buf, len);

    // setting desc
    tx_descs[tmp_reg_tdt].cmd = CMD_RS | CMD_EOP | CMD_IFCS; // EOP !!!!
    tx_descs[tmp_reg_tdt].status = 0;
    tx_descs[tmp_reg_tdt].addr = (uint64_t)PADDR((void*)tx_buf[tmp_reg_tdt]);
    tx_descs[tmp_reg_tdt].length = len;

    // update TDT
    *reg_tdt = (tmp_reg_tdt + 1) % E1000_NTXDESC;

    return 0;
}

static void 
tx_init() 
{
    // tx descs init
    for (int i = 0; i < E1000_NTXDESC; ++i) {
        tx_descs[i].status |= STA_DD;
    }

    // binding
    reg_tdbal = &e1000[E1000_REG_TDBAL >> 2];
    reg_tdlen = &e1000[E1000_REG_TDLEN >> 2];
    reg_tdh = &e1000[E1000_REG_TDH >> 2];
    reg_tdt = &e1000[E1000_REG_TDT >> 2];
    reg_tctl = (struct e1000_reg_tctl_t *)&e1000[E1000_REG_TCTL >> 2];
    reg_tipg = (struct e1000_reg_tipg_t *)&e1000[E1000_REG_TIPG >> 2];

    // TDBAL = tx descs base address
    *reg_tdbal = (uint32_t)PADDR((void*)tx_descs);
    // TDLEN = tx descs len in byte
    *reg_tdlen = sizeof(tx_descs);
    // TDH = TDT = 0
    *reg_tdh = 0;
    *reg_tdt = 0;
    // TCTL setting
    struct e1000_reg_tctl_t tmp_reg_tctl = *reg_tctl;
    tmp_reg_tctl.en = 1;
    tmp_reg_tctl.psp = 1;
    tmp_reg_tctl.ct = 0x10;
    tmp_reg_tctl.cold = 0x40;
    *reg_tctl = tmp_reg_tctl;
    // IPG setting
    struct e1000_reg_tipg_t tmp_reg_tipg = *reg_tipg;
    tmp_reg_tipg.ipgt = 10;
    tmp_reg_tipg.ipgr1 = 8;
    tmp_reg_tipg.ipgr2 = 6;
    tmp_reg_tipg.reserved = 0;
    *reg_tipg = tmp_reg_tipg;

    // test
    // e1000_transmit("hello JOS", sizeof("hello JOS"));
}