# JOS Lab1 Report

[TOC]

## Exercise 1
---
## Exercise 2

跳转到bootloader前的指令：
```
[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
(gdb) si
[f000:e05b]    0xfe05b:	cmpl   $0x0,%cs:0x70c8
0x0000e05b in ?? ()
(gdb) si
[f000:e062]    0xfe062:	jne    0xfd414
0x0000e062 in ?? ()
(gdb) si
[f000:e066]    0xfe066:	xor    %dx,%dx
0x0000e066 in ?? ()
(gdb) si
[f000:e068]    0xfe068:	mov    %dx,%ss
0x0000e068 in ?? ()
(gdb) si
[f000:e06a]    0xfe06a:	mov    $0x7000,%esp
0x0000e06a in ?? ()
(gdb) si
[f000:e070]    0xfe070:	mov    $0xf2d4e,%edx
0x0000e070 in ?? ()
(gdb) si
[f000:e076]    0xfe076:	jmp    0xfff00
0x0000e076 in ?? ()
```
在ROM上进行一些初始化工作，如：检查RAM，初始化硬件，初始化段寄存器ss和栈指针esp等。之后跳转到bootloader，即RAM地址0x7c00处（和GDB显示的不一致）

---
## Exercise 3

> Q1: At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?

在执行了`ljmp    $PROT_MODE_CSEG, $protcseg`后，处理器跳转到32位代码。

在以下代码执行，使能了A20总线后，处理器从16位模式进入32位模式。
```
seta20.1:
    inb     $0x64,%al               # Wait for not busy
    testb   $0x2,%al
    jnz     seta20.1

    movb    $0xd1,%al               # 0xd1 -> port 0x64
    outb    %al,$0x64

seta20.2:
    inb     $0x64,%al               # Wait for not busy
    testb   $0x2,%al
    jnz     seta20.2

    movb    $0xdf,%al               # 0xdf -> port 0x60
    outb    %al,$0x60
```
<br/>

> Q2: What is the last instruction of the boot loader executed, and what is the first instruction of the kernel it just loaded?

bootloader最后执行的语句及其对应的指令为:
```
((void (*)(void)) (ELFHDR->e_entry))();
  7d6b:	ff 15 18 00 01 00    	call   *0x10018
```

kernel被加载进来时执行的第一个指令为:
```
movw	$0x1234,0x472			# warm boot
```
<br/>

> Q3: Where is the first instruction of the kernel?

内核执行的第一条指令位于`0x10000c`

<br/>

> Q4: How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?

1. 根据elf文件结构体的`e_phoff`字段确定第一个程序段头（program segment header）的偏移
2. 根据`e_phnum`字段确定程序段头的数量
3. 依次读入各个程序段头：根据其结构体的`p_memsz`获取对应程序段（program segment）所占的大小，再据此算出该读入多少扇区（sector）

---
## Exercise 4

---
## Exercise 5

> Q: Identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong.

会引起错误的第一条指令为
```
ljmp    $PROT_MODE_CSEG, $protcseg
```
因为`protcseg`不是位置无关代码（position indepandent code）。该地址在链接时确定，但是BIOS将bootloader加载到的地址却是固定的（0x7c00）。因此若改变了链接地址，会导致该指令跳转到错误的位置

---
## Exercise 6

> Q: Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint?

在刚进入bootloader时，那些内存位置均为0

进入kernel时，内存数据如下：

```
(gdb) x/8x 0x100000
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x2000b812	0x220f0011	0xc0200fd8
```
这些数据为bootloader所加载的.text段的开头：
```
// entry.S
.text
.align 4
.long MULTIBOOT_HEADER_MAGIC
.long MULTIBOOT_HEADER_FLAGS
.long CHECKSUM
.globl		_start
_start = RELOC(entry)
.globl entry
entry:
	movw	$0x1234,0x472
	movl	$(RELOC(entry_pgdir)), %eax
	movl	%eax, %cr3
	movl	%cr0, %eax
    ...
```
```
// kernel.asm
.globl entry
entry:
f0100000:	02 b0 ad 1b 00 00    	add    0x1bad(%eax),%dh
f0100006:	00 00                	add    %al,(%eax)
f0100008:	fe 4f 52             	decb   0x52(%edi)
f010000b:	e4                   	.byte 0xe4
f010000c <entry>:
f010000c:	66 c7 05 72 04 00 00 	movw   $0x1234,0x472
f0100013:	34 12 
f0100015:	b8 00 20 11 00       	mov    $0x112000,%eax
f010001a:	0f 22 d8             	mov    %eax,%cr3
f010001d:	0f 20 c0             	mov    %cr0,%eax
f0100020:	......
```

---
## Exercise 7

> Q: What is the first instruction after the new mapping is established that would fail to work properly if the mapping weren't in place?

初次出问题的指令：
```
jmp	*%eax
```
此时`%eax`储存的为`0xf010002f`，若初始使用的页表没有合理映射，可能会使跳转出问题。

---
## Exercise 8

> We have omitted a small fragment of code - the code necessary to print octal numbers using patterns of the form "%o". Find and fill in this code fragment.

修改`printfmt.c`中的`vprintfmt`函数：
```C
// (unsigned) octal
case 'o':
    // Replace this with your code.
    num = getuint(&ap, lflag);
    base = 8;
    goto number;
```

<br/>

> Q1: Explain the interface between printf.c and console.c. Specifically, what function does console.c export? How is this function used by printf.c?

`console.c`导出`cputchar`函数供`printf.c`中的`putch`函数使用：
```C
// printf.c
static void
putch(int ch, int *cnt)
{
	cputchar(ch); // from console.c
	*cnt++;
}
```
`printf.c`中的`putch`作为参数传入`printfmt.c`中的`vprintfmt`函数

<br/>

> Q2: Explain the following from console.c:
```C
1 if (crt_pos >= CRT_SIZE) {
2   int i;
3   memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
4   for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
5       crt_buf[i] = 0x0700 | ' ';
6   crt_pos -= CRT_COLS;
7 }
```
改段代码用于滚屏，也就是当当前输出位置`crt_pos`大于屏幕容量的时候，不断将屏幕上移（每次上移一行）并更新`crt_pos`，直到`crt_pos`位于屏幕内。

<br/>

> Q3: Answer the following questions: 

```C
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
```
<br/>

> Q3.1: In the call to cprintf(), to what does fmt point? To what does ap point?

`fmt`指向字符串`"x %d, y %x, z %d\n"`，也即`8(%ebp)`位置处的第一个参数。`ap`指向可变参数列表，也即`12(%ebp)`位置处的第二个参数

<br/>

> Q3.2: List (in order of execution) each call to cons_putc, va_arg, and vcprintf. For cons_putc, list its argument as well. For va_arg, list what ap points to before and after the call. For vcprintf list the values of its two arguments.

```C
vcprintf("x %d, y %x, z %d\n", va_list{x, y, z})
cons_putc('x')
cons_putc(' ')
va_arg, ap: va_list{x, y, z} => va_list{y, z}
cons_putc('1')
cons_putc(',')
cons_putc(' ')
cons_putc('y')
cons_putc(' ')
va_arg, ap: va_list{y, z} => va_list{z}
cons_putc('3')
cons_putc(',')
cons_putc(' ')
cons_putc('z')
cons_putc(' ')
va_arg, ap: va_list{z} => va_list{}
cons_putc('4')
cons_putc('4')
cons_putc('\n')
```

<br/>

> Q4: What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise. Here's an ASCII table that maps bytes to characters.
> 
> The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?

    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s", 57616, &i);

输出为`He110 World`

57616的16进制表示为110，而十六进制数72,6c,64在ASCII码中对应的字符分别为r, l, d

若为大端法，则只需令`i = 0x726c6400`，无需改动57616

<br/>

> Q5: In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?

```C
cprintf("x=%d y=%d", 3);
```
将会输出`12(%ebp)`处的值

<br/>

> Q6: Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?

将其接口改为
```C
cprintf(..., int n, const char* fmt)
```
其中`n`可变参数的个数。

或者
```C
cprintf(..., const char* fmt)
```
其中可变参数倒序输入。

如果可变参数正序输入但是又没有输入其个数的话，会给后续的处理带来不必要的麻烦（可能需要至少两趟对`fmt`的遍历）

---
## Exercise 9

> Q: Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?

初始化栈指针的指令为：
```
// entry.S
movl	$(bootstacktop),%esp
```
初始的栈所在位置为一个.data段：
```
// entry.S
.data
	.p2align	PGSHIFT
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop   
bootstacktop:
```
如上述代码，采用`.space KSTKSIZE`为栈静态分配空间

栈指针初始指向`bootstacktop`，即该栈空间的地址最高处

---
## Exercise 10

> Q: How many 32-bit words does each recursive nesting level of test_backtrace push on the stack, and what are those words?

递归调用自身时，`test_backtrace`先将`x-1`压栈，再将返回地址压栈，再将`%ebp`压栈，共3个32位数。

---
## Exercise 11-12

在`commands`中插入：
```C
{"backtrace", "Backtrace the call of functions", mon_backtrace},
```
在`monitor`添加函数：
```C
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
    uint32_t ebp, eip, arg;
    struct Eipdebuginfo info;

    cprintf("Stack backtrace:\n");

    for(ebp = read_ebp(); ebp != 0; ebp = *(uint32_t*)(ebp)) {
        cprintf("  ebp %08x", ebp);
        eip = *((uint32_t*)ebp + 1);
        cprintf("  eip %08x", eip);
        cprintf("  args");
        for(int i=0; i<5; ++i) {
            arg = *((uint32_t*)ebp + 2 + i);
            cprintf(" %08x", arg);
        }
        cprintf("\n");
        debuginfo_eip(eip, &info);
        cprintf("         %s:%d: %.*s+%u\n", 
            info.eip_file, 
            info.eip_line, 
            info.eip_fn_namelen, info.eip_fn_name, 
            eip - info.eip_fn_addr);
    }
	return 0;
}
```

在`debuginfo_eip`函数中插入：

```C
// Your code here.
stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
info->eip_line = stabs[lline].n_desc;
```

---
## Challenge

采用ANSI ESC Sequence嵌入来实现彩色字体的显示，如：

```C
"Hello World" => "\033[<FG>;<BG>mHello World\033[0m"
```

其中`<FG> <BG>`为前景色，背景色序号：

| 颜色 | 前景序号 | 背景序号 |
|-----|---------|---------|
|黑   |30       |40       |
|红   |31       |41       |
|绿   |32       |42       |
|黄   |33       |43       |
|蓝   |34       |44       |
|紫   |35       |45       |
|青   |36       |46       |
|白   |37       |47       |

在`stdio.h`中加入颜色设置接口：
```C

// color enum
enum {
    COLOR_BLACK = 0,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE,
    COLOR_NUM,
};

// set and reset the foreground color
void set_fgcolor(int color);
void reset_fgcolor();

// set and reset the background color
void set_bgcolor(int color);
void reset_bgcolor();

```
在`printf.c`中实现：
```C

static int color[2] = {-1, -1};
#define FG 0
#define BG 1
#define BUFSZ 4096
#define INF 0x7fffffff

static const char* 
ansi_esc_embed(const char* input)
{
	static const char fgbg[] = "34";
	static const char num[] = "0123456789";
	static const char esc[] = "\033[";
	static char buf[BUFSZ];
	
	char* bufp = buf;

	bufp += strlcpy(bufp, esc, INF);
	for (int i = 0; i < 2; ++i) {
		if (color[i] != -1) {
			*bufp++ = fgbg[i];
			*bufp++ = num[color[i]];
			*bufp++ = ';';
		}
	}
	*(bufp - 1) = 'm';
	bufp += strlcpy(bufp, input, INF);
	bufp += strlcpy(bufp, esc, INF);
	*bufp++ = '0';
	*bufp++ = 'm';
	*bufp = '\0';

	return buf;
}

inline static void 
set_color(int clr, int fgbg)
{
	if (clr >= 0 && clr < COLOR_NUM) {
		color[fgbg] = clr;
	}
}

void 
set_fgcolor(int clr) 
{
	set_color(clr, FG);
}

void
set_bgcolor(int clr)
{
	set_color(clr, BG);
}

void 
reset_fgcolor()
{
	color[FG] = -1;
}

void 
reset_bgcolor()
{
	color[BG] = -1;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;
	
	if (color[0] != -1 || color[1] != -1)
		fmt = ansi_esc_embed(fmt);
	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}
```
在`init.c`中添加一些有趣的测试：
```C
void 
rainbow(int stride)
{
	static const char msg[] = "rainbow!";
	for (int i = 0; i < COLOR_NUM; ++i) {
		set_fgcolor(i);
		set_bgcolor((i + stride) % COLOR_NUM);
		cprintf("%c", msg[i % (sizeof(msg) - 1)]);
	}
	reset_fgcolor();
	reset_bgcolor();	
	cprintf("\n");
}

void 
test_rainbow()
{
	for(int i = 1; i < COLOR_NUM; ++i)
		rainbow(i);
}
```
测试效果：

![rainbow](assets/rainbow.png)

---
## Some problem about stab_binsearch

当`stabs[m].n_value == addr`时，原始代码的处理感觉有些问题：
```C
// exact match for 'addr', but continue loop to find
// *region_right
*region_left = m;
l = m;
addr++;
```
万一`addr+1`处的地址也是符合所要求的`type`时，所得到的匹配范围就会出错。

因此建议改成：
```C
// exact match for 'addr', but continue loop to find
// *region_right
*region_left = m;
// l = m;
// addr++;
l = m + 1;
```
因为已经有一个恰好匹配了，所以之后的`stabs[m].n_value`必然比`addr`大，故而不会修改`*region_left`，而且找到的`*region_right`也符合定义

还有关于最后的`else`分句：
```C
// find rightmost region containing 'addr'
for (l = *region_right;
    l > *region_left && stabs[l].n_type != type;
    l--)
/* do nothing */;
*region_left = l;
```
个人感觉也不是必要的，因为前面的循环（修改后）已经保证`*region_left + 1`和`*region_right`之间没有符合`type`的symbol了