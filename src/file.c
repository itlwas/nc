#define _CRT_SECURE_NO_WARNINGS
#include "yoc.h"
#include <stdlib.h>
#include <stdio.h>
void file_init(File *file) {
	status_init();
	file->path = (char *)xmalloc(64);
	file->path[0] = '\0';
	file->cap = 64;
	file->cursor.x = 0;
	file->cursor.y = 0;
	file->cursor.rx = 0;
	buffer_init(&file->buffer);
	file->is_modified = false;
}
void file_load(File *file) {
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	FILE *f = fopen(file->path, "r");
	if (!f)
		die("fopen");

	read = getline(&line, &len, f);
	if (read != -1) {
		while (read > 0 && (line[read - 1] == '\n' || line[read - 1] == '\r'))
			line[--read] = '\0';
		line_insert_str(file->buffer.curr, 0, (unsigned char *)line);
	}

	while ((read = getline(&line, &len, f)) != -1) {
		while (read > 0 && (line[read - 1] == '\n' || line[read - 1] == '\r'))
			line[--read] = '\0';
		line_insert(file->buffer.curr, file->buffer.curr->next);
		file->buffer.curr = file->buffer.curr->next;
		line_insert_str(file->buffer.curr, 0, (unsigned char *)line);
		file->buffer.num_lines++;
	}

	file->buffer.curr = file->buffer.begin;
	free(line);
	fclose(f);
}
void file_save(File *file) {
	FILE *f = fopen(file->path, "w");
	if (!f)
		die("fopen");
	Line *line = file->buffer.begin;
	for (; line && line->next; line = line->next) {
		fputs((char *)line->s, f);
		fputs("\n", f);
	}
	if (line) {
		fputs((char *)line->s, f);
		if (line->len != 0)
			fputs("\n", f);
	}
	fclose(f);
}
void file_free(File *file) {
	buffer_free(&file->buffer);
	free(file->path);
	status_free();
}
