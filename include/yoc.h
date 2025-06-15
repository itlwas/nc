#ifndef YOC_H
#define YOC_H
#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <sys/types.h> /* for ssize_t */
#endif
#ifdef _WIN32
#include <windows.h>
extern HANDLE hIn;
extern HANDLE hOut;
#endif
typedef struct line Line;
#ifdef _WIN32
typedef intptr_t ssize_t;
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif
typedef struct {
	size_t x;
	size_t y;
	size_t rx;
} Cursor;
typedef struct {
	Line *curr;
	Line *begin;
	size_t num_lines;
} Buffer;
enum S_Mode { NORMAL, INPUT_MODE, MESSAGE };
extern enum S_Mode status_mode;
typedef struct {
	char *msg;
	size_t len;
	size_t cap;
} Status;
typedef struct {
	size_t cx;
	char *msg;
	Line *input;
	size_t charsoff;
} StatusInput;
typedef struct {
	size_t cap;
	char *path;
	Buffer buffer;
	Cursor cursor;
	Status status;
	bool is_modified;
} File;
#define BUFF_SIZE 128
struct line {
	unsigned char *s;
	size_t len;
	size_t cap;
	size_t width;
	struct line *prev;
	struct line *next;
};
#define LINE_WIDTH_UNCACHED ((size_t)-1)
#define SCREEN_ROWS(x) (((x) <= 1) ? 1 : ((x) - 1))
#define LINE_ROWS(l, x) (((l) / (x)) + (((l) % (x) != 0) ? 1 : 0))
typedef Cursor Window;
struct yoc_t {
	File file;
	size_t tabsize;
	Window window;
	Line *top_line;
	size_t rows;
	size_t cols;
	char **screen_lines;
	size_t screen_rows;
	size_t screen_cols;
};
extern struct yoc_t yoc;
extern bool show_line_numbers;
#define UTF8_CONTINUATION_BYTE 0
#define MAXCHARLEN 6
#define F_KEY(x) ((x) + 0x6f)
#define CTRL_KEY(k) ((k) & 0x1f)
enum key {
	BACKSPACE = 8, TAB, ENTER = 13, ESC = 27,
	PAGE_UP = 33, PAGE_DOWN, END, HOME,
	ARROW_LEFT, ARROW_UP, ARROW_RIGHT, ARROW_DOWN,
	SELECT, PRINT, EXECUTE, PRINT_SCREEN, INSERT, DEL,
	CTRL_END, CTRL_HOME, CTRL_ARROW_LEFT, CTRL_ARROW_UP, CTRL_ARROW_RIGHT, CTRL_ARROW_DOWN
};
void buffer_init(Buffer *buffer);
void buffer_free(Buffer *buffer);
size_t utf8_len(unsigned char c);
bool is_alnum_mbchar(const unsigned char *s);
size_t move_mbleft(const unsigned char *s, size_t pos);
size_t move_mbright(const unsigned char *s, size_t pos);
size_t index_to_mbnum(const unsigned char *s, size_t n);
size_t mbnum_to_index(const unsigned char *s, size_t n);
void die(const char *msg);
void init(int argc, char **argv);
void refresh_screen(void);
void scroll_buffer(void);
void process_keypress(void);
void file_init(File *file);
void file_load(File *file);
void file_save(File *file);
void file_free(File *file);
void write_console(const unsigned char *s, size_t len);
void writeln_console(const unsigned char *s, size_t len);
size_t read_console(unsigned char **s, int *special_key);
Line *line_insert(Line *prev, Line *next);
void line_delete(Line *line);
void line_insert_str(Line *line, size_t at, const unsigned char *str);
void line_delete_str(Line *line, size_t at, size_t len);
void line_insert_char(Line *line, size_t at, unsigned char c);
void line_delete_char(Line *line, size_t at);
void line_free(Line *line);
size_t line_width(Line *line);
size_t line_mblen(Line *line);
void do_home(void);
void do_end(void);
void do_up(void);
void do_down(void);
void do_left(void);
void do_right(void);
void do_prev_word(void);
void do_next_word(void);
void do_page_up(void);
void do_page_down(void);
void do_top(void);
void do_bottom(void);
void status_init(void);
void status_free(void);
void status(const char *msg);
void status_print(size_t cols);
bool status_input(Line *input, char *msg, const char *placeholder);
void term_init(void);
void get_window_size(size_t *x, size_t *y);
void clear_line(void);
void clear_screen(void);
void set_title(const char *title);
void enable_raw_mode(void);
void disable_raw_mode(void);
void get_buffer_size(size_t *x, size_t *y);
void switch_to_normal_buffer(void);
void switch_to_alternate_buffer(void);
void hide_cursor(void);
void show_cursor(void);
void set_cursor_position(size_t x, size_t y);
void get_cursor_position(size_t *x, size_t *y);
void insert(const unsigned char *s);
void do_backspace(void);
void do_enter(void);
void do_tab(void);
bool do_save(void);
void do_quit(void);
void fix_cursor_x(void);
size_t get_tabsize(void);
bool is_file_exist(char *filename);
bool is_continuation_byte(unsigned char c);
size_t find_first_nonblank(const unsigned char *s);
size_t cursor_x_to_rx(Line *line, size_t x);
size_t str_width(const unsigned char *s, size_t len);
size_t length_to_width(const unsigned char *s, size_t len);
size_t width_to_length(const unsigned char *s, size_t width);
size_t rx_to_cursor_x(Line *line, size_t rx);
#endif