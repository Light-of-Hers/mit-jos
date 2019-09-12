// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>

static int fgclr = -1;
static int bgclr = -1;
static const char numbers[] = "0123456789";

void 
set_fgcolor(int clr) 
{
	fgclr = clr;
	cprintf("\033[3%cm", numbers[clr]);
}

void
set_bgcolor(int clr)
{
	bgclr = clr;
	cprintf("\033[4%cm", numbers[clr]);
}

void 
reset_fgcolor()
{
	cprintf("\033[0m");
	if (bgclr != -1)
		cprintf("\033[4%cm", numbers[bgclr]);
	fgclr = -1;
}

void 
reset_bgcolor()
{
	cprintf("\033[0m");
	if (fgclr != -1)
		cprintf("\033[3%cm", numbers[fgclr]);
	bgclr = -1;
}

static void
putch(int ch, int *cnt)
{
	cputchar(ch);
	*cnt++;
}

int
vcprintf(const char *fmt, va_list ap)
{
	int cnt = 0;

	vprintfmt((void*)putch, &cnt, fmt, ap);
	return cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;
	
	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

