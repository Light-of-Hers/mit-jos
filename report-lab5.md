# JOS Lab5 Report

陈仁泽 1700012774

[TOC]

## Exercise 1

在`env_create`函数中加入如下语句即可：

```C
    if (type == ENV_TYPE_FS) 
        e->env_tf.tf_eflags |= FL_IOPL_MASK;
```

> Q: Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

> A: 不需要，因为IO权限保存在EFLAGS中，而上下文切换时会保存/恢复EFLAGS



## Exercise 2

### `bc_pgfault`

按要求完成即可。关于为什么先读取磁盘块再判断是否空闲，个人认为是因为读取的磁盘块可能是本身就是bitmap的一部分。

```C
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
```



### `flush_block`

按要求完成即可：

```C
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
```



## Exercise 3

按顺序搜索空闲块即可：

```C
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.

	// LAB 5: Your code here.
	// panic("alloc_block not implemented");
    int blockno;
    
    if (!super)
        panic("no super block");
    
    for (blockno = 0; blockno < super->s_nblocks; ++blockno)
        if (block_is_free(blockno))
            break;
    if (blockno == super->s_nblocks)
        return -E_NO_DISK;
    
    bitmap[blockno / 32] &= ~(1 << (blockno % 32));
    flush_block(bitmap);
    return blockno;
}
```



## Exercise 4

### `file_block_walk`

先寻找直接块，再寻找间接块，必要时分配一个间接块：

```C
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
    // LAB 5: Your code here.
    // panic("file_block_walk not implemented");
    if (filebno < NDIRECT) {
        *ppdiskbno = &f->f_direct[filebno];
        return 0;
    }
    filebno -= NDIRECT;

    if (filebno < NINDIRECT) {
        if (f->f_indirect == 0) {
            if (!alloc)
                return -E_NOT_FOUND;
            int blockno = alloc_block();
            if (blockno < 0)
                return blockno;
            f->f_indirect = (uint32_t)blockno;
            memset(diskaddr(f->f_indirect), 0, BLKSIZE);
        }
        uint32_t *ind_blk = (uint32_t*)diskaddr(f->f_indirect);
        *ppdiskbno = &ind_blk[filebno];
        return 0;
    }

    return -E_INVAL;
}
```



### `file_get_block`

利用`file_block_walk`寻找对应的slot，得到对应的块号（必要时分配一个块），之后返回磁盘块对应的内存地址即可：

```C
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
    // LAB 5: Your code here.
    // panic("file_get_block not implemented");
    int r;
    uint32_t *diskbno;
    
    if (r = file_block_walk(f, filebno, &diskbno, 1), r < 0)
        return r;
    if (*diskbno == 0) {
        if (r = alloc_block(), r < 0)
            return r;
        *diskbno = (uint32_t)r;
    }
    *blk = (char*)diskaddr(*diskbno);
    return 0;
}
```



## Exercise 5

模仿`serve_set_size`，利用`file_read`实现即可。注意最后要更新文件的offset。

```C
int
serve_read(envid_t envid, union Fsipc *ipc)
{
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;

	if (debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// Lab 5: Your code here:
    int r;
    struct OpenFile *o;

    if (r = openfile_lookup(envid, req->req_fileid, &o), r < 0)
        return r;
    if (r = file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset), r < 0)
        return r;
    o->o_fd->fd_offset += r;
	return r;
}
```



## Exercise 6

### `serve_write`

和`serve_read`实现类似：

```C
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
	if (debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// LAB 5: Your code here.
	// panic("serve_write not implemented");
    int r;
    struct OpenFile *o;

    if (r = openfile_lookup(envid, req->req_fileid, &o), r < 0)
        return r;
    if (r = file_write(o->o_file, req->req_buf, req->req_n, o->o_fd->fd_offset), r < 0)
        return r;
    o->o_fd->fd_offset += r;
    return r;
}
```



### `devfile_write`

模仿`devfile_read`实现即可：

```C
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
	// Make an FSREQ_WRITE request to the file system server.  Be
	// careful: fsipcbuf.write.req_buf is only so large, but
	// remember that write is always allowed to write *fewer*
	// bytes than requested.
	// LAB 5: Your code here
	// panic("devfile_write not implemented");
    int r;

    assert(n <= sizeof(fsipcbuf.write.req_buf));
    
    fsipcbuf.write.req_fileid = fd->fd_file.id;
    fsipcbuf.write.req_n = n;
    memmove(fsipcbuf.write.req_buf, buf, n);

    if (r = fsipc(FSREQ_WRITE, NULL), r < 0)
        return r;
    
    assert(r <= n);

    return r;
}
```



## Exercise 7

按要求设置一些必要的控制/状态寄存器即可：

```C
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	// panic("sys_env_set_trapframe not implemented");
    int r;
    struct Env *e;

    if (r = envid2env(envid, &e, 1), r < 0)
        return r;

    e->env_tf = *tf;
    tf = &e->env_tf;

    tf->tf_eflags |= FL_IF;
    tf->tf_eflags &= ~FL_IOPL_MASK;
    tf->tf_cs |= GD_UT | 3;
    tf->tf_ds |= GD_UD | 3;
    tf->tf_es |= GD_UD | 3;
    tf->tf_ss |= GD_UD | 3;

    return 0;
}
```



## Exercise 8

### `duppage`

按要求修改映射即可。注意我将COW的优先级视作比SHARE更高，也就是如果一个页是COW的，则将其视作COW处理而不管其是否为SHARE。

```C
static int
duppage(envid_t envid, unsigned pn)
{
	// LAB 4: Your code here.
	// panic("duppage not implemented");
    int r;
    void *pg;
    pte_t pte;

    pg = (void*)(pn * PGSIZE);
    pte = uvpt[pn];

    assert(pte & PTE_P && pte & PTE_U);
    if ((pte & PTE_W && !(pte & PTE_SHARE)) || pte & PTE_COW) {
        if (r = sys_page_map(0, pg, envid, pg, (pte & PTE_SYSCALL & ~PTE_W) | PTE_COW), r < 0)
            return r;
        if (r = sys_page_map(0, pg, 0, pg, (pte & PTE_SYSCALL & ~PTE_W) | PTE_COW), r < 0)
            return r;
    } else {
        if (r = sys_page_map(0, pg, envid, pg, pte & PTE_SYSCALL), r < 0)
            return r;
    }

	return 0;
}
```



### `copy_shared_pages`

只映射标记为SHARE的页面即可：

```C
static int
copy_shared_pages(envid_t child)
{
	// LAB 5: Your code here.
    int r;
    uintptr_t addr;

    for (addr = 0; addr < UTOP;) {
        if (!(uvpd[PDX(addr)] & PTE_P)) {
            addr = (uintptr_t)PGADDR(PDX(addr) + 1, PTX(addr), PGOFF(addr));
        } else {
            uintptr_t next = 
                MIN((uintptr_t)PGADDR(PDX(addr) + 1, PTX(addr), PGOFF(addr)), UTOP);
            for (; addr < next; addr += PGSIZE) {
                pte_t pte = uvpt[PGNUM(addr)];
                if (pte & PTE_P && pte & PTE_U && pte & PTE_SHARE)
                    if (r = sys_page_map(0, (void*)addr, 
                                         child, (void*)addr, pte & PTE_SYSCALL), r < 0)
                        return r;
            }
        }
    }
	return 0;
}

```



## Exercise 9

在`trap_dispatch`中添加两个处理即可：

```C
	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.
    if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
        kbd_intr();
        lapic_eoi();
        return;

    }

    if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
        serial_intr();
        lapic_eoi();
        return;
    }
```



## Exercise 10

模仿对输出重定向的处理即可：

```C
            if (fd = open(t, O_RDONLY), fd < 0) {
                cprintf("open %s for read: %e", t, fd);
                exit();
            }
            if (fd != 0) {
                dup(fd, 0);
                close(fd);
            }
```



## This completes this lab

```
internal FS tests [fs/test.c]: OK (1.4s) 
  fs i/o: OK 
  check_bc: OK 
  check_super: OK 
  check_bitmap: OK 
  alloc_block: OK 
  file_open: OK 
  file_get_block: OK 
  file_flush/file_truncate/file rewrite: OK 
testfile: OK (1.3s) 
  serve_open/file_stat/file_close: OK 
  file_read: OK 
  file_write: OK 
  file_read after file_write: OK 
  open: OK 
  large file: OK 
spawn via spawnhello: OK (0.7s) 
Protection I/O space: OK (1.6s) 
PTE_SHARE [testpteshare]: OK (2.3s) 
PTE_SHARE [testfdsharing]: OK (1.1s) 
start the shell [icode]: Timeout! OK (31.4s) 
testshell: OK (1.9s) 
primespipe: OK (6.8s) 
Score: 150/150
```



