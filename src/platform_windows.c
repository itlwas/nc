#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>
#include "yoc.h"
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
HANDLE hIn, hOut;
static HANDLE old_hOut;
static TCHAR old_title[PATH_MAX];
static DWORD mode;
static bool vt_alternate = false;
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
	if (lineptr == NULL || stream == NULL || n == NULL) {
		errno = EINVAL;
		return -1;
	}
	size_t pos = 0;
	int c = fgetc(stream);
	if (c == EOF)
		return -1;
	if (*lineptr == NULL && *n == 0) {
		*lineptr = xmalloc(128);
		if (*lineptr == NULL)
			return -1;
		*n = 128;
	}
	while (c != EOF) {
		if (pos + 1 >= *n) {
			size_t new_size = *n + (*n >> 2);
			if (new_size < 128)
				new_size = 128;
			char *new_ptr = xrealloc(*lineptr, new_size);
			if (new_ptr == NULL)
				return -1;
			*n = new_size;
			*lineptr = new_ptr;
		}
		((unsigned char *)(*lineptr))[pos++] = c;
		if (c == '\n')
			break;
		c = fgetc(stream);
	}
	(*lineptr)[pos] = '\0';
	return pos;
}
static wchar_t get_char(int *special_key);
static bool is_ctrl_pressed(INPUT_RECORD *ir);
static void write_console_wide(const wchar_t *ws, size_t wlen) {
	DWORD written;
	if (!WriteConsoleW(hOut, ws, (DWORD)wlen, &written, NULL))
		die("WriteConsoleW");
}
void write_console(const unsigned char *s, size_t len) {
	if (len == 0)
		return;
	int required = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, (int)len, NULL, 0);
	if (required <= 0)
		die("MultiByteToWideChar");
	wchar_t *wbuf = (wchar_t *)xmalloc((size_t)required * sizeof(wchar_t));
	if (!wbuf)
		die("malloc");
	if (MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, (int)len, wbuf, required) != required) {
		free(wbuf);
		die("MultiByteToWideChar");
	}
	write_console_wide(wbuf, (size_t)required);
	free(wbuf);
}
void writeln_console(const unsigned char *s, size_t len) {
	write_console(s, len);
	static const unsigned char newline[] = "\r\n";
	write_console(newline, 2);
}
size_t read_console(unsigned char **s, int *special_key) {
	wchar_t c = get_char(special_key);
	if (c == '\0') {
		*s = (unsigned char *)xmalloc(1);
		if (!*s)
			die("malloc");
		(*s)[0] = '\0';
		return 0;
	}
	char buff[6] = { '\0' };
	size_t size = WideCharToMultiByte(CP_UTF8, 0, &c, 1, buff, sizeof(buff), NULL, NULL);
	if (size == 0)
		die("WideCharToMultiByte");
	*s = (unsigned char *)xmalloc(size + 1);
	if (!*s)
		die("malloc");
	for (size_t i = 0; i < size; ++i)
		(*s)[i] = buff[i];
	(*s)[size] = '\0';
	return size;
}
static wchar_t get_char(int *special_key) {
	INPUT_RECORD input;
	DWORD nread;
	do {
		if (!ReadConsoleInputW(hIn, &input, 1, &nread))
			die("ReadConsoleInputW");
	} while (!(input.EventType == KEY_EVENT &&
		input.Event.KeyEvent.bKeyDown &&
		input.Event.KeyEvent.wVirtualKeyCode));
	wchar_t retval = '\0';
	WORD keycode = input.Event.KeyEvent.wVirtualKeyCode;
	WORD unicode = input.Event.KeyEvent.uChar.UnicodeChar;
	if (unicode >= CTRL_KEY('a') && unicode <= CTRL_KEY('z')) {
		*special_key = unicode;
	} else if (is_ctrl_pressed(&input)) {
		switch (keycode) {
		case END:
			*special_key = CTRL_END;
			break;
		case HOME:
			*special_key = CTRL_HOME;
			break;
		case ARROW_LEFT:
			*special_key = CTRL_ARROW_LEFT;
			break;
		case ARROW_UP:
			*special_key = CTRL_ARROW_UP;
			break;
		case ARROW_RIGHT:
			*special_key = CTRL_ARROW_RIGHT;
			break;
		case ARROW_DOWN:
			*special_key = CTRL_ARROW_DOWN;
			break;
		default:
			*special_key = '\0';
			retval = unicode;
			break;
		}
	} else if (keycode >= F_KEY(1) && keycode <= F_KEY(12)) {
		*special_key = keycode;
	} else {
		switch (keycode) {
		case BACKSPACE:
		case TAB:
		case ENTER:
		case ESC:
		case PAGE_UP:
		case PAGE_DOWN:
		case END:
		case HOME:
		case ARROW_LEFT:
		case ARROW_UP:
		case ARROW_RIGHT:
		case ARROW_DOWN:
		case SELECT:
		case PRINT:
		case EXECUTE:
		case PRINT_SCREEN:
		case INSERT:
		case DEL:
			*special_key = keycode;
			break;
		default:
			*special_key = '\0';
			retval = unicode;
			break;
		}
	}
	return retval;
}
static bool is_ctrl_pressed(INPUT_RECORD *ir) {
	return (ir->Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED) ||
		(ir->Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED);
}
static void restore_title(void);
static HWND get_console_hwnd(void);
static void save_current_title(void);
static void disable_window_resizing(void);
static void string_to_tchar_array(const char *s, TCHAR t[]);
void term_init(void) {
	hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
		die("GetStdHandle");
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdOut == INVALID_HANDLE_VALUE)
		die("GetStdHandle");
	DWORD outMode;
	if (GetConsoleMode(hStdOut, &outMode)) {
		DWORD withVT = outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (SetConsoleMode(hStdOut, withVT)) {
			vt_alternate = true;
			old_hOut = hStdOut;
			hOut = hStdOut;
		} else {
			SetConsoleMode(hStdOut, outMode);
		}
	}
	switch_to_alternate_buffer();
	disable_window_resizing();
	save_current_title();
	set_title("yoc");
	enable_raw_mode();
}
void get_window_size(size_t *x, size_t *y) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hOut, &csbi))
		die("GetConsoleScreenBufferInfo");
	if (x)
		*x = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	if (y)
		*y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}
void clear_line(void) {
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hOut, &csbi))
		die("GetConsoleScreenBufferInfo");
	COORD coordScreen = {0, csbi.dwCursorPosition.Y};
	if (!FillConsoleOutputCharacter(hOut, (TCHAR)' ', csbi.dwSize.X, coordScreen, &cCharsWritten))
		die("FillConsoleOutputCharacter");
	if (!FillConsoleOutputAttribute(hOut, csbi.wAttributes, csbi.dwSize.X, coordScreen, &cCharsWritten))
		die("FillConsoleOutputAttribute");
	if (!SetConsoleCursorPosition(hOut, coordScreen))
		die("SetConsoleCursorPosition");
}
void clear_screen(void) {
	COORD coordScreen = {0, 0};
	DWORD cCharsWritten, dwConSize;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hOut, &csbi))
		die("GetConsoleScreenBufferInfo");
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
	if (!FillConsoleOutputCharacter(hOut, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten))
		die("FillConsoleOutputCharacter");
	if (!FillConsoleOutputAttribute(hOut, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten))
		die("FillConsoleOutputAttribute");
	if (!SetConsoleCursorPosition(hOut, coordScreen))
		die("SetConsoleCursorPosition");
}
void set_title(const char *title) {
	TCHAR new_title[MAX_PATH];
	string_to_tchar_array(title, new_title);
	if (!SetConsoleTitle(new_title))
		die("SetConsoleTitle");
	atexit(restore_title);
}
static void string_to_tchar_array(const char *s, TCHAR t[]) {
	while ((*t++ = *s++));
}
static void save_current_title(void) {
	if (!GetConsoleTitle(old_title, MAX_PATH))
		die("GetConsoleTitle");
}
static void restore_title(void) {
	if (!SetConsoleTitle(old_title))
		die("SetConsoleTitle");
}
void disable_raw_mode(void) {
	if (!SetConsoleMode(hIn, mode))
		die("SetConsoleMode");
}
void enable_raw_mode(void) {
	atexit(disable_raw_mode);
	if (!GetConsoleMode(hIn, &mode))
		die("GetConsoleMode");
	if (!(SetConsoleMode(hIn, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT))))
		die("SetConsoleMode");
}
void get_buffer_size(size_t *x, size_t *y) {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hOut, &csbi))
		die("GetConsoleScreenBufferInfo");
	if (x)
		*x = csbi.dwSize.X;
	if (y)
		*y = csbi.dwSize.Y;
}
void switch_to_normal_buffer(void) {
	if (vt_alternate) {
		static const char seq[] = "\x1b[?1049l";
		write_console((const unsigned char *)seq, sizeof(seq) - 1);
	} else {
		if (!SetConsoleActiveScreenBuffer(old_hOut))
			die("SetConsoleActiveScreenBuffer");
	}
}
void switch_to_alternate_buffer(void) {
	if (vt_alternate) {
		static const char seq[] = "\x1b[?1049h";
		write_console((const unsigned char *)seq, sizeof(seq) - 1);
		atexit(switch_to_normal_buffer);
	} else {
		old_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		hOut = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
		if (old_hOut == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE)
			die("CreateConsoleScreenBuffer");
		if (!SetConsoleActiveScreenBuffer(hOut))
			die("SetConsoleActiveScreenBuffer");
		atexit(switch_to_normal_buffer);
	}
}
void hide_cursor(void) {
	CONSOLE_CURSOR_INFO cursorInfo;
	if (!GetConsoleCursorInfo(hOut, &cursorInfo))
		die("GetConsoleCursorInfo");
	cursorInfo.bVisible = 0;
	if (!SetConsoleCursorInfo(hOut, &cursorInfo))
		die("SetConsoleCursorInfo");
}
void show_cursor(void) {
	CONSOLE_CURSOR_INFO cursorInfo;
	if (!GetConsoleCursorInfo(hOut, &cursorInfo))
		die("GetConsoleCursorInfo");
	cursorInfo.bVisible = 1;
	if (!SetConsoleCursorInfo(hOut, &cursorInfo))
		die("SetConsoleCursorInfo");
}
void set_cursor_position(size_t x, size_t y) {
	COORD coord;
	coord.X = (SHORT)x;
	coord.Y = (SHORT)y;
	if (!SetConsoleCursorPosition(hOut, coord))
		die("SetConsoleCursorPosition");
}
void get_cursor_position(size_t *x, size_t *y) {
	CONSOLE_SCREEN_BUFFER_INFO cbsi;
	if (!GetConsoleScreenBufferInfo(hOut, &cbsi))
		die("GetConsoleScreenBufferInfo");
	if (x)
		*x = cbsi.dwCursorPosition.X;
	if (y)
		*y = cbsi.dwCursorPosition.Y;
}
static void disable_window_resizing(void) {
	long dwStyle;
	HWND hwnd = get_console_hwnd();
	if (!(dwStyle = GetWindowLong(hwnd, GWL_STYLE)))
		die("GetWindowLong");
	dwStyle ^= WS_THICKFRAME;
	if (!SetWindowLong(hwnd, GWL_STYLE, dwStyle))
		die("SetWindowLong");
}
static HWND get_console_hwnd(void) {
#ifdef _WIN32
	HWND hwnd = GetConsoleWindow();
	if (!hwnd)
		die("GetConsoleWindow");
	return hwnd;
#else
	return NULL;
#endif
}
bool is_file_exist(char *filename) {
	DWORD dwAttrib = GetFileAttributes(filename);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}