#include "yoc.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
static void ensure_trailing_empty_line(File *file);
void file_init(File *file) {
	status_init();
	file->path = (char *)xmalloc(64);
	file->path[0] = '\0';
	file->cap = 64;
	file->cursor.x = 0;
	file->cursor.y = 0;
	file->cursor.rx = 0;
	buf_init(&file->buffer);
	file->is_modified = FALSE;
}
void file_free(File *file) {
	buf_free(&file->buffer);
	free(file->path);
	render_free();
	status_free();
}
void file_load(File *file) {
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	FILE *f = fopen(file->path, "r");
	if (!f) die("fopen");
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
	FILE *f;
	Line *line;
	ensure_trailing_empty_line(file);
	f = fopen(file->path, "w");
	if (!f) die("fopen");
	line = file->buffer.begin;
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
bool_t file_save_prompt(void) {
	Line *input;
	if (editor.file.path[0] != '\0') {
		file_save(&editor.file);
		editor.file.is_modified = FALSE;
		return TRUE;
	}
	input = line_insert(NULL, NULL);
	if (status_input(input, "Save as: ", NULL)) {
		if (input->len > 0) {
			if (input->len + 1 > editor.file.cap) {
				editor.file.cap = input->len + 1;
				editor.file.path = (char *)xrealloc(editor.file.path, editor.file.cap);
			}
			memcpy(editor.file.path, input->s, input->len + 1);
			file_save(&editor.file);
			editor.file.is_modified = FALSE;
			line_free(input);
			return TRUE;
		}
	}
	line_free(input);
	return FALSE;
}
void file_quit_prompt(void) {
	if (editor.file.is_modified) {
		Line *input = line_insert(NULL, NULL);
		char prompt[256];
		const char *name = editor.file.path[0] ? editor.file.path : "[No Name]";
		sprintf(prompt, "Save changes to %s before closing? (y,n,esc): ", name);
		if (status_input(input, prompt, NULL)) {
			char answer = tolower((unsigned char)input->s[0]);
			if (answer == 'y') {
				if (!file_save_prompt()) {
					line_free(input);
					return;
				}
			} else if (answer != 'n') {
				line_free(input);
				return;
			}
		} else {
			line_free(input);
			return;
		}
		line_free(input);
	}
	file_free(&editor.file);
	term_switch_to_norm();
	exit(0);
}
static void ensure_trailing_empty_line(File *file) {
	Line *line;
	if (!file) return;
	line = file->buffer.begin;
	if (!line) return;
	while (line->next) line = line->next;
	if (line->len != 0) {
		line_insert(line, NULL);
		file->buffer.num_lines++;
	}
}
