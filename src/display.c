#include "yoc.h"
static void status_input_print(void);
static bool status_process_input(void);
static void status_input_init(void);
static void status_input_free(void);
static void status_realloc(size_t len);
static void status_set_default(size_t cols);
static void status_do_insert(unsigned char *s);
static void status_do_end(void);
static void status_do_home(void);
static void status_do_backspace(void);
static void status_do_arrow_left(void);
static void status_do_arrow_right(void);
static void display_buffer(void);
static void display_rows(void);
static void display_status_line(void);
static void ensure_screen_buffer(void);
static void draw_line_if_changed(size_t row, const unsigned char *line, size_t len);
static StatusInput *statin;
enum S_Mode status_mode;
static char *rowbuf = NULL;
static size_t rowbuf_cap = 0;
static char *spaces = NULL;
static size_t spaces_cap = 0;
static size_t lineno_pad = 0;
bool show_line_numbers = true;
void status_init(void) {
	if (!(yoc.file.status.msg = (char *)malloc(BUFF_SIZE))) die("malloc");
	yoc.file.status.msg[0] = '\0';
	yoc.file.status.cap = BUFF_SIZE;
	yoc.file.status.len = 0;
	status_mode = NORMAL;
}
void status_free(void) {
	free(yoc.file.status.msg);
	yoc.file.status.cap = 0;
	yoc.file.status.len = 0;
}
void status(const char *msg) {
	size_t len = strlen(msg);
	size_t required = len + 1;
	status_realloc(required);
	memcpy(yoc.file.status.msg, msg, len);
	yoc.file.status.msg[len] = '\0';
	yoc.file.status.len = len;
	status_mode = MESSAGE;
}
void status_print(size_t cols) {
	switch (status_mode) {
		case NORMAL:
			status_set_default(cols);
			/* fall through */
		case MESSAGE: {
			clear_line();
			static const unsigned char rev_on[] = "\x1b[7m";
			static const unsigned char rev_off[] = "\x1b[0m";
			write_console(rev_on, sizeof(rev_on) - 1);
			size_t mlen = yoc.file.status.len;
			if (mlen > cols) mlen = cols;
			write_console((unsigned char *)yoc.file.status.msg, mlen);
			write_console(rev_off, sizeof(rev_off) - 1);
			status_mode = NORMAL;
			break;
		}
		case INPUT_MODE:
			status_input_print();
			break;
	}
}
bool status_input(Line *input, char *msg, const char *placeholder) {
	status_input_init();
	statin->msg = msg;
	statin->input = input;
	if (placeholder) {
		line_insert_str(statin->input, 0, (unsigned char *)placeholder);
		statin->cx += strlen(placeholder);
	}
	return status_process_input();
}
static bool status_process_input(void) {
	int special_key;
	unsigned char *s;
	status_mode = INPUT_MODE;
	for (;;) {
		status_input_print();
		size_t len = read_console(&s, &special_key);
		if (len != 0) {
			status_do_insert(s);
		} else {
			switch (special_key) {
				case ESC:
				case ENTER:
					status_input_free();
					status_mode = NORMAL;
					return (special_key == ESC) ? false : true;
				case HOME:
					status_do_home();
					break;
				case END:
					status_do_end();
					break;
				case ARROW_LEFT:
					status_do_arrow_left();
					break;
				case ARROW_RIGHT:
					status_do_arrow_right();
					break;
				case BACKSPACE:
					status_do_backspace();
					break;
			}
		}
	}
}
static void status_input_print(void) {
	Window window;
	get_window_size(&window.x, &window.y);
	char *msg = statin->msg;
	size_t msglen = strlen(statin->msg);
	if (msglen >= window.x - 2) {
		msg = ": ";
		msglen = 2;
	}
	size_t free_space = window.x - msglen - 1;
	if (statin->cx < statin->charsoff)
		statin->charsoff = statin->cx;
	else if (statin->cx - statin->charsoff > free_space)
		statin->charsoff = statin->cx - free_space;
	size_t len;
	size_t input_width = line_width(statin->input);
	size_t start = mbnum_to_index(statin->input->s, statin->charsoff);
	if (input_width - statin->charsoff > free_space)
		len = width_to_length(statin->input->s + start, free_space);
	else
		len = statin->input->len - start;
	set_cursor_position(0, --window.y);
	clear_line();
	write_console((unsigned char *)msg, msglen);
	write_console(statin->input->s + start, len);
	set_cursor_position(statin->cx + msglen - statin->charsoff, window.y);
}
static void status_realloc(size_t len) {
	if (len >= yoc.file.status.cap) {
		size_t new_cap = yoc.file.status.cap;
		while (len >= new_cap)
			new_cap = new_cap == 0 ? BUFF_SIZE : new_cap * 2;
		yoc.file.status.msg = (char *)realloc(yoc.file.status.msg, new_cap);
		if (yoc.file.status.msg == NULL)
			die("realloc");
		yoc.file.status.cap = new_cap;
	}
}
static void status_set_default(size_t cols) {
	status_realloc(cols + 1);
	char *path = yoc.file.path[0] ? yoc.file.path : "[No Name]";
	int left_len;
	if (yoc.file.is_modified && yoc.file.path[0] != '\0')
		left_len = snprintf(yoc.file.status.msg, yoc.file.status.cap, "%s [+]", path);
	else
		left_len = snprintf(yoc.file.status.msg, yoc.file.status.cap, "%s", path);
	char right_str[32];
	int right_len = snprintf(right_str, sizeof(right_str), "%zu:%zu", yoc.file.cursor.y + 1, yoc.file.cursor.rx + 1);
	if ((size_t)(left_len + right_len + 1) > cols) {
		left_len = cols - right_len - 1;
		if (left_len < 0) left_len = 0;
	}
	yoc.file.status.msg[left_len] = '\0';
	int fill_len = cols - left_len - right_len;
	if (fill_len < 0) fill_len = 0;
	memset(yoc.file.status.msg + left_len, ' ', fill_len);
	memcpy(yoc.file.status.msg + left_len + fill_len, right_str, (size_t)right_len + 1);
	yoc.file.status.len = left_len + fill_len + right_len;
}
static void status_do_insert(unsigned char *s) {
	line_insert_str(statin->input, mbnum_to_index(statin->input->s, statin->cx++), s);
}
static void status_do_home(void) {
	statin->cx = 0;
}
static void status_do_end(void) {
	statin->cx = line_mblen(statin->input);
}
static void status_do_arrow_left(void) {
	if (statin->cx > 0)
		--statin->cx;
}
static void status_do_arrow_right(void) {
	if (statin->cx < line_mblen(statin->input))
		++statin->cx;
}
static void status_do_backspace(void) {
	if (statin->input->len > 0 && statin->cx > 0) {
		size_t charlen = 1;
		size_t i = mbnum_to_index(statin->input->s, statin->cx);
		while (is_continuation_byte(statin->input->s[--i]))
			++charlen;
		line_delete_str(statin->input, i, charlen);
		--statin->cx;
		if (statin->charsoff > 0)
			--statin->charsoff;
	}
}
static void status_input_init(void) {
	statin = (StatusInput *)malloc(sizeof(StatusInput));
	statin->cx = 0;
	statin->charsoff = 0;
}
static void status_input_free(void) {
	free(statin);
}
void refresh_screen(void) {
	size_t cols = 0, rows = 0;
	get_window_size(&cols, &rows);
	yoc.cols = cols;
	yoc.rows = SCREEN_ROWS(rows);
	size_t digits = 1;
	for (size_t n = yoc.file.buffer.num_lines; n >= 10; n /= 10) ++digits;
	lineno_pad = show_line_numbers ? digits + 2 : 0;
	ensure_screen_buffer();
	if (yoc.window.y > yoc.file.cursor.y)
		yoc.window.y = yoc.file.cursor.y;
	if (yoc.window.x > yoc.file.cursor.rx)
		yoc.window.x = yoc.file.cursor.rx;
	scroll_buffer();
	display_buffer();
	display_status_line();
	set_cursor_position((show_line_numbers ? lineno_pad : 0) + yoc.file.cursor.rx - yoc.window.x, yoc.file.cursor.y - yoc.window.y);
	show_cursor();
}
void scroll_buffer(void) {
	yoc.file.cursor.rx = 0;
	if (yoc.file.cursor.y < yoc.file.buffer.num_lines)
		yoc.file.cursor.rx = cursor_x_to_rx(yoc.file.buffer.curr, yoc.file.cursor.x);
	if (yoc.file.cursor.y < yoc.window.y + VSCROLL_MARGIN) {
		yoc.window.y = (yoc.file.cursor.y < VSCROLL_MARGIN) ? 0 : yoc.file.cursor.y - VSCROLL_MARGIN;
	} else if (yoc.file.cursor.y >= yoc.window.y + yoc.rows - VSCROLL_MARGIN) {
		yoc.window.y = yoc.file.cursor.y - yoc.rows + VSCROLL_MARGIN + 1;
	}
	size_t text_cols = yoc.cols - (show_line_numbers ? lineno_pad : 0);
	if (yoc.file.cursor.rx < yoc.window.x + HSCROLL_MARGIN) {
		yoc.window.x = (yoc.file.cursor.rx < HSCROLL_MARGIN) ? 0 : yoc.file.cursor.rx - HSCROLL_MARGIN;
	} else if (yoc.file.cursor.rx >= yoc.window.x + text_cols - HSCROLL_MARGIN) {
		yoc.window.x = yoc.file.cursor.rx - text_cols + HSCROLL_MARGIN + 1;
	}
	Line *line = yoc.file.buffer.curr;
	size_t tmp_row = yoc.file.cursor.y;
	while (tmp_row > yoc.window.y) {
		line = line->prev;
		--tmp_row;
	}
	while (tmp_row < yoc.window.y) {
		line = line->next;
		++tmp_row;
	}
	yoc.top_line = line;
}
static void display_buffer(void) {
	hide_cursor();
	set_cursor_position(0, 0);
	display_rows();
}
static void display_rows(void) {
	Line *line = yoc.top_line;
	if (!rowbuf) ensure_screen_buffer();
	size_t digits = lineno_pad ? lineno_pad - 2 : 1;
	for (size_t y = 0; y < yoc.rows; ++y) {
		rowbuf[0] = '\0';
		if (line && show_line_numbers) {
			size_t lnum = yoc.window.y + y + 1;
			snprintf(rowbuf, lineno_pad + 1, " %*zu ", (int)digits, lnum);
		}
		size_t pos = strlen(rowbuf);
		if (!line) {
			rowbuf[0] = '~';
			rowbuf[1] = '\0';
			if (yoc.file.buffer.num_lines == 1 && yoc.file.buffer.begin->len == 0 && y == yoc.rows / 3) {
				char msg[32];
				int welcomelen = snprintf(msg, sizeof(msg), "yoc editor -- version %s", "0.0.1");
				if ((size_t)welcomelen > yoc.cols) welcomelen = (int)yoc.cols;
				size_t padding = (yoc.cols - (size_t)welcomelen) / 2;
				size_t pos = 0;
				if (padding) {
					rowbuf[pos++] = '~';
					--padding;
				}
				while (padding-- && pos < yoc.cols) rowbuf[pos++] = ' ';
				if (pos + (size_t)welcomelen > yoc.cols) welcomelen = (int)(yoc.cols - pos);
				memcpy(rowbuf + pos, msg, (size_t)welcomelen);
				pos += (size_t)welcomelen;
				rowbuf[pos] = '\0';
			}
		} else {
			const unsigned char *s = line->s;
			size_t i = 0, width = 0;
			size_t text_cols = yoc.cols - (show_line_numbers ? lineno_pad : 0);
			while (i < line->len && width < yoc.window.x + text_cols) {
				unsigned char c = s[i];
				size_t char_len = 1;
				if (c == '\t') {
					size_t spaces_to_add = yoc.tabsize - (width % yoc.tabsize);
					for (size_t k = 0; k < spaces_to_add; ++k) {
						if (width >= yoc.window.x && pos < yoc.cols * MAXCHARLEN) rowbuf[pos++] = ' ';
						++width;
					}
					i += 1;
					continue;
				}
				if (!is_continuation_byte(c)) {
					char_len = utf8_len(c);
					if (char_len == UTF8_CONTINUATION_BYTE || i + char_len > line->len) char_len = 1;
					size_t char_width = char_display_width(s + i);
					if (width + char_width > yoc.window.x + text_cols)
						break;
					if (width >= yoc.window.x && pos + char_len <= yoc.cols * MAXCHARLEN) {
						memcpy(rowbuf + pos, s + i, char_len);
						pos += char_len;
					}
					width += char_width;
				}
				i += char_len;
			}
			rowbuf[pos] = '\0';
			line = line->next;
		}
		draw_line_if_changed(y, (unsigned char *)rowbuf, strlen(rowbuf));
	}
}
static void display_status_line(void) {
	set_cursor_position(0, yoc.rows);
	status_print(yoc.cols);
}
static void ensure_screen_buffer(void) {
	const size_t rows_needed = yoc.rows + 1;
	bool size_changed = !(yoc.screen_lines && yoc.screen_rows == rows_needed && yoc.screen_cols == yoc.cols);
	if (!size_changed) return;
	clear_screen();
	if (yoc.screen_lines) {
		for (size_t r = 0; r < yoc.screen_rows; ++r)
			free(yoc.screen_lines[r]);
		free(yoc.screen_lines);
	}
	yoc.screen_lines = (char **)malloc(rows_needed * sizeof(char *));
	if (!yoc.screen_lines) die("malloc");
	for (size_t r = 0; r < rows_needed; ++r) {
		yoc.screen_lines[r] = (char *)malloc((yoc.cols * MAXCHARLEN) + 1);
		if (!yoc.screen_lines[r]) die("malloc");
		yoc.screen_lines[r][0] = '\0';
	}
	yoc.screen_rows = rows_needed;
	yoc.screen_cols = yoc.cols;
	const size_t needed_rowbuf_cap = (yoc.cols * MAXCHARLEN) + 1;
	if (rowbuf_cap < needed_rowbuf_cap) {
		rowbuf = (char *)realloc(rowbuf, needed_rowbuf_cap);
		if (!rowbuf) die("realloc");
		rowbuf_cap = needed_rowbuf_cap;
	}
	if (spaces_cap < yoc.cols + 1) {
		spaces = (char *)realloc(spaces, yoc.cols + 1);
		if (!spaces) die("realloc");
		spaces_cap = yoc.cols + 1;
	}
	memset(spaces, ' ', yoc.cols);
	spaces[yoc.cols] = '\0';
}
static void draw_line_if_changed(size_t row, const unsigned char *line, size_t len) {
	if (row >= yoc.screen_rows) return;
	char *prev = yoc.screen_lines[row];
	size_t prev_len = strlen(prev);
	if (prev_len == len && memcmp(prev, line, len) == 0) return;
	size_t i = 0;
	size_t rx_common = 0;
	while (i < prev_len && i < len) {
		unsigned char c = (unsigned char)prev[i];
		size_t char_len = utf8_len(c);
		if (char_len == UTF8_CONTINUATION_BYTE || i + char_len > prev_len || i + char_len > len) break;
		if (memcmp(prev + i, line + i, char_len) != 0) break;
		if (c == '\t')
			rx_common += yoc.tabsize - (rx_common % yoc.tabsize);
		else
			rx_common += 1;
		i += char_len;
	}
	set_cursor_position(rx_common, row);
	if (i < len)
		write_console(line + i, len - i);
	size_t new_width = str_width(line, len);
	size_t clear_from = (new_width > rx_common) ? new_width : rx_common;
	size_t clear_to = yoc.cols;
	if (clear_from < clear_to)
		write_console((unsigned char *)spaces, clear_to - clear_from);
	if (len > yoc.cols * MAXCHARLEN)
		len = yoc.cols * MAXCHARLEN;
	memcpy(prev, line, len);
	prev[len] = '\0';
}
void display_free(void) {
	if (rowbuf) {
		free(rowbuf);
		rowbuf = NULL;
		rowbuf_cap = 0;
	}
	if (spaces) {
		free(spaces);
		spaces = NULL;
		spaces_cap = 0;
	}
	if (yoc.screen_lines) {
		for (size_t r = 0; r < yoc.screen_rows; ++r)
			free(yoc.screen_lines[r]);
		free(yoc.screen_lines);
		yoc.screen_lines = NULL;
		yoc.screen_rows = 0;
		yoc.screen_cols = 0;
	}
}
