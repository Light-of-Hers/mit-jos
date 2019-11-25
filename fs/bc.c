
#include "fs.h"
#include <inc/elink.h>
#include <inc/config.h>
#include <inc/log.h>

#ifdef CONF_BUF_CACHE
#define NCACHE 1024
#define NALLOC (NCACHE / 8)
typedef struct BufferCache {
	struct BufferCache *bufc_free_link;
	EmbedLink bufc_used_link;
	void *bufc_addr;
} BufferCache;

static BufferCache bcaches[NCACHE];
static BufferCache *bufc_free;
static EmbedLink bufc_used;
static int ncache = 0;
static int nalloc = 0;

static void bufc_init(void);
static int bufc_alloc(void *addr);
static int bufc_scan(void);
static int bufc_evict(void);
static int bufc_remove(BufferCache *bc);
static bool is_reserved(void *addr);

static bool 
is_reserved(void *addr)
{
	return addr < diskaddr(2);
}

static int
bufc_alloc(void *addr)
{
	int r;

	if (++nalloc == NALLOC) {
		nalloc = 0;
		if (r = bufc_scan(), r < 0)
			return r;
	}
	if (is_reserved(addr))
		return 0;
	if (ncache == NCACHE) {
		if (r = bufc_evict(), r < 0)
			return r;
	}
	assert(bufc_free);
	ncache++;
	BufferCache *bc = bufc_free;
	bufc_free = bufc_free->bufc_free_link;
	elink_enqueue(&bufc_used, &bc->bufc_used_link);
	bc->bufc_addr = addr;

	return 0;
}
static void
bufc_init(void)
{
	bufc_free = NULL;
	for (int i = 0; i < NCACHE; ++i) {
		elink_init(&bcaches[i].bufc_used_link);
		bcaches[i].bufc_free_link = bufc_free, bufc_free = bcaches + i;
		bcaches[i].bufc_addr = 0;
	}
	elink_init(&bufc_used);
}
static int
bufc_scan(void) 
{
	int r;

	for (EmbedLink *ln = bufc_used.next; ln != &bufc_used; ln = ln->next) {
		BufferCache *bc = master(ln, BufferCache, bufc_used_link);
		void *addr = bc->bufc_addr;
		if (uvpt[PGNUM(addr)] & PTE_A) {
			flush_block(addr);
			if (r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL), r < 0) {
				checkpoint;
				return r;
			}
		}
	}

	return 0;
}
static int 
bufc_remove(BufferCache *bc) 
{
	int r;

	flush_block(bc->bufc_addr);
	if (r = sys_page_unmap(0, bc->bufc_addr), r < 0)
		return r;
	ncache--;
	elink_remove(&bc->bufc_used_link);
	bc->bufc_free_link = bufc_free;
	bufc_free = bc;
	bc->bufc_addr = 0;

	return 0;
}
static int 
bufc_evict(void) 
{
	int r;

	checkpoint;

	EmbedLink *ln;
	for (ln = bufc_used.next; ln != &bufc_used;) {
		BufferCache *bc = master(ln, BufferCache, bufc_used_link);
		void *addr = bc->bufc_addr;
		ln = ln->next;
		if (!(uvpt[PGNUM(addr)] & PTE_A)) {
			if (r = bufc_remove(bc), r < 0)
				return r;
		}
	}
	if (ncache == NCACHE) {
		ln = bufc_used.prev;
		assert(ln != &bufc_used);
		BufferCache *bc = master(ln, BufferCache, bufc_used_link);
		if (r = bufc_remove(bc), r < 0)
			return r;
	}
	return 0;
}
#endif

extern volatile pde_t uvpd[];
extern volatile pte_t uvpt[];

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (uvpt[PGNUM(va)] & PTE_D) != 0;
}

// Fault any disk block that is read in to memory by
// loading it from disk.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_eip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page.
	// Hint: first round addr to page boundary. fs/ide.c has code to read
	// the disk.
	//
	// LAB 5: you code here:

    addr = ROUNDDOWN(addr, BLKSIZE);
    if (r = sys_page_alloc(0, addr, PTE_P | PTE_U | PTE_W), r < 0)
        panic("in bc_pgfault, sys_page_alloc: %e", r);
    if (r = ide_read(blockno * BLKSECTS, addr, BLKSECTS), r < 0)
        panic("in bc_pgfault, ide_read: %e", r);

	// Clear the dirty bit for the disk block page since we just read the
	// block from disk
	if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
		panic("in bc_pgfault, sys_page_map: %e", r);

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
    // perhaps it's the bitmap block, therefore reading the block before checking the bitmap
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);

#ifdef CONF_BUF_CACHE
	if (r = bufc_alloc(addr), r < 0)
		panic("in bc_pgfault, bufc_alloc: %e", r);
#endif

}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	// panic("flush_block not implemented");
    int r;

    addr = ROUNDDOWN(addr, BLKSIZE);
    if (!va_is_mapped(addr) || !va_is_dirty(addr))
        return;
    
    if (r = ide_write(blockno * BLKSECTS, addr, BLKSECTS), r < 0)
        panic("in flush_block, ide_write: %e", r);

    if (r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL), r < 0)
        panic("in flush_block, sys_page_map: %e", r);
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	// Now repeat the same experiment, but pass an unaligned address to
	// flush_block.

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");

	// Pass an unaligned address to flush_block.
	flush_block(diskaddr(1) + 20);
	assert(va_is_mapped(diskaddr(1)));

	// Skip the !va_is_dirty() check because it makes the bug somewhat
	// obscure and hence harder to debug.
	//assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{

	struct Super super;
	set_pgfault_handler(bc_pgfault);
#ifdef CONF_BUF_CACHE
	bufc_init();
#endif
	check_bc();

	// cache the super block by reading it once
	memmove(&super, diskaddr(1), sizeof super);

}

