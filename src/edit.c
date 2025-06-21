#include "yoc.h"
#include <ctype.h>
#include <string.h>
#define PREFERRED_COL_UNSET ((size_t)-1)
static size_t desired_rx = PREFERRED_COL_UNSET;
static void maybe_reset_modified(void);
static void delete_char(void);
static void delete_empty_line(void);
static void break_line(void);
static void join_with_prev_line(void);
static bool_t is_blank(Line *line);
void edit_move_home(void) {
	size_t pos = find_first_nonblank(editor.file.buffer.curr->s);
	size_t fn_x = index_to_mbnum(editor.file.buffer.curr->s, pos);
	if (pos < editor.file.buffer.curr->len && editor.file.cursor.x != fn_x)
		editor.file.cursor.x = fn_x;
	else
		editor.file.cursor.x = 0;
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_end(void) {
	editor.file.cursor.x = line_get_mblen(editor.file.buffer.curr);
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_up(void) {
	if (editor.file.cursor.y > 0) {
		bool_t start_of_line = (editor.file.cursor.x == 0);
		if (desired_rx == PREFERRED_COL_UNSET)
			desired_rx = editor.file.cursor.rx;
		editor.file.buffer.curr = editor.file.buffer.curr->prev;
		--editor.file.cursor.y;
		editor.file.cursor.x = rx_to_x(editor.file.buffer.curr, desired_rx);
		edit_fix_cursor_x();
		if (start_of_line) {
			size_t fn = find_first_nonblank(editor.file.buffer.curr->s);
			size_t fn_x = index_to_mbnum(editor.file.buffer.curr->s, fn);
			if (fn_x > 0)
				editor.file.cursor.x = fn_x;
			editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
			desired_rx = editor.file.cursor.rx;
		}
	}
}
void edit_move_down(void) {
	if (editor.file.cursor.y < editor.file.buffer.num_lines - 1) {
		bool_t start_of_line = (editor.file.cursor.x == 0);
		if (desired_rx == PREFERRED_COL_UNSET)
			desired_rx = editor.file.cursor.rx;
		editor.file.buffer.curr = editor.file.buffer.curr->next;
		++editor.file.cursor.y;
		editor.file.cursor.x = rx_to_x(editor.file.buffer.curr, desired_rx);
		edit_fix_cursor_x();
		if (start_of_line) {
			size_t fn = find_first_nonblank(editor.file.buffer.curr->s);
			size_t fn_x = index_to_mbnum(editor.file.buffer.curr->s, fn);
			if (fn_x > 0)
				editor.file.cursor.x = fn_x;
			editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
			desired_rx = editor.file.cursor.rx;
		}
	}
}
void edit_move_left(void) {
	if (editor.file.cursor.x > 0)
		--editor.file.cursor.x;
	else if (editor.file.cursor.y > 0) {
		edit_move_up();
		edit_move_end();
	}
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_right(void) {
	if (editor.file.cursor.x < line_get_mblen(editor.file.buffer.curr))
		++editor.file.cursor.x;
	else if (editor.file.buffer.curr->next) {
		editor.file.buffer.curr = editor.file.buffer.curr->next;
		++editor.file.cursor.y;
		editor.file.cursor.x = 0;
	}
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_prev_word(void) {
	size_t pos = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
	bool_t seen_a_word = FALSE;
	bool_t step_forward = FALSE;
	for (;;) {
		if (pos == 0) {
			if (editor.file.buffer.curr->prev == NULL) break;
			editor.file.buffer.curr = editor.file.buffer.curr->prev;
			pos = editor.file.buffer.curr->len;
			--editor.file.cursor.y;
		}
		pos = move_mbleft(editor.file.buffer.curr->s, pos);
		if (is_alnum_mbchar(editor.file.buffer.curr->s + pos)) {
			seen_a_word = TRUE;
			if (pos == 0) break;
		} else if (seen_a_word) {
			step_forward = TRUE;
			break;
		}
	}
	if (step_forward)
		pos = move_mbright(editor.file.buffer.curr->s, pos);
	editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, pos);
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
	render_scroll();
}
void edit_move_next_word(void) {
	size_t pos = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
	bool_t started_on_word = is_alnum_mbchar(editor.file.buffer.curr->s + pos);
	bool_t seen_space = !started_on_word;
	for (;;) {
		if (editor.file.buffer.curr->s[pos] == '\0') {
			if (editor.file.buffer.curr->next == NULL) break;
			editor.file.buffer.curr = editor.file.buffer.curr->next;
			++editor.file.cursor.y;
			seen_space = TRUE;
			pos = 0;
		} else
			pos = move_mbright(editor.file.buffer.curr->s, pos);
		if (!is_alnum_mbchar(editor.file.buffer.curr->s + pos))
			seen_space = TRUE;
		else if (seen_space)
			break;
	}
	editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, pos);
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
	render_scroll();
}
void edit_move_pgup(void) {
	size_t y = editor.rows;
	while (y--) edit_move_up();
	edit_fix_cursor_x();
	render_scroll();
	desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_pgdown(void) {
	size_t y = editor.rows;
	while (y-- && editor.file.cursor.y < editor.file.buffer.num_lines - 1)
		edit_move_down();
	edit_fix_cursor_x();
	render_scroll();
	desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_top(void) {
	editor.file.cursor.x = 0;
	editor.file.cursor.y = 0;
	editor.file.buffer.curr = editor.file.buffer.begin;
	desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_bottom(void) {
	while (editor.file.buffer.curr->next) {
		++editor.file.cursor.y;
		editor.file.buffer.curr = editor.file.buffer.curr->next;
	}
	edit_move_end();
	desired_rx = PREFERRED_COL_UNSET;
}
void edit_fix_cursor_x(void) {
	size_t len = line_get_mblen(editor.file.buffer.curr);
	if (editor.file.cursor.x > len)
		editor.file.cursor.x = len;
}
void edit_insert_n(const unsigned char *s, size_t s_len) {
	if (s_len == 0) return;
	line_insert_strn(
		editor.file.buffer.curr,
		mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x),
		s,
		s_len
	);
	editor.file.is_modified = TRUE;
	maybe_reset_modified();
	editor.file.cursor.x += index_to_mbnum(s, s_len);
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_insert(const unsigned char *s) {
	edit_insert_n(s, strlen((const char *)s));
}
void edit_backspace(void) {
	bool_t changed = FALSE;
	if (editor.file.cursor.x > 0 && editor.file.buffer.curr->len > 0) {
		delete_char();
		changed = TRUE;
	} else if (editor.file.cursor.x == 0 && editor.file.buffer.curr->len == 0 && editor.file.buffer.curr->prev != NULL) {
		delete_empty_line();
		changed = TRUE;
	} else if (editor.file.cursor.x == 0 && editor.file.buffer.curr->len > 0 && editor.file.buffer.curr->prev != NULL) {
		join_with_prev_line();
		changed = TRUE;
	}
	if (changed)
		editor.file.is_modified = TRUE;
	maybe_reset_modified();
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_enter(void) {
	Line *prev_line = editor.file.buffer.curr;
	size_t indent_end;
	size_t cursor_pos_bytes;
	line_new(prev_line, prev_line->next);
	editor.file.buffer.num_lines++;
	if (editor.file.cursor.x < line_get_mblen(prev_line))
		break_line();
	editor.file.buffer.curr = prev_line->next;
	++editor.file.cursor.y;
	indent_end = find_first_nonblank(prev_line->s);
	cursor_pos_bytes = mbnum_to_index(prev_line->s, editor.file.cursor.x);
	if (cursor_pos_bytes < indent_end)
		indent_end = cursor_pos_bytes;
	if (indent_end > 0) {
		unsigned char *indent = (unsigned char *)xmalloc(indent_end + 1);
		memcpy(indent, prev_line->s, indent_end);
		indent[indent_end] = '\0';
		line_insert_strn(editor.file.buffer.curr, 0, indent, indent_end);
		free(indent);
	}
	editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, indent_end);
	editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
	editor.file.is_modified = TRUE;
	maybe_reset_modified();
	render_scroll();
}
static void delete_char(void) {
	size_t start;
	size_t char_len;
	if (editor.file.cursor.x == 0)
		return;
	start = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x - 1);
	char_len = utf8_len(editor.file.buffer.curr->s[start]);
	if (char_len == 0 || start + char_len > editor.file.buffer.curr->len)
		char_len = 1;
	line_del_str(editor.file.buffer.curr, start, char_len);
	--editor.file.cursor.x;
	edit_fix_cursor_x();
}
static void delete_empty_line(void) {
	editor.file.buffer.curr = editor.file.buffer.curr->prev;
	editor.file.cursor.x = line_get_mblen(editor.file.buffer.curr);
	--editor.file.cursor.y;
	buf_del_line(&editor.file.buffer, editor.file.buffer.curr->next);
}
static void break_line(void) {
	size_t at = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
	size_t len_to_move = editor.file.buffer.curr->len - at;
	unsigned char *line_content = (unsigned char *)xmalloc(len_to_move + 1);
	memmove(line_content, editor.file.buffer.curr->s + at, len_to_move + 1);
	line_insert_strn(editor.file.buffer.curr->next, 0, line_content, len_to_move);
	line_del_str(editor.file.buffer.curr, at, editor.file.buffer.curr->len - at);
	free(line_content);
}
static void join_with_prev_line(void) {
	Line *current_line = editor.file.buffer.curr;
	Line *prev_line = current_line->prev;
	editor.file.cursor.x = line_get_mblen(prev_line);
	line_insert_strn(prev_line, prev_line->len, current_line->s, current_line->len);
	editor.file.buffer.curr = prev_line;
	--editor.file.cursor.y;
	buf_del_line(&editor.file.buffer, current_line);
}
void edit_process_key(void) {
	int special_key;
	unsigned char *s;
	size_t len = term_read(&s, &special_key);
	if (len != 0) {
		edit_insert_n(s, len);
	} else {
		switch (special_key) {
		case BACKSPACE: edit_backspace(); break;
		case TAB: edit_insert_n((unsigned char*)"\t", 1); break;
		case ENTER: edit_enter(); break;
		case HOME: edit_move_home(); break;
		case END: edit_move_end(); break;
		case PAGE_UP: edit_move_pgup(); break;
		case PAGE_DOWN: edit_move_pgdown(); break;
		case ARROW_LEFT: edit_move_left(); break;
		case ARROW_UP: edit_move_up(); break;
		case ARROW_RIGHT: edit_move_right(); break;
		case ARROW_DOWN: edit_move_down(); break;
		case CTRL_HOME: edit_move_top(); break;
		case CTRL_END: edit_move_bottom(); break;
		case CTRL_ARROW_LEFT: edit_move_prev_word(); break;
		case CTRL_ARROW_RIGHT: edit_move_next_word(); break;
		case CTRL_ARROW_UP: edit_move_prev_para(); break;
		case CTRL_ARROW_DOWN: edit_move_next_para(); break;
		case CTRL_KEY('s'): file_save_prompt(); break;
		case CTRL_KEY('q'): file_quit_prompt(); break;
		case CTRL_KEY('r'): show_line_numbers = !show_line_numbers; break;
		}
	}
	render_scroll();
	maybe_reset_modified();
}
static void maybe_reset_modified(void) {
	if (editor.file.buffer.num_lines == 1 && editor.file.buffer.begin->len == 0) {
		if (editor.file.path[0] == '\0' || !fs_exists(editor.file.path)) {
			editor.file.is_modified = FALSE;
		}
	}
}
static bool_t is_blank(Line *line) {
	return line->len == 0;
}
void edit_move_prev_para(void) {
	Line *line;
	size_t y;
	if (editor.file.cursor.y == 0)
		return;
	line = editor.file.buffer.curr;
	y = editor.file.cursor.y;
	while (y > 0 && is_blank(line)) {
		line = line->prev;
		--y;
	}
	while (y > 0 && line->prev && !is_blank(line->prev)) {
		line = line->prev;
		--y;
	}
	editor.file.buffer.curr = line;
	editor.file.cursor.y = y;
	edit_move_home();
	desired_rx = PREFERRED_COL_UNSET;
	render_scroll();
}
void edit_move_next_para(void) {
	Line *line;
	size_t y;
	if (editor.file.cursor.y >= editor.file.buffer.num_lines - 1)
		return;
	line = editor.file.buffer.curr;
	y = editor.file.cursor.y;
	while (y < editor.file.buffer.num_lines - 1 && !is_blank(line)) {
		line = line->next;
		++y;
	}
	while (y < editor.file.buffer.num_lines - 1 && is_blank(line)) {
		line = line->next;
		++y;
	}
	editor.file.buffer.curr = line;
	editor.file.cursor.y = y;
	edit_move_home();
	desired_rx = PREFERRED_COL_UNSET;
	render_scroll();
}
