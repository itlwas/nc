#include "yoc.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
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
	FILE *f;
	char *line = NULL;
	size_t line_cap = 0;
	ssize_t line_len;
	bool_t first_line = TRUE;
	f = fopen(file->path, "rb");
	if (!f) die("fopen");
	while ((line_len = getline(&line, &line_cap, f)) != -1) {
		if (line_len > 0 && line[line_len - 1] == '\n') {
			line_len--;
		}
		if (line_len > 0 && line[line_len - 1] == '\r') {
			line_len--;
		}
		if (first_line) {
			line_insert_strn(file->buffer.curr, 0, (unsigned char *)line, (size_t)line_len);
			first_line = FALSE;
		} else {
			line_new(file->buffer.curr, file->buffer.curr->next);
			file->buffer.curr = file->buffer.curr->next;
			file->buffer.num_lines++;
			line_insert_strn(file->buffer.curr, 0, (unsigned char *)line, (size_t)line_len);
		}
	}
	free(line);
	fclose(f);
	file->buffer.curr = file->buffer.begin;
}
void file_save(File *file) {
	FILE *f = fopen(file->path, "w");
	Line *line;
	if (!f) die("fopen");
	setvbuf(f, NULL, _IOFBF, 65536);
	if (file->buffer.num_lines == 1 && file->buffer.begin->len == 0) {
		fclose(f);
		return;
	}
	for (line = file->buffer.begin; line; line = line->next) {
		fwrite(line->s, 1, line->len, f);
		if (line->next)
			fputc('\n', f);
	}
	fputc('\n', f);
	fclose(f);
}
bool_t file_save_prompt(void) {
	Line *input;
	if (editor.file.path[0] != '\0') {
		file_save(&editor.file);
		editor.file.is_modified = FALSE;
		return TRUE;
	}
	input = line_new(NULL, NULL);
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
		Line *input = line_new(NULL, NULL);
		char prompt[256];
		const char *name = editor.file.path[0] ? editor.file.path : "[No Name]";
		snprintf(prompt, sizeof(prompt), "Save changes to %s before closing? (y,n,esc): ", name);
		if (status_input(input, prompt, NULL)) {
			int answer = tolower((unsigned char)input->s[0]);
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
