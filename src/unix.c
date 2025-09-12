#include "yoc.h"
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
static struct termios orig_termios;
static volatile sig_atomic_t winch_flag = 0;
static void handle_winch(int sig);
void term_write(const unsigned char *s, size_t len) {
    while (len > 0) {
        ssize_t written = write(STDOUT_FILENO, s, len);
        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }
            die("write");
        }
        s += (size_t)written;
        len -= (size_t)written;
    }
}
size_t term_read(unsigned char **s, int *special_key) {
    static unsigned char buf[64];
    ssize_t nread;
    *special_key = 0;
    *s = NULL;
    for (;;) {
        nread = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (nread > 0) {
            break;
        }
        if (nread == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                if (winch_flag) {
                    winch_flag = 0;
                    render_refresh();
                }
                continue;
            }
            die("read");
        }
    }
    buf[nread] = '\0';
    if (buf[0] == '\x1b') {
        if (nread == 1) {
            int pending = 0;
            if (ioctl(STDIN_FILENO, FIONREAD, &pending) == 0 && pending > 0) {
                ssize_t remaining = (ssize_t)(sizeof(buf) - 1) - nread;
                if (remaining < 0) remaining = 0;
                if (pending > remaining) {
                    pending = (int)remaining;
                }
                ssize_t extra = read(STDIN_FILENO, buf + nread, (size_t)pending);
                if (extra > 0) {
                    nread += extra;
                    buf[nread] = '\0';
                }
            }
        }
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
            } else if (
                nread >= 6 &&
                buf[1] == '[' &&
                buf[2] == '1' &&
                buf[3] == ';' &&
                buf[4] == '5'
            ) {
                switch (buf[5]) {
                    case 'A': *special_key = CTRL_ARROW_UP; break;
                    case 'B': *special_key = CTRL_ARROW_DOWN; break;
                    case 'C': *special_key = CTRL_ARROW_RIGHT; break;
                    case 'D': *special_key = CTRL_ARROW_LEFT; break;
                    default: *special_key = 0; break;
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
            } else if (buf[1] == 'O' && nread == 3) {
                switch (buf[2]) {
                    case 'H': *special_key = HOME; break;
                    case 'F': *special_key = END; break;
                    default: *special_key = 0; break;
                }
                return 0;
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
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        die("ioctl");
    }
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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}
void term_enable_raw(void) {
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(term_disable_raw);
    raw = orig_termios;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
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
    int len = snprintf(buf, sizeof(buf), "\x1b[%lu;%luH", (unsigned long)(y + 1), (unsigned long)(x + 1));
    term_write((unsigned char *)buf, (size_t)len);
}
bool_t fs_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return FALSE;
    }
    return S_ISREG(st.st_mode);
}
void fs_canonicalize(const char *path, char *out, size_t size) {
    if (realpath(path, out) == NULL) {
        strncpy(out, path, size - 1);
        out[size - 1] = '\0';
    }
}
static void handle_winch(int sig) {
    (void)sig;
    winch_flag = 1;
}
