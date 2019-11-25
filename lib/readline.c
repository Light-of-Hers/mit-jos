#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/string.h>

// Handle the extended ASCII code inputed by console
inline static void handle_ext_ascii(int c);
// Handle the escape sequence inputed by serial (user-terminal)
inline static void handle_esc_seq(void);

// Move the cursor right
inline static void move_right(void);
// Move the cursor left
inline static void move_left(void);

// Flush buffer's [cur, tail) to the displays
// and move the cursor back
inline static void flush_buf(void);

// Insert char to current cursor
inline static void insert_char(int c);
// Remove current cursor's char
inline static void remove_char(void);
// Terminate the input
inline static void end_input(void);

#define BUFLEN 1024
static char buf[BUFLEN];

// Current position of cursor
static int cur;
// Tail of buffer
static int tail;

static int echoing;

char *
readline(const char *prompt)
{
	int c;

#if JOS_KERNEL
	if (prompt != NULL)
		cprintf("%s", prompt);
#else
	if (prompt != NULL)
		fprintf(1, "%s", prompt);
#endif

	cur = tail = 0;
	echoing = iscons(0);
	while (1) {
		c = getchar();
		if (c < 0) {
			if (c != -E_EOF)
				cprintf("read error: %e\n", c);
			return NULL;
		} else if ((c == '\b' || c == '\x7f') && cur > 0) {
			remove_char();
		} else if (c >= ' ' && c <= '~' && tail < BUFLEN-1) {
			// Must have c <= '~',
			// because DEL(0x7f) is larger than '~'
			// and it will be inputed when you push
			// 'backspace' in user-terminal
			insert_char(c);
		} else if (c == '\n' || c == '\r') {
			end_input();
			return buf;
		} else if (c == '\x1b') {
			handle_esc_seq(); // only serial will input esc
		} else if (c > '\x7f') {
			handle_ext_ascii(c); // only console will input extended ascii
		}
	}
}

inline static void 
flush_buf(void)
{
	for (int i = cur; i < tail; ++i)
		cputchar(buf[i]);
	for (int i = cur; i < tail; ++i)
		cputchar('\b'); // cursor move back
}

inline static void 
insert_char(int c) 
{
	if (cur == tail) {
		tail++, buf[cur++] = c;
		if (echoing)
			cputchar(c);
	} else { // general case
		memmove(buf + cur + 1, buf + cur, tail - cur);
		buf[cur] = c, tail++;
		if (echoing) 
			flush_buf();
		move_right();
	}
}

inline static void 
remove_char(void)
{
	if (cur == tail) {
		cur--, tail--;
		if (echoing)
			cputchar('\b'), cputchar(' '), cputchar('\b');
	} else { // general case
		memmove(buf + cur - 1, buf + cur, tail - cur);
		buf[tail - 1] = ' ';
		move_left();
		if (echoing)
			flush_buf();
		tail--;
	}
}

inline static void 
move_left(void)
{
	if (cur > 0) {
		if (echoing)
			cputchar('\b');
		cur--;
	}
}

inline static void 
move_right(void)
{
	if (cur < tail) {
		if (echoing)
			cputchar(buf[cur]);
		cur++;
	}
}

inline static void 
end_input(void)
{
	if (echoing) {
		for (; cur < tail; cputchar(buf[cur++]))
			/* move the cursor to the tail */;
		cputchar('\n');
	}
	cur = tail;
	buf[tail] = 0;
}

#define EXT_ASCII_LF 228
#define EXT_ASCII_RT 229
#define EXT_ASCII_UP 226
#define EXT_ASCII_DN 227

inline static void 
handle_ext_ascii(int c)
{
	switch(c) {
	case EXT_ASCII_LF:
		move_left();
		return;
	case EXT_ASCII_RT: 
		move_right();
		return;
	}
	insert_char(c);
}

#define ESC_LF 'D'
#define ESC_RT 'C'
#define ESC_UP 'A'
#define ESC_DN 'B'

inline static void 
handle_esc_seq(void)
{
	char a, b = 0;

	a = getchar();
	if (a == '[') {
		switch(b = getchar()) {
		case ESC_LF: 
			move_left();
			return;
		case ESC_RT:
			move_right();
			return; 
		}
	}
	insert_char(a);
	if (b)
		insert_char(b);
}