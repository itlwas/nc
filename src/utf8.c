#include "yoc.h"
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>
#include <string.h>
#if defined(_WIN32) && !defined(YOCP_NO_WCWIDTH)
#define YOCP_NO_WCWIDTH
#endif
#ifndef YOCP_NO_WCWIDTH
#include <wchar.h>
#endif
static size_t char_display_width_impl(const unsigned char *s, size_t *char_len_out);
size_t utf8_len(unsigned char c) {
	if ((c & 0x80u) == 0) return 1;
	if ((c & 0x40u) == 0) return 0;
	if ((c & 0x20u) == 0) return 2;
	if ((c & 0x10u) == 0) return 3;
	return 4;
}
bool_t is_continuation_byte(unsigned char c) {
	return (c & 0xC0u) == 0x80u;
}
bool_t is_alnum_mbchar(const unsigned char *s) {
	mbstate_t state;
	wchar_t wc;
	size_t res;
	if ((*s & 0x80u) == 0)
		return isalnum((unsigned char)*s);
	memset(&state, 0, sizeof(state));
	res = mbrtowc(&wc, (const char *)s, MAXCHARLEN, &state);
	if (res == (size_t)-1 || res == (size_t)-2)
		return FALSE;
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
	size_t i = 0;
	for (i = 0; i < n; ++i)
		if (!is_continuation_byte(s[i]))
			++num;
	return num;
}
size_t mbnum_to_index(const unsigned char *s, size_t n) {
	size_t pos = 0;
	size_t i = 0;
	for (i = 0; i < n; ++i) {
		if (!is_continuation_byte(s[pos]))
			++pos;
		while (is_continuation_byte(s[pos]))
			++pos;
	}
	return pos;
}
size_t char_display_width(const unsigned char *s) {
	return char_display_width_impl(s, NULL);
}
static size_t char_display_width_impl(const unsigned char *s, size_t *char_len_out) {
	size_t clen;
	if (((*s & 0x80u) == 0)) {
		if (char_len_out)
			*char_len_out = 1;
		return 1;
	}
	clen = utf8_len(*s);
	if (clen == 0 || clen > MAXCHARLEN)
		clen = 1;
	if (char_len_out)
		*char_len_out = clen;
#ifndef YOCP_NO_WCWIDTH
	{
		mbstate_t st;
		wchar_t wc;
		size_t res;
		int w;
		memset(&st, 0, sizeof(st));
		res = mbrtowc(&wc, (const char *)s, clen, &st);
		if (res == (size_t)-1 || res == (size_t)-2)
			return 1;
		w = wcwidth(wc);
		return (w < 0) ? 1u : (size_t)w;
	}
#else
	(void)clen;
	return 1;
#endif
}
size_t cursor_x_to_rx(Line *line, size_t x) {
	size_t rx = 0;
	size_t pos = mbnum_to_index(line->s, x);
	size_t i;
	for (i = 0; i < pos;) {
		if (line->s[i] == '\t') {
			rx += editor.tabsize - rx % editor.tabsize;
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
size_t rx_to_cursor_x(Line *line, size_t rx_target) {
	size_t rx = 0;
	size_t x = 0;
	size_t pos = 0;
	while (line->s[pos] != '\0') {
		if (rx >= rx_target)
			break;
		if (line->s[pos] == '\t') {
			size_t ts = editor.tabsize - (rx % editor.tabsize);
			if (rx + ts > rx_target)
				break;
			rx += ts;
			++x;
			++pos;
			continue;
		}
		if (!is_continuation_byte(line->s[pos])) {
			size_t char_len;
			size_t w = char_display_width_impl(line->s + pos, &char_len);
			if (rx + w > rx_target)
				break;
			rx += w;
			++x;
			pos += char_len;
		} else {
			++pos;
		}
	}
	return x;
}
size_t str_width(const unsigned char *s, size_t len) {
	return length_to_width(s, len);
}
size_t length_to_width(const unsigned char *s, size_t len) {
	size_t col = 0;
	size_t i;
	for (i = 0; i < len;) {
		if (s[i] == '\t') {
			col += editor.tabsize - col % editor.tabsize;
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
	size_t col;
	for (col = 0; col < width && s[len] != '\0';) {
		if (s[len] == '\t') {
			size_t ts = editor.tabsize - col % editor.tabsize;
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
