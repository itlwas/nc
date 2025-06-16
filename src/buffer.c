#include <stdlib.h>
#include "yoc.h"
void buffer_init(Buffer *buffer) {
	buffer->begin = buffer->curr = line_insert(NULL, NULL);
	buffer->num_lines = 1;
}
void buffer_free(Buffer *buffer) {
	while (buffer->begin) {
		Line *temp = buffer->begin;
		buffer->begin = buffer->begin->next;
		line_delete(temp);
	}
}
Line *line_insert(Line *prev, Line *next) {
	Line *line = (Line *)xmalloc(sizeof(Line));
	line->s = (unsigned char *)xmalloc(1);
	line->s[0] = '\0';
	line->len = 0;
	line->cap = 1;
	line->width = LINE_WIDTH_UNCACHED;
	line->prev = prev;
	line->next = next;
	if (prev) prev->next = line;
	if (next) next->prev = line;
	return line;
}
void line_delete(Line *line) {
	if (line->prev && line->next) {
		line->prev->next = line->next;
		line->next->prev = line->prev;
	} else if (line->prev) {
		line->prev->next = NULL;
	} else if (line->next) {
		line->next->prev = NULL;
	}
	free(line->s);
	free(line);
}
static inline void line_reserve(Line *line, size_t additional) {
	size_t required = line->len + additional + 1;
	if (required <= line->cap) return;
	size_t new_cap = line->cap;
	if (new_cap == 0) new_cap = 1;
	while (new_cap < required && new_cap <= SIZE_MAX / 2) {
		new_cap <<= 1;
	}
	if (new_cap < required) {
		new_cap = required;
	}
	line->s = (unsigned char *)xrealloc(line->s, new_cap);
	line->cap = new_cap;
}
void line_insert_str(Line *line, size_t at, const unsigned char *str) {
	size_t str_len = strlen((const char *)str);
	if (str_len == 0)
		return;
	if (at > line->len)
		at = line->len;
	line_reserve(line, str_len);
	if (at < line->len)
		memmove(line->s + at + str_len, line->s + at, line->len - at + 1);
	memcpy(line->s + at, str, str_len);
	line->len += str_len;
	line->s[line->len] = '\0';
	line->width = LINE_WIDTH_UNCACHED;
}
void line_delete_str(Line *line, size_t at, size_t len) {
	if (len == 0 || at >= line->len)
		return;
	if (at + len > line->len)
		len = line->len - at;
	memmove(line->s + at, line->s + at + len, line->len - at - len + 1);
	line->len -= len;
	line->width = LINE_WIDTH_UNCACHED;
}
void line_insert_char(Line *line, size_t at, unsigned char c) {
	if (at > line->len)
		at = line->len;
	line_reserve(line, 1);
	if (at < line->len)
		memmove(line->s + at + 1, line->s + at, line->len - at + 1);
	line->s[at] = c;
	++line->len;
	line->s[line->len] = '\0';
	line->width = LINE_WIDTH_UNCACHED;
}
void line_delete_char(Line *line, size_t at) {
	line_delete_str(line, at, 1);
}
void line_free(Line *line) {
	while (line) {
		Line *temp = line;
		line = line->next;
		free(temp->s);
		free(temp);
	}
}
size_t line_width(Line *line) {
	if (line->width == LINE_WIDTH_UNCACHED)
		line->width = length_to_width(line->s, line->len);
	return line->width;
}
size_t line_mblen(Line *line) {
	return index_to_mbnum(line->s, line->len);
}
void buffer_delete_line(Buffer *buffer, Line *line) {
	if (!buffer || !line) return;
	if (line->prev)
		line->prev->next = line->next;
	if (line->next)
		line->next->prev = line->prev;
	if (buffer->begin == line)
		buffer->begin = line->next;
	if (buffer->curr == line)
		buffer->curr = line->next ? line->next : line->prev;
	free(line->s);
	free(line);
	if (buffer->num_lines)
		--buffer->num_lines;
}
