#include "yoc.h"
#include <ctype.h>
#include <string.h>
#define PREFERRED_COL_UNSET ((size_t)-1)
static size_t desired_rx = PREFERRED_COL_UNSET;
static void maybe_reset_modified(void);
static bool_t buffer_matches_saved_file(void);
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
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_end(void) {
	editor.file.cursor.x = line_mblen(editor.file.buffer.curr);
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_up(void) {
	if (editor.file.cursor.y > 0) {
		bool_t start_of_line = (editor.file.cursor.x == 0);
		if (desired_rx == PREFERRED_COL_UNSET)
			desired_rx = editor.file.cursor.rx;
		editor.file.buffer.curr = editor.file.buffer.curr->prev;
		--editor.file.cursor.y;
		editor.file.cursor.x = rx_to_cursor_x(editor.file.buffer.curr, desired_rx);
		edit_fix_cursor_x();
		if (start_of_line) {
			size_t fn = find_first_nonblank(editor.file.buffer.curr->s);
			size_t fn_x = index_to_mbnum(editor.file.buffer.curr->s, fn);
			if (fn_x > 0)
				editor.file.cursor.x = fn_x;
			editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
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
		editor.file.cursor.x = rx_to_cursor_x(editor.file.buffer.curr, desired_rx);
		edit_fix_cursor_x();
		if (start_of_line) {
			size_t fn = find_first_nonblank(editor.file.buffer.curr->s);
			size_t fn_x = index_to_mbnum(editor.file.buffer.curr->s, fn);
			if (fn_x > 0)
				editor.file.cursor.x = fn_x;
			editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
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
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_move_right(void) {
	if (editor.file.cursor.x < line_mblen(editor.file.buffer.curr))
		++editor.file.cursor.x;
	else if (editor.file.buffer.curr->next) {
		editor.file.buffer.curr = editor.file.buffer.curr->next;
		++editor.file.cursor.y;
		editor.file.cursor.x = 0;
	}
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
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
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
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
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
	render_scroll();
}
void edit_move_page_up(void) {
	size_t y = editor.rows;
	while (y--) edit_move_up();
	edit_fix_cursor_x();
	render_scroll();
	desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_page_down(void) {
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
	size_t len = line_mblen(editor.file.buffer.curr);
	if (editor.file.cursor.x > len)
		editor.file.cursor.x = len;
}
void edit_insert(const unsigned char *s) {
	size_t s_len = strlen((const char *)s);
	line_insert_str(
		editor.file.buffer.curr,
		mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x),
		s
	);
	editor.file.is_modified = TRUE;
	maybe_reset_modified();
	editor.file.cursor.x += index_to_mbnum(s, s_len);
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
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
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
}
void edit_enter(void) {
	Line *prev_line = editor.file.buffer.curr;
	line_insert(prev_line, prev_line->next);
	editor.file.buffer.num_lines++;
	if (editor.file.cursor.x < line_mblen(prev_line))
		break_line();
	editor.file.buffer.curr = prev_line->next;
	++editor.file.cursor.y;
	size_t indent_end = find_first_nonblank(prev_line->s);
	size_t cursor_pos_bytes = mbnum_to_index(prev_line->s, editor.file.cursor.x);
	if (cursor_pos_bytes < indent_end)
		indent_end = cursor_pos_bytes;
	if (indent_end > 0) {
		unsigned char *indent = (unsigned char *)xmalloc(indent_end + 1);
		memcpy(indent, prev_line->s, indent_end);
		indent[indent_end] = '\0';
		line_insert_str(editor.file.buffer.curr, 0, indent);
		free(indent);
	}
	editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, indent_end);
	editor.file.cursor.rx = cursor_x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
	desired_rx = editor.file.cursor.rx;
	editor.file.is_modified = TRUE;
	maybe_reset_modified();
	render_scroll();
}
static void delete_char(void) {
	if (editor.file.cursor.x == 0)
		return;
	size_t start = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x - 1);
	size_t char_len = utf8_len(editor.file.buffer.curr->s[start]);
	if (char_len == 0 || start + char_len > editor.file.buffer.curr->len)
		char_len = 1;
	line_delete_str(editor.file.buffer.curr, start, char_len);
	--editor.file.cursor.x;
	edit_fix_cursor_x();
}
static void delete_empty_line(void) {
	editor.file.buffer.curr = editor.file.buffer.curr->prev;
	editor.file.cursor.x = line_mblen(editor.file.buffer.curr);
	--editor.file.cursor.y;
	buf_delete_line(&editor.file.buffer, editor.file.buffer.curr->next);
}
static void break_line(void) {
	size_t at = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
	unsigned char *line_content = (unsigned char *)xmalloc(editor.file.buffer.curr->len - at + 1);
	memmove(line_content, editor.file.buffer.curr->s + at, editor.file.buffer.curr->len - at + 1);
	line_insert_str(editor.file.buffer.curr->next, 0, line_content);
	line_delete_str(editor.file.buffer.curr, at, editor.file.buffer.curr->len - at);
	free(line_content);
}
static void join_with_prev_line(void) {
	unsigned char *line_content = (unsigned char *)xmalloc(editor.file.buffer.curr->len + 1);
	memmove(line_content, editor.file.buffer.curr->s, editor.file.buffer.curr->len);
	line_content[editor.file.buffer.curr->len] = '\0';
	editor.file.cursor.x = line_mblen(editor.file.buffer.curr->prev);
	line_insert_str(editor.file.buffer.curr->prev, editor.file.buffer.curr->prev->len, line_content);
	editor.file.buffer.curr = editor.file.buffer.curr->prev;
	buf_delete_line(&editor.file.buffer, editor.file.buffer.curr->next);
	--editor.file.cursor.y;
	free(line_content);
}
void edit_process_key(void) {
	int special_key;
	unsigned char *s;
	size_t len = term_read(&s, &special_key);
	if (len != 0) {
		edit_insert(s);
	} else {
		switch (special_key) {
		case BACKSPACE: edit_backspace(); break;
		case TAB: edit_insert((unsigned char*)"\t"); break;
		case ENTER: edit_enter(); break;
		case HOME: edit_move_home(); break;
		case END: edit_move_end(); break;
		case PAGE_UP: edit_move_page_up(); break;
		case PAGE_DOWN: edit_move_page_down(); break;
		case ARROW_LEFT: edit_move_left(); break;
		case ARROW_UP: edit_move_up(); break;
		case ARROW_RIGHT: edit_move_right(); break;
		case ARROW_DOWN: edit_move_down(); break;
		case CTRL_HOME: edit_move_top(); break;
		case CTRL_END: edit_move_bottom(); break;
		case CTRL_ARROW_LEFT: edit_move_prev_word(); break;
		case CTRL_ARROW_RIGHT: edit_move_next_word(); break;
		case CTRL_ARROW_UP: edit_move_prev_paragraph(); break;
		case CTRL_ARROW_DOWN: edit_move_next_paragraph(); break;
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
		if (editor.file.path[0] == '\0' || !is_file_exist(editor.file.path)) {
			editor.file.is_modified = FALSE;
			return;
		}
	}
	if (editor.file.is_modified && buffer_matches_saved_file()) {
		editor.file.is_modified = FALSE;
	}
}
static bool_t buffer_matches_saved_file(void) {
	FILE *f;
	Line *line;
	char *disk_line = NULL;
	size_t cap = 0;
	ssize_t read;
	if (editor.file.path[0] == '\0') return FALSE;
	f = fopen(editor.file.path, "r");
	if (!f) return FALSE;
	line = editor.file.buffer.begin;
	while (line) {
		read = getline(&disk_line, &cap, f);
		if (read == -1) {
			free(disk_line);
			fclose(f);
			return FALSE;
		}
		while (read > 0 && (disk_line[read - 1] == '\n' || disk_line[read - 1] == '\r'))
			disk_line[--read] = '\0';
		if (strcmp((char *)line->s, disk_line) != 0) {
			free(disk_line);
			fclose(f);
			return FALSE;
		}
		line = line->next;
	}
	if (getline(&disk_line, &cap, f) != -1) {
		free(disk_line);
		fclose(f);
		return FALSE;
	}
	free(disk_line);
	fclose(f);
	return TRUE;
}
static bool_t is_blank(Line *line) { return line->len == 0; }
void edit_move_prev_paragraph(void) {
	if (editor.file.cursor.y == 0)
		return;
	Line *line = editor.file.buffer.curr;
	size_t y = editor.file.cursor.y;
	/* Skip current blank lines, if any */
	while (y > 0 && is_blank(line)) {
		line = line->prev;
		--y;
	}
	/* Skip the current paragraph */
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
void edit_move_next_paragraph(void) {
	if (editor.file.cursor.y >= editor.file.buffer.num_lines - 1)
		return;
	Line *line = editor.file.buffer.curr;
	size_t y = editor.file.cursor.y;
	/* Skip the remainder of the current paragraph */
	while (y < editor.file.buffer.num_lines - 1 && !is_blank(line)) {
		line = line->next;
		++y;
	}
	/* Skip following blank lines */
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
