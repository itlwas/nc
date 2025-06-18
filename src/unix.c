#include "yoc.h"
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
static struct termios orig_termios;
static void handle_winch(int sig);
void term_write(const unsigned char *s, size_t len) {
	if (write(STDOUT_FILENO, s, len) == -1)
		die("write");
}
size_t term_read(unsigned char **s, int *special_key) {
	static unsigned char buf[64];
	int nread;
	*s = NULL;
	for (;;) {
		nread = read(STDIN_FILENO, buf, sizeof(buf) - 1);
		if (nread > 0) {
			break;
		}
		if (nread == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			}
			die("read");
		}
	}
	buf[nread] = '\0';
	if (buf[0] == '\x1b') {
		if (nread == 1) {
			*special_key = ESC;
		} else if (buf[1] == '[') {
			if (nread == 3) {
				switch (buf[2]) {
					case 'A': *special_key = ARROW_UP; break;
					case 'B': *special_key = ARROW_DOWN; break;
					case 'C': *special_key = ARROW_RIGHT; break;
					case 'D': *special_key = ARROW_LEFT; break;
					case 'H': *special_key = HOME; break;
					case 'F': *special_key = END; break;
				}
			} else if (nread > 3 && buf[nread - 1] == '~') {
				switch (buf[2]) {
					case '1': *special_key = HOME; break;
					case '3': *special_key = DEL; break;
					case '4': *special_key = END; break;
					case '5': *special_key = PAGE_UP; break;
					case '6': *special_key = PAGE_DOWN; break;
					case '7': *special_key = HOME; break;
					case '8': *special_key = END; break;
				}
			}
		} else {
			*s = buf;
			return (size_t)nread;
		}
		return 0;
	}
	if (buf[0] < ' ' || buf[0] == 127) {
		*special_key = buf[0] == 127 ? BACKSPACE : buf[0];
		return 0;
	}
	*s = buf;
	return (size_t)nread;
}
void term_init(void) {
	term_switch_to_alt();
	term_set_title("yoc");
	term_enable_raw();
}
void term_get_win_size(size_t *x, size_t *y) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
		die("ioctl");
	*x = ws.ws_col;
	*y = ws.ws_row;
}
void term_clear_line(void) {
	term_write((unsigned char *)"\x1b[K", 3);
}
void term_clear_screen(void) {
	term_write((unsigned char *)"\x1b[2J", 4);
}
void term_set_title(const char *title) {
	char buf[256];
	int len = snprintf(buf, sizeof(buf), "\x1b]0;%s\x07", title);
	term_write((unsigned char *)buf, (size_t)len);
}
void term_disable_raw(void) {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetattr");
}
void term_enable_raw(void) {
	struct termios raw;
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
		die("tcgetattr");
	atexit(term_disable_raw);
	raw = orig_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
	signal(SIGWINCH, handle_winch);
}
void term_switch_to_norm(void) {
	static const char seq[] = "\x1b[?1049l";
	term_write((unsigned char *)seq, sizeof(seq) - 1);
}
void term_switch_to_alt(void) {
	static const char seq[] = "\x1b[?1049h";
	term_write((unsigned char *)seq, sizeof(seq) - 1);
	atexit(term_switch_to_norm);
}
void term_hide_cursor(void) {
	term_write((unsigned char *)"\x1b[?25l", 6);
}
void term_show_cursor(void) {
	term_write((unsigned char *)"\x1b[?25h", 6);
}
void term_set_cursor(size_t x, size_t y) {
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "\x1b[%zu;%zuH", y + 1, x + 1);
	term_write((unsigned char *)buf, (size_t)len);
}
bool_t is_file_exist(char *filename) {
	struct stat buffer;
	return (stat(filename, &buffer) == 0);
}
void file_canonicalize_path(const char *path, char *out_path, size_t out_size) {
	char tmp[PATH_MAX];
	if (realpath(path, out_path) == NULL) {
		strncpy(out_path, path, out_size - 1);
		out_path[out_size - 1] = '\0';
	}
	strncpy(tmp, out_path, sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';
	char *slash = strrchr(tmp, '/');
	const char *base = slash ? slash + 1 : tmp;
	char dirbuf[PATH_MAX];
	DIR *dir;
	if (slash) {
		*slash = '\0';
		dir = opendir(tmp[0] ? tmp : "/");
		strncpy(dirbuf, tmp, sizeof(dirbuf) - 1);
		dirbuf[sizeof(dirbuf) - 1] = '\0';
	} else {
		dir = opendir(".");
		dirbuf[0] = '\0';
	}
	if (dir) {
		struct dirent *de;
		while ((de = readdir(dir)) != NULL) {
			if (strcasecmp(de->d_name, base) == 0) {
				if (slash) {
					size_t needed = strlen(dirbuf) + 1 + strlen(de->d_name) + 1;
					if (needed <= out_size) {
						snprintf(out_path, out_size, "%s/%s", dirbuf, de->d_name);
					}
				} else {
					snprintf(out_path, out_size, "%s", de->d_name);
				}
				break;
			}
		}
		closedir(dir);
	}
}
static void handle_winch(int sig) {
	(void)sig;
	render_refresh();
}
