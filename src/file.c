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
	bool_t had_trailing_newline = FALSE;
	Buffer *buf = &file->buffer;
	f = fopen(file->path, "rb");
	if (!f) die("fopen");
	buf->digest = 0;
	while ((line_len = getline(&line, &line_cap, f)) != -1) {
		had_trailing_newline = FALSE;
		if (line_len > 0 && line[line_len - 1] == '\n') {
			line_len--;
			had_trailing_newline = TRUE;
		}
		if (line_len > 0 && line[line_len - 1] == '\r') {
			line_len--;
		}
		if (first_line) {
			line_insert_strn(buf->curr, 0, (unsigned char *)line, (size_t)line_len);
			buf->curr->hash = fnv1a_hash(buf->curr->s, buf->curr->len);
			buf->digest += buf->curr->hash;
			first_line = FALSE;
		} else {
			Line *newline = line_new(buf->curr, buf->curr->next);
			buf->curr = newline;
			buf->num_lines++;
			line_insert_strn(newline, 0, (unsigned char *)line, (size_t)line_len);
			newline->hash = fnv1a_hash(newline->s, newline->len);
			buf->digest += newline->hash;
		}
	}
	if (had_trailing_newline) {
		Line *newline = line_new(buf->curr, NULL);
		buf->curr = newline;
		buf->num_lines++;
		buf->digest += newline->hash;
	}
	free(line);
	fclose(f);
	buf->curr = buf->begin;
	file->saved_digest = buf->digest;
	file->is_modified = FALSE;
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
		if (line->next) {
			fputc('\n', f);
		} else if (line->len > 0) {
			fputc('\n', f);
			line_new(line, NULL);
			file->buffer.num_lines++;
			file->buffer.digest += line->next->hash;
		}
	}
	fclose(f);
	file->saved_digest = file->buffer.digest;
	file->is_modified = FALSE;
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
		const char *name = editor.file.path[0] ? extract_filename(editor.file.path) : "[No Name]";
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
