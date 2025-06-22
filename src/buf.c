#include "yoc.h"
#include <string.h>
static size_t calc_capacity(size_t current, size_t required) {
	if (current < LINE_INLINE_CAP)
		current = LINE_INLINE_CAP;
	while (current < required) {
		size_t candidate = (current < 1024) ? current << 1 : current + (current >> 1);
		if (candidate <= current || candidate > (size_t)-1 / 2)
			return required;
		current = candidate;
	}
	return current;
}
static void line_reserve(Line *line, size_t additional) {
	size_t required = line->len + additional + 1;
	size_t new_cap;
	if (required <= line->cap)
		return;
	new_cap = calc_capacity(line->cap, required);
	if (line->s == line->inline_space) {
		unsigned char *heap_mem = (unsigned char *)xmalloc(new_cap);
		memcpy(heap_mem, line->s, line->len + 1);
		line->s = heap_mem;
	} else {
		line->s = (unsigned char *)xrealloc(line->s, new_cap);
	}
	line->cap = new_cap;
}
void buf_init(Buffer *buffer) {
	buffer->begin = buffer->curr = line_new(NULL, NULL);
	buffer->num_lines = 1;
}
void buf_free(Buffer *buffer) {
	while (buffer->begin) {
		Line *temp = buffer->begin;
		buffer->begin = buffer->begin->next;
		line_del(temp);
	}
}
void buf_del_line(Buffer *buffer, Line *line) {
	if (!buffer || !line)
		return;
	if (line->prev)
		line->prev->next = line->next;
	if (line->next)
		line->next->prev = line->prev;
	if (buffer->begin == line)
		buffer->begin = line->next;
	if (buffer->curr == line)
		buffer->curr = line->next ? line->next : line->prev;
	if (line->s != line->inline_space)
		free(line->s);
	free(line);
	if (buffer->num_lines)
		--buffer->num_lines;
	if (buffer->num_lines == 0) {
		buffer->begin = buffer->curr = line_new(NULL, NULL);
		buffer->num_lines = 1;
	} else if (!buffer->curr) {
		buffer->curr = buffer->begin;
	}
}
Line *line_new(Line *prev, Line *next) {
	Line *line = (Line *)xmalloc(sizeof(Line));
	line->s = line->inline_space;
	line->inline_space[0] = '\0';
	line->len = 0;
	line->cap = LINE_INLINE_CAP;
	line->width = LINE_WIDTH_UNCACHED;
	line->mb_len = LINE_MBLEN_UNCACHED;
	line->prev = prev;
	line->next = next;
	if (prev)
		prev->next = line;
	if (next)
		next->prev = line;
	return line;
}
void line_del(Line *line) {
	if (line->prev && line->next) {
		line->prev->next = line->next;
		line->next->prev = line->prev;
	} else if (line->prev) {
		line->prev->next = NULL;
	} else if (line->next) {
		line->next->prev = NULL;
	}
	if (line->s != line->inline_space)
		free(line->s);
	free(line);
}
void line_free(Line *line) {
	while (line) {
		Line *temp = line;
		line = line->next;
		if (temp->s != temp->inline_space)
			free(temp->s);
		free(temp);
	}
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
	line->mb_len = LINE_MBLEN_UNCACHED;
}
void line_del_char(Line *line, size_t at) {
	line_del_str(line, at, 1);
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
	line->mb_len = LINE_MBLEN_UNCACHED;
}
void line_insert_strn(Line *line, size_t at, const unsigned char *str, size_t len) {
	if (len == 0)
		return;
	if (at > line->len)
		at = line->len;
	line_reserve(line, len);
	if (at < line->len)
		memmove(line->s + at + len, line->s + at, line->len - at + 1);
	memcpy(line->s + at, str, len);
	line->len += len;
	line->s[line->len] = '\0';
	line->width = LINE_WIDTH_UNCACHED;
	line->mb_len = LINE_MBLEN_UNCACHED;
}
void line_del_str(Line *line, size_t at, size_t len) {
	if (len == 0 || at >= line->len)
		return;
	if (at + len > line->len)
		len = line->len - at;
	memmove(line->s + at, line->s + at + len, line->len - at - len + 1);
	line->len -= len;
	line->width = LINE_WIDTH_UNCACHED;
	line->mb_len = LINE_MBLEN_UNCACHED;
}
size_t line_get_width(Line *line) {
	if (line->width == LINE_WIDTH_UNCACHED)
		line->width = length_to_width(line->s, line->len);
	return line->width;
}
size_t line_get_mblen(Line *line) {
	if (line->mb_len == LINE_MBLEN_UNCACHED)
		line->mb_len = index_to_mbnum(line->s, line->len);
	return line->mb_len;
}
