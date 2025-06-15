#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "yoc.h"
#include <stdio.h>
static size_t desired_rx = 0;
void do_home(void) {
	size_t pos = find_first_nonblank(yoc.file.buffer.curr->s);
	if (pos < yoc.file.buffer.curr->len && yoc.file.cursor.x != pos)
		yoc.file.cursor.x = pos;
	else
		yoc.file.cursor.x = 0;
	desired_rx = 0;
}
void do_end(void) {
	yoc.file.cursor.x = line_mblen(yoc.file.buffer.curr);
	desired_rx = yoc.file.cursor.rx = cursor_x_to_rx(yoc.file.buffer.curr, yoc.file.cursor.x);
}
void do_up(void) {
	if (yoc.file.cursor.y > 0) {
		desired_rx = yoc.file.cursor.rx;
		yoc.file.buffer.curr = yoc.file.buffer.curr->prev;
		--yoc.file.cursor.y;
		yoc.file.cursor.x = rx_to_cursor_x(yoc.file.buffer.curr, desired_rx);
		fix_cursor_x();
	}
}
void do_down(void) {
	if (yoc.file.cursor.y < yoc.file.buffer.num_lines - 1) {
		desired_rx = yoc.file.cursor.rx;
		yoc.file.buffer.curr = yoc.file.buffer.curr->next;
		++yoc.file.cursor.y;
		yoc.file.cursor.x = rx_to_cursor_x(yoc.file.buffer.curr, desired_rx);
		fix_cursor_x();
	}
}
void do_left(void) {
	if (yoc.file.cursor.x > 0)
		--yoc.file.cursor.x;
	else if (yoc.file.cursor.y > 0) {
		do_up();
		do_end();
	}
	desired_rx = yoc.file.cursor.rx = cursor_x_to_rx(yoc.file.buffer.curr, yoc.file.cursor.x);
}
void do_right(void) {
	if (yoc.file.cursor.x < line_mblen(yoc.file.buffer.curr))
		++yoc.file.cursor.x;
	else if (yoc.file.buffer.curr->next) {
		do_down();
		do_home();
	}
	desired_rx = yoc.file.cursor.rx = cursor_x_to_rx(yoc.file.buffer.curr, yoc.file.cursor.x);
}
void do_prev_word(void) {
	size_t pos = mbnum_to_index(yoc.file.buffer.curr->s, yoc.file.cursor.x);
	bool seen_a_word = false;
	bool step_forward = false;
	for (;;) {
		if (pos == 0) {
			if (yoc.file.buffer.curr->prev == NULL)
				break;
			yoc.file.buffer.curr = yoc.file.buffer.curr->prev;
			pos = yoc.file.buffer.curr->len;
			--yoc.file.cursor.y;
		}
		pos = move_mbleft(yoc.file.buffer.curr->s, pos);
		if (is_alnum_mbchar(yoc.file.buffer.curr->s + pos)) {
			seen_a_word = true;
			if (pos == 0)
				break;
		} else if (seen_a_word) {
			step_forward = true;
			break;
		}
	}
	if (step_forward)
		pos = move_mbright(yoc.file.buffer.curr->s, pos);
	yoc.file.cursor.x = index_to_mbnum(yoc.file.buffer.curr->s, pos);
	scroll_buffer();
}
void do_next_word(void) {
	size_t pos = mbnum_to_index(yoc.file.buffer.curr->s, yoc.file.cursor.x);
	bool started_on_word = is_alnum_mbchar(yoc.file.buffer.curr->s + pos);
	bool seen_space = !started_on_word;
	for (;;) {
		if (yoc.file.buffer.curr->s[pos] == '\0') {
			if (yoc.file.buffer.curr->next == NULL)
				break;
			yoc.file.buffer.curr = yoc.file.buffer.curr->next;
			++yoc.file.cursor.y;
			seen_space = true;
			pos = 0;
		} else
			pos = move_mbright(yoc.file.buffer.curr->s, pos);
		if (!is_alnum_mbchar(yoc.file.buffer.curr->s + pos))
			seen_space = true;
		else if (seen_space)
			break;
	}
	yoc.file.cursor.x = index_to_mbnum(yoc.file.buffer.curr->s, pos);
	scroll_buffer();
}
void do_page_up(void) {
	size_t y, i;
	get_window_size(NULL, &y);
	for (i = 0, y = yoc.rows; i < y; ++i)
		do_up();
	fix_cursor_x();
	scroll_buffer();
}
void do_page_down(void) {
	size_t y, i;
	yoc.file.cursor.y += yoc.rows;
	if (yoc.file.cursor.y > yoc.file.buffer.num_lines - 1)
		yoc.file.cursor.y = yoc.file.buffer.num_lines - 1;
	for (i = 0, y = yoc.rows; i < y; ++i)
		do_down();
	fix_cursor_x();
	scroll_buffer();
}
void do_top(void) {
	yoc.file.cursor.x = 0;
	yoc.file.cursor.y = 0;
	yoc.file.buffer.curr = yoc.file.buffer.begin;
}
void do_bottom(void) {
	while (yoc.file.buffer.curr->next) {
		++yoc.file.cursor.y;
		yoc.file.buffer.curr = yoc.file.buffer.curr->next;
	}
	do_end();
}
static void delete_character(void);
static void delete_empty_line(void);
static void break_line_into_two(void);
static void concatenate_with_previous_line(void);
static void maybe_reset_modified(void);
static bool buffer_matches_saved_file(void) {
	if (yoc.file.path[0] == '\0')
		return false;
	FILE *f = fopen(yoc.file.path, "r");
	if (!f)
		return false;
	Line *line = yoc.file.buffer.begin;
	char *disk_line = NULL;
	size_t cap = 0;
	ssize_t read;
	while (line) {
		read = getline(&disk_line, &cap, f);
		if (read == -1) {
			free(disk_line);
			fclose(f);
			return false;
		}
		while (read > 0 && (disk_line[read - 1] == '\n' || disk_line[read - 1] == '\r'))
			disk_line[--read] = '\0';
		if (strcmp((char *)line->s, disk_line) != 0) {
			free(disk_line);
			fclose(f);
			return false;
		}
		line = line->next;
	}
	if (getline(&disk_line, &cap, f) != -1) {
		free(disk_line);
		fclose(f);
		return false;
	}
	free(disk_line);
	fclose(f);
	return true;
}
void insert(const unsigned char *s) {
	line_insert_str(
		yoc.file.buffer.curr,
		mbnum_to_index(yoc.file.buffer.curr->s, yoc.file.cursor.x),
		s
	);
	yoc.file.is_modified = true;
	maybe_reset_modified();
	++yoc.file.cursor.x;
}
void do_backspace(void) {
	bool changed = false;
	if (yoc.file.cursor.x > 0 && yoc.file.buffer.curr->len > 0) {
		delete_character();
		changed = true;
	} else if (
		yoc.file.cursor.x == 0 &&
		yoc.file.buffer.curr->len == 0 &&
		yoc.file.buffer.curr->prev != NULL
	) {
		delete_empty_line();
		changed = true;
	} else if (
		yoc.file.cursor.x == 0 &&
		yoc.file.buffer.curr->len > 0 &&
		yoc.file.buffer.curr->prev != NULL
	) {
		concatenate_with_previous_line();
		changed = true;
	}
	if (changed)
		yoc.file.is_modified = true;
	maybe_reset_modified();
}
void do_enter(void) {
	line_insert(yoc.file.buffer.curr, yoc.file.buffer.curr->next);
	yoc.file.buffer.num_lines++;
	if (yoc.file.cursor.x < line_mblen(yoc.file.buffer.curr))
		break_line_into_two();
	yoc.file.buffer.curr = yoc.file.buffer.curr->next;
	yoc.file.cursor.x = 0;
	++yoc.file.cursor.y;
	do_home();
	yoc.file.is_modified = true;
	maybe_reset_modified();
	scroll_buffer();
}
void do_tab(void) {
	insert((unsigned char *)"\t");
}
static void delete_character(void) {
	size_t char_len = 1;
	size_t i = mbnum_to_index(yoc.file.buffer.curr->s, yoc.file.cursor.x);
	while (is_continuation_byte(yoc.file.buffer.curr->s[--i]))
		++char_len;
	line_delete_str(yoc.file.buffer.curr, i, char_len);
	--yoc.file.cursor.x;
	fix_cursor_x();
}
static void delete_empty_line(void) {
	yoc.file.buffer.curr = yoc.file.buffer.curr->prev;
	yoc.file.cursor.x = line_mblen(yoc.file.buffer.curr);
	--yoc.file.cursor.y;
	buffer_delete_line(&yoc.file.buffer, yoc.file.buffer.curr->next);
}
static void break_line_into_two(void) {
	size_t at = mbnum_to_index(yoc.file.buffer.curr->s, yoc.file.cursor.x);
	unsigned char *line = (unsigned char *)malloc(yoc.file.buffer.curr->len - at + 1);
	memmove(line, yoc.file.buffer.curr->s + at, yoc.file.buffer.curr->len - at + 1);
	line_insert_str(yoc.file.buffer.curr->next, 0, line);
	line_delete_str(yoc.file.buffer.curr, at, yoc.file.buffer.curr->len - at);
	free(line);
}
static void concatenate_with_previous_line(void) {
	unsigned char *line = (unsigned char *)malloc(yoc.file.buffer.curr->len + 1);
	memmove(line, yoc.file.buffer.curr->s, yoc.file.buffer.curr->len);
	line[yoc.file.buffer.curr->len] = '\0';
	yoc.file.cursor.x = line_mblen(yoc.file.buffer.curr->prev);
	line_insert_str(
		yoc.file.buffer.curr->prev,
		yoc.file.buffer.curr->prev->len,
		line
	);
	yoc.file.buffer.curr = yoc.file.buffer.curr->prev;
	buffer_delete_line(&yoc.file.buffer, yoc.file.buffer.curr->next);
	--yoc.file.cursor.y;
	free(line);
}
void process_keypress(void) {
	int special_key;
	unsigned char *s;
	size_t len = read_console(&s, &special_key);
	if (len != 0) {
		insert(s);
	} else {
		switch (special_key) {
		case BACKSPACE:
			do_backspace();
			break;
		case TAB:
			do_tab();
			break;
		case ENTER:
			do_enter();
			break;
		case HOME:
			do_home();
			break;
		case END:
			do_end();
			break;
		case PAGE_UP:
			do_page_up();
			break;
		case PAGE_DOWN:
			do_page_down();
			break;
		case ARROW_LEFT:
			do_left();
			break;
		case ARROW_UP:
			do_up();
			break;
		case ARROW_RIGHT:
			do_right();
			break;
		case ARROW_DOWN:
			do_down();
			break;
		case CTRL_HOME:
			do_top();
			break;
		case CTRL_END:
			do_bottom();
			break;
		case CTRL_ARROW_LEFT:
			do_prev_word();
			break;
		case CTRL_ARROW_RIGHT:
			do_next_word();
			break;
		case CTRL_KEY('s'):
			do_save();
			break;
		case CTRL_KEY('q'):
			do_quit();
			break;
		case CTRL_KEY('r'):
			show_line_numbers = !show_line_numbers;
			break;
		}
	}
	free(s);
	scroll_buffer();
	maybe_reset_modified();
}
static void maybe_reset_modified(void) {
	if (yoc.file.buffer.num_lines == 1 && yoc.file.buffer.begin->len == 0) {
		if (yoc.file.path[0] == '\0' || !is_file_exist(yoc.file.path)) {
			yoc.file.is_modified = false;
			return;
		}
	}
	if (yoc.file.is_modified && buffer_matches_saved_file()) {
		yoc.file.is_modified = false;
	}
}