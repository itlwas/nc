#include "yoc.h"
#include <windows.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
HANDLE hIn, hOut;
static HANDLE old_hOut;
static TCHAR old_title[PATH_MAX];
static DWORD mode;
static bool_t vt_alternate = FALSE;
static wchar_t get_wch(int *special_key);
static bool_t is_ctrl_pressed(INPUT_RECORD *ir);
static void write_console_wide(const wchar_t *ws, size_t wlen);
static void restore_title(void);
static void save_current_title(void);
static void string_to_tchar_array(const char *s, TCHAR t[]);
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (lineptr == NULL || stream == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }
    int c = fgetc(stream);
    if (c == EOF) return -1;
    if (*lineptr == NULL && *n == 0) {
        *n = 128;
        *lineptr = xmalloc(*n);
        if (*lineptr == NULL) return -1;
    }
    size_t pos = 0;
    while (c != EOF) {
        if (pos + 1 >= *n) {
            size_t new_size = *n + (*n >> 2);
            if (new_size < 128) new_size = 128;
            char *new_ptr = xrealloc(*lineptr, new_size);
            if (new_ptr == NULL) return -1;
            *n = new_size;
            *lineptr = new_ptr;
        }
        ((unsigned char *)(*lineptr))[pos++] = (unsigned char)c;
        if (c == '\n') break;
        c = fgetc(stream);
    }
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}
void term_write(const unsigned char *s, size_t len) {
    if (len == 0) return;
    int required = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, (int)len, NULL, 0);
    static wchar_t *wbuf = NULL;
    static size_t wcap = 0;
    if (required <= 0) {
        if (len > wcap) {
            wbuf = (wchar_t *)xrealloc(wbuf, len * sizeof(wchar_t));
            wcap = len;
        }
        for (size_t i = 0; i < len; ++i) {
            wbuf[i] = (s[i] < 0x80) ? (wchar_t)s[i] : L'?';
        }
        write_console_wide(wbuf, len);
        return;
    }
    if ((size_t)required > wcap) {
        wbuf = (wchar_t *)xrealloc(wbuf, (size_t)required * sizeof(wchar_t));
        wcap = (size_t)required;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, (int)len, wbuf, required) != required) {
        size_t n = len;
        if (n > wcap) {
            wbuf = (wchar_t *)xrealloc(wbuf, n * sizeof(wchar_t));
            wcap = n;
        }
        for (size_t i = 0; i < n; ++i) {
            wbuf[i] = (s[i] < 0x80) ? (wchar_t)s[i] : L'?';
        }
        write_console_wide(wbuf, n);
        return;
    }
    write_console_wide(wbuf, (size_t)required);
}
size_t term_read(unsigned char **s, int *special_key) {
    wchar_t c = get_wch(special_key);
    static unsigned char buf[MAXCHARLEN + 1];
    if (c == L'\0') {
        buf[0] = '\0';
        *s = buf;
        return 0;
    }
    wchar_t wcs[2];
    int wlen = 1;
    wcs[0] = c;
    if (c >= 0xD800 && c <= 0xDBFF) {
        int dummy_key;
        wchar_t c2 = get_wch(&dummy_key);
        if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
            wcs[1] = c2;
            wlen = 2;
        }
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wcs, wlen, (LPSTR)buf, MAXCHARLEN, NULL, NULL);
    if (size <= 0) die("WideCharToMultiByte");
    buf[size] = '\0';
    *s = buf;
    return (size_t)size;
}
void term_init(void) {
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) die("GetStdHandle");
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE) die("GetStdHandle");
    DWORD outMode;
    if (GetConsoleMode(hStdOut, &outMode)) {
        DWORD withVT = outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (SetConsoleMode(hStdOut, withVT)) {
            vt_alternate = TRUE;
            old_hOut = hStdOut;
            hOut = hStdOut;
        } else {
            SetConsoleMode(hStdOut, outMode);
        }
    }
    term_switch_to_alt();
    save_current_title();
    term_set_title("yoc");
    term_enable_raw();
}
void term_get_win_size(size_t *x, size_t *y) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
        die("GetConsoleScreenBufferInfo");
    }
    if (x) *x = (size_t)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    if (y) *y = (size_t)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
}
void term_clear_line(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) die("GetConsoleScreenBufferInfo");
    COORD coordScreen;
    coordScreen.X = 0;
    coordScreen.Y = csbi.dwCursorPosition.Y;
    DWORD cCharsWritten;
    if (!FillConsoleOutputCharacter(hOut, (TCHAR)' ', (DWORD)csbi.dwSize.X, coordScreen, &cCharsWritten)) die("FillConsoleOutputCharacter");
    if (!FillConsoleOutputAttribute(hOut, csbi.wAttributes, (DWORD)csbi.dwSize.X, coordScreen, &cCharsWritten)) die("FillConsoleOutputAttribute");
    if (!SetConsoleCursorPosition(hOut, coordScreen)) die("SetConsoleCursorPosition");
}
void term_clear_screen(void) {
    COORD coordScreen = {0, 0};
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) die("GetConsoleScreenBufferInfo");
    DWORD dwConSize = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
    DWORD cCharsWritten;
    if (!FillConsoleOutputCharacter(hOut, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten)) die("FillConsoleOutputCharacter");
    if (!FillConsoleOutputAttribute(hOut, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten)) die("FillConsoleOutputAttribute");
    if (!SetConsoleCursorPosition(hOut, coordScreen)) die("SetConsoleCursorPosition");
}
void term_set_title(const char *title) {
    TCHAR new_title[MAX_PATH];
    string_to_tchar_array(title, new_title);
    if (!SetConsoleTitle(new_title)) {
        die("SetConsoleTitle");
    }
    atexit(restore_title);
}
void term_disable_raw(void) {
    if (!SetConsoleMode(hIn, mode)) {
        die("SetConsoleMode");
    }
}
void term_enable_raw(void) {
    atexit(term_disable_raw);
    if (!GetConsoleMode(hIn, &mode)) {
        die("GetConsoleMode");
    }
    if (!(SetConsoleMode(hIn, mode & (DWORD)~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT)))) {
        die("SetConsoleMode");
    }
}
void term_switch_to_norm(void) {
	if (vt_alternate) {
		static const char seq[] = "\x1b[?1049l";
		term_write((const unsigned char *)seq, sizeof(seq) - 1);
	} else {
		if (old_hOut == NULL || old_hOut == INVALID_HANDLE_VALUE) {
			return;
		}
		if (!SetConsoleActiveScreenBuffer(old_hOut)) {
			die("SetConsoleActiveScreenBuffer");
		}
	}
}
void term_switch_to_alt(void) {
    if (vt_alternate) {
        static const char seq[] = "\x1b[?1049h";
        term_write((const unsigned char *)seq, sizeof(seq) - 1);
        atexit(term_switch_to_norm);
    } else {
        old_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        hOut = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        if (old_hOut == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) {
            die("CreateConsoleScreenBuffer");
        }
        if (!SetConsoleActiveScreenBuffer(hOut)) {
            die("SetConsoleActiveScreenBuffer");
        }
        atexit(term_switch_to_norm);
    }
}
void term_hide_cursor(void) {
    CONSOLE_CURSOR_INFO cursorInfo;
    if (!GetConsoleCursorInfo(hOut, &cursorInfo)) die("GetConsoleCursorInfo");
    cursorInfo.bVisible = 0;
    if (!SetConsoleCursorInfo(hOut, &cursorInfo)) die("SetConsoleCursorInfo");
}
void term_show_cursor(void) {
    CONSOLE_CURSOR_INFO cursorInfo;
    if (!GetConsoleCursorInfo(hOut, &cursorInfo)) die("GetConsoleCursorInfo");
    cursorInfo.bVisible = 1;
    if (!SetConsoleCursorInfo(hOut, &cursorInfo)) die("SetConsoleCursorInfo");
}
void term_set_cursor(size_t x, size_t y) {
    COORD coord;
    coord.X = (SHORT)x;
    coord.Y = (SHORT)y;
    if (!SetConsoleCursorPosition(hOut, coord)) {
        die("SetConsoleCursorPosition");
    }
}
bool_t fs_exists(const char *path) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen <= 0) {
        return FALSE;
    }
    wchar_t *wpath = (wchar_t *)xmalloc((size_t)wlen * sizeof(wchar_t));
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen) != wlen) {
        free(wpath);
        return FALSE;
    }
    DWORD dwAttrib = GetFileAttributesW(wpath);
    free(wpath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
void fs_canonicalize(const char *path, char *out, size_t size) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (wlen > 0) {
        wchar_t *wpath = (wchar_t *)xmalloc((size_t)wlen * sizeof(wchar_t));
        if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen) == wlen) {
            DWORD need = GetFullPathNameW(wpath, 0, NULL, NULL);
            if (need > 0) {
                wchar_t *wfull = (wchar_t *)xmalloc((size_t)need * sizeof(wchar_t));
                DWORD written = GetFullPathNameW(wpath, need, wfull, NULL);
                if (written > 0 && written < need) {
                    int u8len = WideCharToMultiByte(CP_UTF8, 0, wfull, -1, NULL, 0, NULL, NULL);
                    if (u8len > 0) {
                        if ((size_t)u8len <= size) {
                            (void)WideCharToMultiByte(CP_UTF8, 0, wfull, -1, out, u8len, NULL, NULL);
                            free(wfull);
                            free(wpath);
                            return;
                        } else if (size > 0) {
                            (void)WideCharToMultiByte(CP_UTF8, 0, wfull, -1, out, (int)size - 1, NULL, NULL);
                            out[size - 1] = '\0';
                            free(wfull);
                            free(wpath);
                            return;
                        }
                    }
                }
                free(wfull);
            }
        }
        free(wpath);
    }
    if (size > 0) {
        strncpy(out, path, size - 1);
        out[size - 1] = '\0';
    }
}
static void write_console_wide(const wchar_t *ws, size_t wlen) {
    while (wlen > 0) {
        DWORD written;
        if (!WriteConsoleW(hOut, ws, (DWORD)wlen, &written, NULL)) {
            die("WriteConsoleW");
        }
        ws += written;
        wlen -= written;
    }
}
static wchar_t get_wch(int *special_key) {
    INPUT_RECORD input;
    DWORD nread;
    wchar_t retval = L'\0';
    *special_key = 0;
    for (;;) {
        if (!ReadConsoleInputW(hIn, &input, 1, &nread)) {
            die("ReadConsoleInputW");
        }
        if (input.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            render_refresh();
            *special_key = 0;
            return L'\0';
        }
        if (input.EventType == KEY_EVENT && input.Event.KeyEvent.bKeyDown) {
            break;
        }
    }
    WORD keycode = input.Event.KeyEvent.wVirtualKeyCode;
    WORD unicode = input.Event.KeyEvent.uChar.UnicodeChar;
    if (is_ctrl_pressed(&input)) {
        if (keycode >= 'A' && keycode <= 'Z') {
            *special_key = CTRL_KEY(keycode);
        } else {
            switch (keycode) {
                case VK_END:   *special_key = CTRL_END;        break;
                case VK_HOME:  *special_key = CTRL_HOME;       break;
                case VK_LEFT:  *special_key = CTRL_ARROW_LEFT; break;
                case VK_UP:    *special_key = CTRL_ARROW_UP;   break;
                case VK_RIGHT: *special_key = CTRL_ARROW_RIGHT;break;
                case VK_DOWN:  *special_key = CTRL_ARROW_DOWN; break;
            }
        }
    } else if (keycode >= VK_F1 && keycode <= VK_F12) {
        *special_key = keycode;
    } else {
        switch (keycode) {
            case VK_BACK:   *special_key = BACKSPACE;   break;
            case VK_TAB:    *special_key = TAB;         break;
            case VK_RETURN: *special_key = ENTER;       break;
            case VK_ESCAPE: *special_key = ESC;         break;
            case VK_PRIOR:  *special_key = PAGE_UP;     break;
            case VK_NEXT:   *special_key = PAGE_DOWN;   break;
            case VK_END:    *special_key = END;         break;
            case VK_HOME:   *special_key = HOME;        break;
            case VK_LEFT:   *special_key = ARROW_LEFT;  break;
            case VK_UP:     *special_key = ARROW_UP;    break;
            case VK_RIGHT:  *special_key = ARROW_RIGHT; break;
            case VK_DOWN:   *special_key = ARROW_DOWN;  break;
            case VK_INSERT: *special_key = DEL + 1;     break;
            case VK_DELETE: *special_key = DEL;         break;
            default:
                *special_key = 0;
                retval = unicode;
                break;
        }
    }
    return retval;
}
static bool_t is_ctrl_pressed(INPUT_RECORD *ir) {
    return (ir->Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
}
static void string_to_tchar_array(const char *s, TCHAR t[]) {
    while ((*t++ = *s++));
}
static void save_current_title(void) {
    if (!GetConsoleTitle(old_title, MAX_PATH)) {
        die("GetConsoleTitle");
    }
}
static void restore_title(void) {
    if (!SetConsoleTitle(old_title)) {
        die("SetConsoleTitle");
    }
}
