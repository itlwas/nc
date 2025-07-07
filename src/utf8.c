#include "yoc.h"
#include <ctype.h>
#include <string.h>
#include <stdint.h>
static uint32_t decode_utf8(const unsigned char *s) {
	unsigned char c0 = s[0];
	if ((c0 & 0x80u) == 0)
		return c0;
	if ((c0 & 0xE0u) == 0xC0u)
		return ((uint32_t)(c0 & 0x1Fu) << 6) | (uint32_t)(s[1] & 0x3Fu);
	if ((c0 & 0xF0u) == 0xE0u)
		return ((uint32_t)(c0 & 0x0Fu) << 12) |
			((uint32_t)(s[1] & 0x3Fu) << 6) |
			(uint32_t)(s[2] & 0x3Fu);
	if ((c0 & 0xF8u) == 0xF0u)
		return ((uint32_t)(c0 & 0x07u) << 18) |
			((uint32_t)(s[1] & 0x3Fu) << 12) |
			((uint32_t)(s[2] & 0x3Fu) << 6) |
			(uint32_t)(s[3] & 0x3Fu);
	return 0xFFFDu;
}
static int is_wide_cp(uint32_t cp) {
	return (
		(cp >= 0x1100u && (cp <= 0x115Fu ||
			cp == 0x2329u || cp == 0x232Au ||
			(cp >= 0x2E80u && cp <= 0xA4CFu) ||
			(cp >= 0xAC00u && cp <= 0xD7A3u) ||
			(cp >= 0xF900u && cp <= 0xFAFFu) ||
			(cp >= 0xFE10u && cp <= 0xFE19u) ||
			(cp >= 0xFE30u && cp <= 0xFE6Fu) ||
			(cp >= 0xFF00u && cp <= 0xFF60u) ||
			(cp >= 0xFFE0u && cp <= 0xFFE6u) ||
			(cp >= 0x1F300u && cp <= 0x1F64Fu) ||
			(cp >= 0x1F900u && cp <= 0x1F9FFu) ||
			(cp >= 0x20000u && cp <= 0x3FFFDu)))
	);
}
static int is_combining_cp(uint32_t cp) {
	return (
		(cp >= 0x0300u && cp <= 0x036Fu)  ||
		(cp >= 0x1AB0u && cp <= 0x1AFFu)  ||
		(cp >= 0x1DC0u && cp <= 0x1DFFu)  ||
		(cp >= 0x20D0u && cp <= 0x20FFu)  ||
		(cp >= 0xFE20u && cp <= 0xFE2Fu)
	);
}
static int is_word_char(const unsigned char *s);
size_t utf8_len(unsigned char c) {
    if (LIKELY((c & 0x80u) == 0)) return 1;
    if ((c & 0xE0u) == 0xC0u && LIKELY(c >= 0xC2u)) return 2;
    if (LIKELY((c & 0xF0u) == 0xE0u)) return 3;
    if ((c & 0xF8u) == 0xF0u && LIKELY(c <= 0xF4u)) return 4;
    return 0;
}
bool_t is_continuation_byte(unsigned char c) {
    return (bool_t)UNLIKELY(((c & 0xC0u) == 0x80u));
}
bool_t is_alnum_mbchar(const unsigned char *s) {
    if ((*s & 0x80u) == 0) {
        return isalnum((unsigned char)*s);
    }
    return is_word_char(s);
}
size_t move_mbleft(const unsigned char *s, size_t pos) {
    while (pos && is_continuation_byte(s[--pos]));
    return pos;
}
size_t move_mbright(const unsigned char *s, size_t pos) {
    unsigned char c = s[pos];
    if (c == '\0') {
        return pos;
    }
    size_t len = utf8_len(c);
    if (len == 0) {
        len = 1;
    }
    while (len-- && s[pos] != '\0') {
        ++pos;
    }
    return pos;
}
size_t index_to_mbnum(const unsigned char *s, size_t n) {
    size_t num = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!is_continuation_byte(s[i])) {
            ++num;
        }
    }
    return num;
}
size_t mbnum_to_index(const unsigned char *s, size_t n) {
    size_t pos = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[pos] == '\0') {
            break;
        }
        pos = move_mbright(s, pos);
    }
    return pos;
}
size_t char_display_width(const unsigned char *s) {
	unsigned char c = *s;
	if (LIKELY(c < 0x80u)) {
		return (c < 0x20u || c == 0x7Fu) ? 0u : 1u;
	}
	if (is_continuation_byte(c)) {
		return 0u;
	}
	uint32_t cp = decode_utf8(s);
	if (is_combining_cp(cp)) {
		return 0u;
	}
	return is_wide_cp(cp) ? 2u : 1u;
}
size_t x_to_rx(Line *line, size_t x) {
    size_t rx = 0;
    size_t pos = mbnum_to_index(line->s, x);
    for (size_t i = 0; i < pos;) {
        unsigned char c = line->s[i];
        if (c == '\t') {
            rx += editor.tabsize - (rx % editor.tabsize);
            i++;
        } else {
            size_t len = utf8_len(c);
            if (len == 0) len = 1;
            rx += char_display_width(&line->s[i]);
            i += len;
        }
    }
    return rx;
}
size_t rx_to_x(Line *line, size_t rx_target) {
    size_t rx = 0;
    size_t x = 0;
    size_t pos = 0;
    while (line->s[pos] != '\0' && rx < rx_target) {
        unsigned char c = line->s[pos];
        if (c == '\t') {
            size_t tab_w = editor.tabsize - (rx % editor.tabsize);
            if (rx + tab_w > rx_target) break;
            rx += tab_w;
            pos++;
        } else {
            size_t char_w = char_display_width(&line->s[pos]);
            size_t len = utf8_len(c);
            if (len == 0) len = 1;
            if (rx + char_w > rx_target) break;
            rx += char_w;
            pos += len;
        }
        x++;
    }
    return x;
}
size_t str_width(const unsigned char *s, size_t len) {
    return length_to_width(s, len);
}
size_t length_to_width(const unsigned char *s, size_t len) {
    size_t col = 0;
    for (size_t i = 0; i < len;) {
        unsigned char c = s[i];
        if (c == '\t') {
            col += editor.tabsize - (col % editor.tabsize);
            ++i;
            continue;
        }
        size_t char_len = utf8_len(c);
        if (char_len == 0) char_len = 1;
        col += char_display_width(&s[i]);
        i += char_len;
    }
    return col;
}
size_t width_to_length(const unsigned char *s, size_t width) {
    size_t len = 0;
    size_t col = 0;
    while (s[len] != '\0' && col < width) {
        unsigned char c = s[len];
        if (c == '\t') {
            size_t tab_w = editor.tabsize - (col % editor.tabsize);
            if (col + tab_w > width) break;
            col += tab_w;
            ++len;
            continue;
        }
        size_t char_len = utf8_len(c);
        if (char_len == 0) char_len = 1;
        size_t char_w  = char_display_width(&s[len]);
        if (col + char_w > width) break;
        col += char_w;
        len += char_len;
    }
    return len;
}
size_t find_first_nonblank(const unsigned char *s) {
    size_t i = 0;
    while (s[i] != '\0' && isspace((unsigned char)s[i])) {
        ++i;
    }
    return i;
}
static int is_word_char(const unsigned char *s) {
    if (s[0] == '\0' || isspace((unsigned char)s[0])) {
        return 0;
    }
    if ((s[0] & 0x80u) != 0) {
        return utf8_len(s[0]) > 0;
    }
    return !ispunct((unsigned char)s[0]);
}
