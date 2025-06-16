#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include "yoc.h"
#if defined(_WIN32) && !defined(YOCP_NO_WCWIDTH)
#define YOCP_NO_WCWIDTH
#endif
static size_t char_display_width_impl(const unsigned char *restrict s, size_t *restrict char_len_out);
void die(const char *msg) {
	switch_to_normal_buffer();
	perror(msg);
	fflush(stderr);
	printf("\r");
	exit(EXIT_FAILURE);
}
bool is_alnum_mbchar(const unsigned char *s) {
	if ((*s & 0x80u) == 0)
		return isalnum((unsigned char)*s);
	mbstate_t state;
	memset(&state, 0, sizeof(state));
	wchar_t wc;
	size_t res = mbrtowc(&wc, (const char *)s, MAXCHARLEN, &state);
	if (res == (size_t)-1 || res == (size_t)-2)
		return false;
	return iswalnum(wc);
}
size_t move_mbleft(const unsigned char *s, size_t pos) {
	while (pos && is_continuation_byte(s[--pos]));
	return pos;
}
size_t move_mbright(const unsigned char *s, size_t pos) {
	if (s[pos] == '\0')
		return pos;
	++pos;
	while (s[pos] != '\0' && is_continuation_byte(s[pos]))
		++pos;
	return pos;
}
size_t index_to_mbnum(const unsigned char *s, size_t n) {
	size_t num = 0;
	for (size_t i = 0; i < n; ++i)
		if (!is_continuation_byte(s[i]))
			++num;
	return num;
}
size_t mbnum_to_index(const unsigned char *s, size_t n) {
	size_t pos = 0;
	for (size_t i = 0; i < n; ++i) {
		if (!is_continuation_byte(s[pos]))
			++pos;
		while (is_continuation_byte(s[pos]))
			++pos;
	}
	return pos;
}
void fix_cursor_x(void) {
	size_t len = line_mblen(yoc.file.buffer.curr);
	if (yoc.file.cursor.x > len)
		yoc.file.cursor.x = len;
}
size_t get_tabsize(void) {
	return 4;
}
size_t cursor_x_to_rx(Line *line, size_t x) {
	size_t rx = 0;
	size_t pos = mbnum_to_index(line->s, x);
	for (size_t i = 0; i < pos;) {
		if (line->s[i] == '\t') {
			rx += yoc.tabsize - rx % yoc.tabsize;
			i += 1;
			continue;
		}
		if (!is_continuation_byte(line->s[i])) {
			size_t char_len;
			rx += char_display_width_impl(line->s + i, &char_len);
			i += char_len;
		} else {
			i += 1;
		}
	}
	return rx;
}
size_t str_width(const unsigned char *s, size_t len) {
	return length_to_width(s, len);
}
size_t length_to_width(const unsigned char *s, size_t len) {
	size_t col = 0;
	for (size_t i = 0; i < len;) {
		if (s[i] == '\t') {
			col += yoc.tabsize - col % yoc.tabsize;
			i += 1;
			continue;
		}
		if (!is_continuation_byte(s[i])) {
			size_t char_len;
			col += char_display_width_impl(s + i, &char_len);
			i += char_len;
		} else {
			i += 1;
		}
	}
	return col;
}
size_t width_to_length(const unsigned char *s, size_t width) {
	size_t len = 0;
	for (size_t col = 0; col < width && s[len] != '\0';) {
		if (s[len] == '\t') {
			size_t ts = yoc.tabsize - col % yoc.tabsize;
			if (col + ts > width)
				break;
			col += ts;
			++len;
			continue;
		}
		if (!is_continuation_byte(s[len])) {
			size_t char_len;
			size_t w = char_display_width_impl(s + len, &char_len);
			if (col + w > width)
				break;
			col += w;
			len += char_len;
		} else {
			++len;
		}
	}
	return len;
}
size_t find_first_nonblank(const unsigned char *s) {
	size_t i = 0;
	while (s[i] != '\0' && isspace((unsigned char)s[i]))
		++i;
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
		while (is_continuation_byte(line->s[pos]))
			++pos;
	}
	return x;
}
void *xmalloc(size_t size) {
	void *ptr = malloc(size);
	if (!ptr)
		die("malloc");
	return ptr;
}
void *xrealloc(void *ptr, size_t size) {
	void *new_ptr = realloc(ptr, size);
	if (!new_ptr)
		die("realloc");
	return new_ptr;
}
static size_t char_display_width_impl(const unsigned char *restrict s, size_t *restrict char_len_out) {
	if (YOC_LIKELY((*s & 0x80u) == 0)) {
		if (char_len_out)
			*char_len_out = 1;
		return 1;
	}
	size_t clen = utf8_len(*s);
	if (clen == UTF8_CONTINUATION_BYTE || clen == 0 || clen > MAXCHARLEN)
		clen = 1;
	if (char_len_out)
		*char_len_out = clen;
#ifndef YOCP_NO_WCWIDTH
	mbstate_t st = {0};
	wchar_t wc;
	size_t res = mbrtowc(&wc, (const char *)s, clen, &st);
	if (res == (size_t)-1 || res == (size_t)-2)
		return 1;
	int w = wcwidth(wc);
	return (w < 0) ? 1u : (size_t)w;
#else
	(void)clen;
	return 1;
#endif
}
size_t char_display_width(const unsigned char *s) {
	return char_display_width_impl(s, NULL);
}
