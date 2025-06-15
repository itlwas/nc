#include "yoc.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/stat.h>
static int get_char(void *c);
static int parse_input(unsigned char *s);
static int parse_sequence(unsigned char *s);
static int parse_escape_sequence(void);
void write_console(const unsigned char *s, size_t len) {
	if (write(STDOUT_FILENO, s, len) == -1)
		die("write");
}
void writeln_console(const unsigned char *s, size_t len) {
	write_console(s, len);
	write_console((unsigned char *)"\n\r", 2);
}
size_t read_console(unsigned char **s, int *special_key) {
	unsigned char input;
	while (get_char(&input) != 1);
	size_t len = utf8_len(input);
	*s = (unsigned char *)malloc(len + 1);
	if (!*s)
		die("realloc");
	(*s)[0] = input;
	size_t size = 1;
	for (size_t i = 1; i < len && get_char(&input) == 1; ++i, ++size)
		(*s)[i] = input;
	(*s)[size] = '\0';
	*special_key = parse_input(*s);
	return *special_key ? 0 : size;
}
static int get_char(void *c) {
	int nread = read(STDIN_FILENO, c, 1);
	if (nread == -1 && errno != EAGAIN)
		die("read");
	return nread;
}
static int parse_input(unsigned char *s) {
	return (s[0] == ESC) ? parse_escape_sequence() : parse_sequence(s);
}
static int parse_sequence(unsigned char *s) {
	if (s[0] == '\x7f')
		return BACKSPACE;
	if (s[0] == TAB)
		return TAB;
	if (s[0] == ENTER)
		return ENTER;
	if (s[0] >= CTRL_KEY('a') && s[0] <= CTRL_KEY('z'))
		return CTRL_KEY(s[0]);
	return 0;
}
static int parse_escape_sequence(void) {
	unsigned char buf[5] = {0};
	if (get_char(&buf[0]) == -1)
		return ESC;
	if (get_char(&buf[1]) == -1)
		return ESC;
	switch (buf[0]) {
	case '[':
		if (buf[1] >= '0' && buf[1] <= '9') {
			if (get_char(&buf[2]) == -1)
				return ESC;
			if (buf[2] == '~') {
				switch (buf[1]) {
				case '1': return HOME;
				case '2': return INSERT;
				case '3': return DEL;
				case '4': return END;
				case '5': return PAGE_UP;
				case '6': return PAGE_DOWN;
				case '7': return HOME;
				case '8': return END;
				}
			} else if (buf[2] >= '0' && buf[2] <= '9') {
				if (get_char(&buf[3]) == -1)
					return ESC;
				if (buf[3] == '~') {
					switch (buf[1]) {
					case '1':
						switch (buf[2]) {
						case '5': return F_KEY(5);
						case '7': return F_KEY(6);
						case '8': return F_KEY(7);
						case '9': return F_KEY(8);
						}
						break;
					case '2':
						switch (buf[2]) {
						case '0': return F_KEY(9);
						case '1': return F_KEY(10);
						case '3': return F_KEY(11);
						case '4': return F_KEY(12);
						}
						break;
					}
				}
			} else if (buf[2] == ';') {
				if (get_char(&buf[3]) == -1)
					return ESC;
				if (get_char(&buf[4]) == -1)
					return ESC;
				if (buf[3] == '5') {
					switch (buf[4]) {
					case 'A': return CTRL_ARROW_UP;
					case 'B': return CTRL_ARROW_DOWN;
					case 'C': return CTRL_ARROW_RIGHT;
					case 'D': return CTRL_ARROW_LEFT;
					case 'F': return CTRL_END;
					case 'H': return CTRL_HOME;
					}
				}
			}
		} else {
			switch (buf[1]) {
			case 'A': return ARROW_UP;
			case 'B': return ARROW_DOWN;
			case 'C': return ARROW_RIGHT;
			case 'D': return ARROW_LEFT;
			case 'F': return END;
			case 'H': return HOME;
			}
		}
		break;
	case 'O':
		switch (buf[1]) {
		case 'A': return ARROW_UP;
		case 'B': return ARROW_DOWN;
		case 'C': return ARROW_RIGHT;
		case 'D': return ARROW_LEFT;
		case 'P': return F_KEY(1);
		case 'Q': return F_KEY(2);
		case 'R': return F_KEY(3);
		case 'S': return F_KEY(4);
		}
		break;
	}
	return ESC;
}
static struct termios orig_termios;
static struct sigaction sa;
static void set_sigaction(void);
static void sigaction_init(void);
static void handle_signal(int signal);
static void create_signal_handler(void);
static void delete_signal_handler(void);
void term_init(void) {
	set_title("yoc");
	enable_raw_mode();
	sigaction_init();
	create_signal_handler();
	atexit(delete_signal_handler);
	switch_to_alternate_buffer();
}
void get_window_size(size_t *x, size_t *y) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			die("write");
		get_cursor_position(x, y);
		if (x)
			++(*x);
		if (y)
			++(*y);
	} else {
		if (x)
			*x = ws.ws_col;
		if (y)
			*y = ws.ws_row;
	}
}
void clear_line(void) {
	if (write(STDOUT_FILENO, "\x1b[2K", 4) != 4)
		die("write");
}
void clear_screen(void) {
	if (write(STDOUT_FILENO, "\x1b[2J", 4) != 4)
		die("write");
	if (write(STDOUT_FILENO, "\x1b[H", 3) != 3)
		die("write");
}
void set_title(const char *title) {
	char buff[64] = "";
	int len = sprintf(buff, "\x1b]0;%s\x7", title);
	if (len < 0)
		die("sprintf");
	if (write(STDOUT_FILENO, buff, (size_t)len) == -1)
		die("write");
}
void disable_raw_mode(void) {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}
void enable_raw_mode(void) {
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		die("tcgetattr");
	struct termios raw = orig_termios;
	atexit(disable_raw_mode);
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN]  = 0;
	raw.c_cc[VTIME] = 1;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}
void get_buffer_size(size_t *x, size_t *y) {
	get_window_size(x, y);
}
void switch_to_normal_buffer(void) {
	if (write(STDOUT_FILENO, "\x1b[?1049l", 8) != 8)
		die("write");
}
void switch_to_alternate_buffer(void) {
	if (write(STDOUT_FILENO, "\x1b[?1049h\x1b[H", 11) != 11)
		die("write");
	atexit(switch_to_normal_buffer);
}
void hide_cursor(void) {
	if (write(STDOUT_FILENO, "\x1b[?25l", 6) != 6)
		die("write");
}
void show_cursor(void) {
	if (write(STDOUT_FILENO, "\x1b[?25h", 6) != 6)
		die("write");
}
void set_cursor_position(size_t x, size_t y) {
	char buff[32] = "";
	int len = sprintf(buff, "\x1b[%zu;%zuH", y + 1, x + 1);
	if (len < 0)
		die("sprintf");
	if (write(STDOUT_FILENO, buff, (size_t)len) == -1)
		die("write");
}
void get_cursor_position(size_t *x, size_t *y) {
	char buf[32];
	unsigned i = 0;
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		die("write");
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		++i;
	}
	buf[i] = '\0';
	if (buf[0] != '\x1b' || buf[1] != '[')
		die("get_cursor_position");
	if (x) {
		if (sscanf(&buf[4], "%zu", x) != 1)
			die("sscanf");
		--(*x);
	}
	if (y) {
		if (sscanf(&buf[2], "%zu", y) != 1)
			die("sscanf");
		--(*y);
	}
}
static void sigaction_init(void) {
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);
}
static void handle_signal(int signal) {
	switch (signal) {
	case SIGWINCH:
		refresh_screen();
		break;
	}
}
static void create_signal_handler(void) {
	sa.sa_handler = &handle_signal;
	set_sigaction();
}
static void delete_signal_handler(void) {
	sa.sa_handler = SIG_DFL;
	set_sigaction();
}
static void set_sigaction(void) {
	if (sigaction(SIGWINCH, &sa, NULL) == -1)
		die("Cannot handle SIGWINCH");
}
bool is_file_exist(char *filename) {
	struct stat buffer;
	return stat(filename, &buffer) == 0;
}
bool is_continuation_byte(unsigned char c) {
	return utf8_len(c) == UTF8_CONTINUATION_BYTE;
}