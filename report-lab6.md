# JOS Lab6 Report

陈仁泽 1700012774

[TOC]

## Exercise 1

在时钟中断处调用`time_tick`：

```C
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
        time_tick();
        lapic_eoi();
#ifndef CONF_MFQ
		sched_yield();
		return;
#else
		struct Env* cure = curenv;
		if (cure && cure->env_mfq_left_ticks-- == 1) {
			sched_yield();
		}
        return;
#endif
    }
```

系统调用处直接调用`time_msec`即可：

```C
// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	// panic("sys_time_msec not implemented");
    return time_msec();
}
```



## Exercise 3

在`e1000.h`和`e1000.c`中实现一个pci-attach的函数：

```C
int
pci_e1000_attach(struct pci_func *pcif) 
{
    pci_func_enable(pcif);
    return 1;
}
```

查询手册得到E1000的VENDOR-ID和DEV-ID：

```C
#define E1000_VEND     0x8086  // Vendor ID for Intel 
#define E1000_DEV      0x100E  // Device ID for the e1000 Qemu, Bochs, and VirtualBox emmulated NICs
```

在`pci_attach_vendor`中添加：

```C
struct pci_driver pci_attach_vendor[] = {
    { E1000_VEND, E1000_DEV, &pci_e1000_attach },
	{ 0, 0, 0 },
};
```



## Exercise 4

在`e1000.c`中定义一个MMIO指针：

```C
static volatile uint32_t *e1000;
```

在`pci_e1000_attach`中初始化MMIO指针：

```C
int
pci_e1000_attach(struct pci_func *pcif) 
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    return 1;
}
```



## Exercise 5

根据手册定义传输描述符的结构：

```C
struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};
```

设置一些基本常量，定义描述符表和传输缓冲区：

```C
#define E1000_NTXDESC 32
#define E1000_MAXPACK 1518

__attribute__((__aligned__(sizeof(struct e1000_tx_desc))))
static volatile struct e1000_tx_desc tx_descs[E1000_NTXDESC];
static volatile uint8_t tx_buf[E1000_NTXDESC][E1000_MAXPACK];
```

查手册得到一些寄存器的位置，以及一些寄存器（TCTL/TIPG）内部必要字段的位置或偏移，定义并实现`tx_init`函数：

+ 初始化传输描述符表（主要是设置STA.DD位，相当于标记该缓冲区为空）。
+ 按手册要求初始化TDBAL（因为是32位系统，不用初始化TDBAH）,TDLEN,TDH,TDT寄存器。
+ 结合手册和实验要求初始化TCTL,TIPG寄存器。

```C
#define REG_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define REG_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define REG_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define REG_TDH      0x03810  /* TX Descriptor Head - RW */
#define REG_TDT      0x03818  /* TX Descripotr Tail - RW */
#define REG_TCTL     0x00400  /* TX Control - RW */
#define REG_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define TCTL_EN_BIT         (1 << 1)
#define TCTL_PSP_BIT        (1 << 3)
#define TCTL_CT_SHIFT       4
#define TCTL_COLD_SHIFT     12

#define TIPG_IPGT_SHIFT     0
#define TIPG_IPGR1_SHIFT    10
#define TIPG_IPGR2_SHIFT    20

#define TX_CMD_EOP_BIT     (1 << 0)
#define TX_CMD_IFCS_BIT    (1 << 1)
#define TX_CMD_RS_BIT      (1 << 3)
#define TX_STA_DD_BIT      (1 << 0)

static volatile uint32_t *reg_tdh, *reg_tdt;

static void 
tx_init() 
{
    // tx descs init
    for (int i = 0; i < E1000_NTXDESC; ++i) {
        tx_descs[i].status |= TX_STA_DD_BIT;
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
        | TCTL_EN_BIT 
        | TCTL_PSP_BIT 
        | (0x10 << TCTL_CT_SHIFT) 
        | (0x40 << TCTL_COLD_SHIFT)
        ;

    // IPG setting
    e1000[REG_TIPG >> 2] = 0
        | (10 << TIPG_IPGT_SHIFT) 
        | (8 << TIPG_IPGR1_SHIFT) 
        | (6 << TIPG_IPGR2_SHIFT)
        ;
}
```

在`pci_e1000_attach`中添加`tx_init`：

```C
int
pci_e1000_attach(struct pci_func *pcif) 
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    tx_init();
    return 1;
}
```



## Exercise 6

添加并实现`e1000_transmit`函数。按手册要求实现即可。有几点需要注意：

+ 判断一个缓冲区是否空闲不能依靠TDH寄存器，而需要利用描述符中的STA.DD位来判断。发包时也要设置CMD.RS位。
+ 传输的地址要用物理地址。
+ 因为我设置单个缓冲区大小足够装下一个以太网包，所以每次发包都要设置CMD.EOP位标记这是包的最后一段。
+ CMD.IFCS位用于插入FCS校验码，可以增强链路层的检错能力，本次实验中不设置也没什么关系（反正接收时并不会处理……）。

```C
int 
e1000_transmit(const char *buf, size_t len) 
{
    assert(len <= E1000_MAXPACK);
    
    uint32_t tmp_reg_tdt = *reg_tdt;

    // queue if full
    if (!(tx_descs[tmp_reg_tdt].status & TX_STA_DD_BIT))
        return -1;

    // copy memory
    memmove((void*)tx_buf[tmp_reg_tdt], buf, len);

    // setting desc
    tx_descs[tmp_reg_tdt].cmd = 0 
        | TX_CMD_RS_BIT 
        | TX_CMD_EOP_BIT 
        | TX_CMD_IFCS_BIT
        ;

    tx_descs[tmp_reg_tdt].status = 0;
    tx_descs[tmp_reg_tdt].addr = (uint64_t)PADDR((void*)tx_buf[tmp_reg_tdt]);
    tx_descs[tmp_reg_tdt].length = len;

    // update TDT
    *reg_tdt = (tmp_reg_tdt + 1) % E1000_NTXDESC;

    return 0;
}
```



## Exercise 7

设置一个系统调用`sys_dl_transmit`（数据链路层传输）简单包装`e1000_transmit`：

```C
static int 
sys_dl_transmit(const char* buf, size_t len) {
    user_mem_assert(curenv, buf, len, PTE_U);
    return e1000_transmit(buf, len);    
}
```



## Exercise 8

按要求实现即可，注意只接受来自ns_envid的传输请求：

```C
void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
    int r;
    int feid;

    while(1) {
        if (r = ipc_recv(&feid, &nsipcbuf, NULL), r < 0)
            panic("in output, ipc_recv: %e", r);
        if (r != NSREQ_OUTPUT || feid != ns_envid)
            continue;
        while (r = sys_dl_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len), r < 0)
            /* spinning */;
    }
}
```



> Q: How did you structure your transmit implementation? In particular, what do you do if the transmit ring is full?

> A: 传输缓冲区满的话直接忙等（因为是外部设备，而且是发送消息，一般只会阻塞很短的时间，因此忙等就够了），直到传输成功。



## Exercise 10

根据手册定义接收描述符的结构：

```C
struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};
```

设置一些基本常量，定义描述符表和接收缓冲区：

```C
#define E1000_NRXDESC 128

#define QEMU_MAC_ADDR  0x563412005452

__attribute__((__aligned__(sizeof(struct e1000_rx_desc))))
static volatile struct e1000_rx_desc rx_descs[E1000_NRXDESC];
static volatile uint8_t rx_buf[E1000_NRXDESC][E1000_MAXPACK];
```

查手册得到一些寄存器的位置，以及一些寄存器内部必要字段的位置或偏移，并实现`rx_init`函数（具体过程不再赘述）：

```C
#define REG_RAL      0x05400
#define REG_RAH      0x05404
#define REG_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define REG_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define REG_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define REG_RDH      0x02810  /* RX Descriptor Head - RW */
#define REG_RDT      0x02818  /* RX Descriptor Tail - RW */
#define REG_RCTL     0x00100  /* RX Control - RW */

#define REG_MTA      0x05200  /* Multicast Table Array - RW Array */


#define RAH_AV_BIT              (1 << 31)

#define RCTL_EN_BIT             (1 << 1)
#define RCTL_LBM_SHIFT          6
#define RCTL_RDMTS_SHIFT        8
#define RCTL_BAM_BIT            (1 << 15)
#define RCTL_BSIZE_SHIFT        16
#define RCTL_SECRC_BIT          (1 << 26)

#define RX_STA_DD_BIT      (1 << 0)
#define RX_STA_EOP_BIT     (1 << 1)

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
    e1000[REG_RAH >> 2] = QEMU_MAC_ADDR >> 32 | RAH_AV_BIT;

    // setting MTA: Multicast Table Array
    memset((void*)(e1000 + REG_MTA), 0, 128 * sizeof(uint32_t));

    // setting RDBAL, RDLEN
    e1000[REG_RDBAL >> 2] = (uint32_t)PADDR((void*)rx_descs);
    e1000[REG_RDLEN >> 2] = sizeof(rx_descs);

    // setting RDH, RHT
    reg_rdh = &e1000[REG_RDH >> 2];
    reg_rdt = &e1000[REG_RDT >> 2];
    *reg_rdh = 0;
    *reg_rdt = E1000_NRXDESC - 1;

    // setting RCTL
    e1000[REG_RCTL >> 2] =　0
        | RCTL_EN_BIT
        | (0 << RCTL_LBM_SHIFT)
        | (3 << RCTL_RDMTS_SHIFT)
        | RCTL_BAM_BIT
        | (0 << RCTL_BSIZE_SHIFT)
        | RCTL_SECRC_BIT
        ;
}
```

将`rx_init`加入`pci_e1000_attach`中：

```C
int
pci_e1000_attach(struct pci_func *pcif) 
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
    tx_init();
    rx_init();
    return 1;
}
```



## Exercise 11

添加并实现`e1000_receive`函数。注意该实现的缓冲区负载上限为缓冲区大小减一（因为RDT好像不能指向缓冲区外部，但网卡判断缓冲区满的条件又是RDH == RDT。不太懂该怎么完全利用缓冲区……）：

```C
int
e1000_receive(char *buf, size_t len)
{
    uint32_t tmp_reg_rdt = (*reg_rdt + 1) % E1000_NRXDESC;

    // queue is empty
    if (!(rx_descs[tmp_reg_rdt].status & RX_STA_DD_BIT))
        return -1;

    // one buffer, one packet
    assert(rx_descs[tmp_reg_rdt].status & RX_STA_EOP_BIT);
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
```

实现一个系统调用`sys_dl_receive`，简单包装一下`e1000_receive`：

```C
static int 
sys_dl_receive(char *buf, size_t len) 
{
    user_mem_assert(curenv, buf, len, PTE_U | PTE_W);
    return e1000_receive(buf, len);
}
```



## Exercise 12

按要求实现即可。注意每次接收包时都重新分配一个物理页，防止服务请求方来不及读取数据就被覆写了。

```C
void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int r;

	while(1) {
		if (r = sys_page_alloc(0, &nsipcbuf, PTE_P | PTE_U | PTE_W), r < 0)
			panic("in input, sys_page_alloc: %e", r);
		while (r = sys_dl_receive(nsipcbuf.pkt.jp_data, PGSIZE - sizeof(int)), r < 0)
			sys_yield();
		nsipcbuf.pkt.jp_len = r;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_U);
		if (r = sys_page_unmap(0, &nsipcbuf), r < 0)
			panic("in input, sys_page_unmap: %e", r);
	}
}
```



> Q: How did you structure your receive implementation? In particular, what do you do if the receive queue is empty and a user environment requests the next incoming packet?

> A: 缓冲区空的时候只是简单地出让CPU，期待下次调度时缓冲区中有数据（这时最好使用轮转调度）。通常情况下这样做足够了，对于一些实时性要求比较高的网络应用来说可能性能上不太好。更好的做法是采用中断来提醒收包。



## Exercise 13

### `send_file`

实现比较简单。注意返回前要及时关闭fd。

```C
static int
send_file(struct http_request *req)
{
	int r;
	off_t file_size = -1;
	int fd;

	// open the requested url for reading
	// if the file does not exist, send a 404 error using send_error
	// if the file is a directory, send a 404 error using send_error
	// set file_size to the size of the file

	// LAB 6: Your code here.
	// panic("send_file not implemented");
	if (fd = open(req->url, O_RDONLY), fd < 0) {
		send_error(req, 404);
		return fd;
	}

	struct Stat st;
	if (r = stat(req->url, &st), r < 0 || st.st_isdir) {
		r = send_error(req, 404);
		goto end;
	}

	file_size = st.st_size;

	if ((r = send_header(req, 200)) < 0)
		goto end;

	if ((r = send_size(req, file_size)) < 0)
		goto end;

	if ((r = send_content_type(req)) < 0)
		goto end;

	if ((r = send_header_fin(req)) < 0)
		goto end;

	r = send_data(req, fd);

end:
	close(fd);
	return r;
}
```

### `send_data`

实现比较简单。注意向网络写数据时可能需要连续写多次才能将缓冲区数据写完。

```C
static int
send_data(struct http_request *req, int fd)
{
	// LAB 6: Your code here.
	// panic("send_data not implemented");
	int r;
	int rd, wt, tot;
	char buf[256];

	tot = 0;
	while (rd = read(fd, buf, sizeof(buf)), rd > 0) {
		for (wt = 0; wt < rd; ) {
			if (r = write(req->sock, buf + wt, rd - wt), r < 0)
				return r;
			wt += r;
		}
		tot += rd;
	}
	return rd < 0 ? rd : tot;
}
```



> Q: What does the web page served by JOS's web server say?

> A: This file came from JOS. Cheesy web page!



> Q: How long approximately did it take you to do this lab?

> A: 



## This complete this lab

```
testtime: OK (7.9s) 
pci attach: OK (0.8s) 
testoutput [5 packets]: OK (2.0s) 
testoutput [100 packets]: OK (2.2s) 
Part A score: 35/35

testinput [5 packets]: OK (1.9s) 
testinput [100 packets]: OK (1.2s) 
tcp echo server [echosrv]: OK (1.6s) 
web server [httpd]: 
  http://localhost:26002/: OK (2.2s) 
  http://localhost:26002/index.html: OK (0.9s) 
  http://localhost:26002/random_file.txt: OK (1.7s) 
Part B score: 70/70

Score: 105/105
```



## Challenge: Get MAC Address from EEPROM

### 原理

查阅手册可知，可以通过EERD寄存器来访问EEPROM：

> Software can use the EEPROM Read register (EERD) to cause the Ethernet controller to read a word from the EEPROM that the software can then use. To do this, software writes the address to read the Read Address (EERD.ADDR) field and then simultaneously writes a 1b to the Start Read bit (EERD.START). The Ethernet controller then reads the word from the EEPROM, sets the Read Done bit (EERD.DONE), and puts the data in the Read Data field (EERD.DATA). Software can poll the EEPROM Read register until it sees the EERD.DONE bit set, then use the data from the EERD.DATA field. Any words read this way are not written to hardware’s internal registers.

而MAC地址可以通过访问EEPROM的前三个16位字来获得。



### 实现

查完手册，实现起来就很简单了：

```C
#define REG_EERD     0x00014

#define EERD_START_BIT      (1 << 0)
#define EERD_DONE_BIT       (1 << 4)
#define EERD_ADDR_SHIFT     8
#define EERD_DATA_SHIFT     16

uint64_t
e1000_read_mac_addr(void)
{
    uint32_t eerd = 0;
    uint16_t data = 0;
    uint64_t mac_addr = 0;

    for (int i = 0; i < 3; ++i) {
        e1000[REG_EERD >> 2] = 0
            | (i << EERD_ADDR_SHIFT)
            | EERD_START_BIT
            ;
        while(eerd = e1000[REG_EERD >> 2], !(eerd & EERD_DONE_BIT))
            /* waiting */;
        data = eerd >> EERD_DATA_SHIFT;
        mac_addr |= (uint64_t)data << (i * 16);
    }
    return mac_addr;
}
```

初始化RAH:RAL时可以直接用上：

```C
    // setting RAH:RAL
    uint64_t mac_addr = e1000_read_mac_addr();
    e1000[REG_RAL >> 2] = mac_addr & 0xFFFFFFFF;
    e1000[REG_RAH >> 2] = (mac_addr >> 32) | RAH_AV_BIT;
```

添加系统调用`sys_dl_read_mac_addr`简单包装：

```C
static int 
sys_dl_read_mac_addr(uint8_t *mac)
{
    user_mem_assert(curenv, mac, 6, PTE_U | PTE_W);
    uint64_t mac_addr = e1000_read_mac_addr();
    for (int i = 0; i < 6; ++i)
        mac[i] = (uint8_t)(mac_addr >> (8 * i));
    return 0;
}
```

修改`/net/lwip/jos/jif/jif.c`中的`low_level_init`函数：

```C
static void
low_level_init(struct netif *netif)
{
    int r;

    netif->hwaddr_len = 6;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST;

    sys_dl_read_mac_addr(netif->hwaddr);
    // MAC address is hardcoded to eliminate a system call
    // netif->hwaddr[0] = 0x52;
    // netif->hwaddr[1] = 0x54;
    // netif->hwaddr[2] = 0x00;
    // netif->hwaddr[3] = 0x12;
    // netif->hwaddr[4] = 0x34;
    // netif->hwaddr[5] = 0x56;
}
```



