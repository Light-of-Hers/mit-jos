// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>

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
	
	if (color[0] != -1 || color[1] != -1)
		fmt = ansi_esc_embed(fmt);
	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

