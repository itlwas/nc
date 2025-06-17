#ifndef YOC_H
#define YOC_H
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <termios.h>
#endif
#define YOC_VERSION "0.0.1"
#define TRUE 1
#define FALSE 0
#define BUFF_SIZE 128
#define MAXCHARLEN 6
#define VSCROLL_MARGIN 3
#define HSCROLL_MARGIN 5
#define LINE_WIDTH_UNCACHED ((size_t)-1)
#define SCREEN_ROWS(x) (((x) <= 1) ? 1 : ((x) - 1))
#define LINE_MBLEN_UNCACHED ((size_t)-1)
typedef int bool_t;
typedef struct line_t Line;
struct line_t {
	unsigned char *s;
	size_t len;
	size_t cap;
	size_t width;
	size_t mb_len;
	Line *prev;
	Line *next;
};
typedef struct {
	size_t x;
	size_t y;
	size_t rx;
} Pos;
typedef struct {
	Line *curr;
	Line *begin;
	size_t num_lines;
} Buffer;
typedef struct {
	char *msg;
	size_t len;
	size_t cap;
} Status;
enum S_Mode { NORMAL, INPUT_MODE, MESSAGE };
extern enum S_Mode status_mode;
typedef struct {
	size_t cap;
	char *path;
	Buffer buffer;
	Pos cursor;
	Status status;
	bool_t is_modified;
} File;
typedef struct {
	File file;
	size_t tabsize;
	Pos window;
	Line *top_line;
	size_t rows;
	size_t cols;
	char **screen_lines;
	size_t *screen_lens;
	size_t screen_rows;
	size_t screen_cols;
} Editor;
extern Editor editor;
extern bool_t show_line_numbers;
#ifdef _WIN32
extern HANDLE hIn;
extern HANDLE hOut;
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif
enum key {
	BACKSPACE = 8, TAB, ENTER = 13, ESC = 27,
	PAGE_UP = 33, PAGE_DOWN, END, HOME,
	ARROW_LEFT, ARROW_UP, ARROW_RIGHT, ARROW_DOWN,
	DEL = 46,
	CTRL_END = 100, CTRL_HOME, CTRL_ARROW_LEFT, CTRL_ARROW_UP, CTRL_ARROW_RIGHT, CTRL_ARROW_DOWN
};
#define CTRL_KEY(k) ((k) & 0x1f)
void die(const char *msg);
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
size_t utf8_len(unsigned char c);
bool_t is_continuation_byte(unsigned char c);
size_t move_mbleft(const unsigned char *s, size_t pos);
size_t move_mbright(const unsigned char *s, size_t pos);
size_t index_to_mbnum(const unsigned char *s, size_t n);
size_t mbnum_to_index(const unsigned char *s, size_t n);
bool_t is_alnum_mbchar(const unsigned char *s);
size_t char_display_width(const unsigned char *s);
size_t str_width(const unsigned char *s, size_t len);
size_t length_to_width(const unsigned char *s, size_t len);
size_t width_to_length(const unsigned char *s, size_t width);
size_t rx_to_cursor_x(Line *line, size_t rx_target);
size_t cursor_x_to_rx(Line *line, size_t x);
size_t find_first_nonblank(const unsigned char *s);
void buf_init(Buffer *buffer);
void buf_free(Buffer *buffer);
void buf_delete_line(Buffer *buffer, Line *line);
Line *line_insert(Line *prev, Line *next);
void line_delete(Line *line);
void line_free(Line *line);
void line_insert_char(Line *line, size_t at, unsigned char c);
void line_delete_char(Line *line, size_t at);
void line_insert_str(Line *line, size_t at, const unsigned char *str);
void line_delete_str(Line *line, size_t at, size_t len);
size_t line_width(Line *line);
size_t line_mblen(Line *line);
void render_refresh(void);
void render_scroll(void);
void render_free(void);
void status_init(void);
void status_free(void);
void status_msg(const char *msg);
void status_print(void);
bool_t status_input(Line *input, char *msg, const char *placeholder);
void edit_process_key(void);
void edit_insert(const unsigned char *s);
void edit_enter(void);
void edit_backspace(void);
void edit_move_home(void);
void edit_move_end(void);
void edit_move_up(void);
void edit_move_down(void);
void edit_move_left(void);
void edit_move_right(void);
void edit_move_prev_word(void);
void edit_move_next_word(void);
void edit_move_page_up(void);
void edit_move_page_down(void);
void edit_move_top(void);
void edit_move_bottom(void);
void edit_fix_cursor_x(void);
void file_init(File *file);
void file_free(File *file);
void file_load(File *file);
void file_save(File *file);
bool_t file_save_prompt(void);
void file_quit_prompt(void);
bool_t is_file_exist(char *filename);
void term_init(void);
void term_get_win_size(size_t *x, size_t *y);
void term_clear_line(void);
void term_clear_screen(void);
void term_set_title(const char *title);
void term_enable_raw(void);
void term_disable_raw(void);
void term_switch_to_alt(void);
void term_switch_to_norm(void);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_set_cursor(size_t x, size_t y);
void term_write(const unsigned char *s, size_t len);
size_t term_read(unsigned char **s, int *special_key);
#endif
