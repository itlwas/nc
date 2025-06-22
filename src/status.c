#include "yoc.h"
#include <string.h>
static const char *extract_filename(const char *path) {
	const char *slash = strrchr(path, '/');
	const char *backslash = strrchr(path, '\\');
	const char *sep = (slash > backslash) ? slash : backslash;
	return sep ? sep + 1 : path;
}
typedef struct {
	size_t cx;
	char *msg;
	Line *input;
	size_t charsoff;
} StatusInput;
static void status_input_print(void);
static bool_t status_process_input(void);
static void status_input_init(void);
static void status_input_free(void);
static void status_realloc(size_t len);
static void status_set_default(void);
static void status_do_insert(unsigned char *s);
static void status_do_end(void);
static void status_do_home(void);
static void status_do_backspace(void);
static void status_do_arrow_left(void);
static void status_do_arrow_right(void);
static StatusInput *statin;
enum S_Mode status_mode;
void status_init(void) {
	editor.file.status.msg = (char *)xmalloc(BUFF_SIZE);
	editor.file.status.msg[0] = '\0';
	editor.file.status.cap = BUFF_SIZE;
	editor.file.status.len = 0;
	status_mode = NORMAL;
}
void status_free(void) {
	free(editor.file.status.msg);
	editor.file.status.cap = 0;
	editor.file.status.len = 0;
}
void status_msg(const char *msg) {
	size_t len = strlen(msg);
	status_realloc(len + 1);
	memcpy(editor.file.status.msg, msg, len);
	editor.file.status.msg[len] = '\0';
	editor.file.status.len = len;
	status_mode = MESSAGE;
}
void status_print(void) {
	if (status_mode == INPUT_MODE) {
		status_input_print();
	} else {
		char rev_on[] = "\x1b[7m";
		char rev_off[] = "\x1b[0m";
		size_t mlen;
		if (status_mode == NORMAL)
			status_set_default();
		term_write((unsigned char*)rev_on, strlen(rev_on));
		term_clear_line();
		mlen = editor.file.status.len;
		if (mlen > editor.cols)
			mlen = editor.cols;
		term_write((unsigned char *)editor.file.status.msg, mlen);
		term_write((unsigned char*)rev_off, strlen(rev_off));
		status_mode = NORMAL;
	}
}
bool_t status_input(Line *input, char *msg, const char *placeholder) {
	bool_t ret;
	status_input_init();
	statin->msg = msg;
	statin->input = input;
	if (placeholder) {
		line_insert_str(statin->input, 0, (unsigned char *)placeholder);
		statin->cx += strlen(placeholder);
	}
	ret = status_process_input();
	status_input_free();
	return ret;
}
static bool_t status_process_input(void) {
	int special_key;
	unsigned char *s;
	size_t len;
	status_mode = INPUT_MODE;
	for (;;) {
		status_input_print();
		len = term_read(&s, &special_key);
		if (len != 0) {
			status_do_insert(s);
		} else {
			switch (special_key) {
				case ESC:
					status_mode = NORMAL;
					return FALSE;
				case ENTER:
					status_mode = NORMAL;
					return TRUE;
				case HOME: status_do_home(); break;
				case END: status_do_end(); break;
				case ARROW_LEFT: status_do_arrow_left(); break;
				case ARROW_RIGHT: status_do_arrow_right(); break;
				case BACKSPACE: status_do_backspace(); break;
			}
		}
	}
}
static void status_input_print(void) {
	char *msg = statin->msg;
	size_t msglen = strlen(statin->msg);
	size_t free_space;
	size_t len;
	size_t input_width;
	size_t start;
	if (msglen >= editor.cols - 2) {
		msg = ": ";
		msglen = 2;
	}
	free_space = editor.cols - msglen - 1;
	if (statin->cx < statin->charsoff)
		statin->charsoff = statin->cx;
	else if (statin->cx - statin->charsoff > free_space)
		statin->charsoff = statin->cx - free_space;
	input_width = line_get_width(statin->input);
	start = mbnum_to_index(statin->input->s, statin->charsoff);
	if (input_width - statin->charsoff > free_space)
		len = width_to_length(statin->input->s + start, free_space);
	else
		len = statin->input->len - start;
	term_set_cursor(0, editor.rows);
	term_clear_line();
	term_write((unsigned char *)msg, msglen);
	term_write(statin->input->s + start, len);
	term_set_cursor(statin->cx + msglen - statin->charsoff, editor.rows);
}
static void status_realloc(size_t len) {
	if (len >= editor.file.status.cap) {
		size_t new_cap = editor.file.status.cap;
		while (len >= new_cap)
			new_cap = new_cap == 0 ? BUFF_SIZE : new_cap * 2;
		editor.file.status.msg = (char *)xrealloc(editor.file.status.msg, new_cap);
		editor.file.status.cap = new_cap;
	}
}
static void status_set_default(void) {
	const char *path;
	size_t left_len;
	char right_str[32];
	size_t right_len;
	size_t fill_len;
	status_realloc(editor.cols + 1);
	path = editor.file.path[0] ? extract_filename(editor.file.path) : "[No Name]";
	if (editor.file.is_modified && editor.file.path[0] != '\0')
		left_len = (size_t)snprintf(editor.file.status.msg, editor.file.status.cap, "%s [+]", path);
	else
		left_len = (size_t)snprintf(editor.file.status.msg, editor.file.status.cap, "%s", path);
	right_len = (size_t)snprintf(right_str, sizeof(right_str), "%lu:%lu", (unsigned long)(editor.file.cursor.y + 1), (unsigned long)(editor.file.cursor.rx + 1));
	if (left_len + right_len + 1 > editor.cols) {
		if (editor.cols > right_len + 1)
			left_len = editor.cols - right_len - 1;
		else
			left_len = 0;
	}
	editor.file.status.msg[left_len] = '\0';
	fill_len = editor.cols - left_len - right_len;
	memset(editor.file.status.msg + left_len, ' ', fill_len);
	memcpy(editor.file.status.msg + left_len + fill_len, right_str, right_len + 1);
	editor.file.status.len = left_len + fill_len + right_len;
}
static void status_do_insert(unsigned char *s) {
	size_t str_len = strlen((const char *)s);
	size_t chars;
	if (str_len == 0)
		return;
	chars = index_to_mbnum(s, str_len);
	line_insert_str(statin->input, mbnum_to_index(statin->input->s, statin->cx), s);
	statin->cx += chars;
}
static void status_do_home(void) {
	statin->cx = 0;
}
static void status_do_end(void) {
	statin->cx = line_get_mblen(statin->input);
}
static void status_do_arrow_left(void) {
	if (statin->cx > 0)
		--statin->cx;
}
static void status_do_arrow_right(void) {
	if (statin->cx < line_get_mblen(statin->input))
		++statin->cx;
}
static void status_do_backspace(void) {
	size_t start;
	size_t char_len;
	if (statin->input->len == 0 || statin->cx == 0)
		return;
	start = mbnum_to_index(statin->input->s, statin->cx - 1);
	char_len = utf8_len(statin->input->s[start]);
	if (char_len == 0 || start + char_len > statin->input->len)
		char_len = 1;
	line_del_str(statin->input, start, char_len);
	--statin->cx;
	if (statin->cx < statin->charsoff)
		statin->charsoff = statin->cx;
}
static void status_input_init(void) {
	statin = (StatusInput *)xmalloc(sizeof(StatusInput));
	statin->cx = 0;
	statin->charsoff = 0;
}
static void status_input_free(void) {
	free(statin);
}
