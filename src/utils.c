#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <stdio.h>
#include "yoc.h"
void die(const char *msg) {
	switch_to_normal_buffer();
	perror(msg);
	fflush(stderr);
	printf("\r");
	exit(EXIT_FAILURE);
}
size_t utf8_len(unsigned char c) {
	if ((c & (1 << 7)) == 0) return 1;
	if ((c & (1 << 6)) == 0) return UTF8_CONTINUATION_BYTE;
	if ((c & (1 << 5)) == 0) return 2;
	if ((c & (1 << 4)) == 0) return 3;
	return 4;
}
bool is_alnum_mbchar(const unsigned char *s) {
	wchar_t wc;
	if (mbtowc(&wc, (char *)s, MAXCHARLEN) < 0) return 0;
	return iswalnum(wc);
}
size_t move_mbleft(const unsigned char *s, size_t pos) {
	while (pos && is_continuation_byte(s[--pos]));
	return pos;
}
size_t move_mbright(const unsigned char *s, size_t pos) {
	while (s[++pos] && is_continuation_byte(s[pos]));
	return pos;
}
size_t index_to_mbnum(const unsigned char *s, size_t n) {
	size_t num = 0;
	for (size_t i = 0; i < n; ++i)
		if (!is_continuation_byte(s[i])) ++num;
	return num;
}
size_t mbnum_to_index(const unsigned char *s, size_t n) {
	size_t pos = 0;
	for (size_t i = 0; i < n; ++i) {
		if (!is_continuation_byte(s[pos])) ++pos;
		while (is_continuation_byte(s[pos])) ++pos;
	}
	return pos;
}
void fix_cursor_x(void) {
	size_t len = line_mblen(yoc.file.buffer.curr);
	if (yoc.file.cursor.x > len) yoc.file.cursor.x = len;
}
#ifdef _WIN32
size_t get_tabsize(void) {
	return 4;
}
#else
size_t get_tabsize(void) {
	return 4;
}
#endif
size_t cursor_x_to_rx(Line *line, size_t x) {
	size_t rx = 0;
	size_t pos = mbnum_to_index(line->s, x);
	for (size_t i = 0; i < pos; ++i) {
		if (line->s[i] == '\t')
			rx += yoc.tabsize - rx % yoc.tabsize;
		else if (!is_continuation_byte(line->s[i]))
			++rx;
	}
	return rx;
}
size_t str_width(const unsigned char *s, size_t len) {
	return length_to_width(s, len);
}
size_t length_to_width(const unsigned char *s, size_t len) {
	size_t col = 0;
	for (size_t i = 0; i < len; ++i) {
		if (s[i] == '\t')
			col += yoc.tabsize - col % yoc.tabsize;
		else if (!is_continuation_byte(s[i]))
			++col;
	}
	return col;
}
size_t width_to_length(const unsigned char *s, size_t width) {
	size_t len = 0, tabsize;
	for (size_t col = 0; width > 0; ++len) {
		if (s[len] == '\t') {
			tabsize = yoc.tabsize - col % yoc.tabsize;
			width -= tabsize;
			col += tabsize;
		} else if (!is_continuation_byte(s[len])) {
			++col;
			--width;
		}
	}
	while (s[len] != 0 && is_continuation_byte(s[len])) ++len;
	return len;
}
size_t find_first_nonblank(const unsigned char *s) {
	size_t i = 0;
	while (isspace(s[i]) && s[i++] != 0);
	return i;
}
size_t rx_to_cursor_x(Line *line, size_t rx_target) {
	size_t rx = 0;
	size_t x = 0;
	size_t pos = 0;
	while (line->s[pos] != '\0') {
		if (rx >= rx_target)
			break;
		if (line->s[pos] == '\t') {
			size_t ts = yoc.tabsize - (rx % yoc.tabsize);
			if (rx + ts > rx_target)
				break;
			rx += ts;
			++x;
			++pos;
			continue;
		}
		if (!is_continuation_byte(line->s[pos])) {
			if (rx + 1 > rx_target)
				break;
			++rx;
			++x;
		}
		++pos;
		while (is_continuation_byte(line->s[pos])) {
			++pos;
		}
	}
	return x;
}
