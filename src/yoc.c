#include <locale.h>
#include <stdio.h>
#include <ctype.h>
#include "yoc.h"
struct yoc_t yoc;
static void show_usage(char *name);
void init(int argc, char **argv) {
	if (argc > 2)
		show_usage(argv[0]);
	term_init();
	setlocale(LC_ALL, "");
	get_window_size(&yoc.cols, &yoc.rows);
	yoc.rows = SCREEN_ROWS(yoc.rows);
	file_init(&yoc.file);
	yoc.top_line = yoc.file.buffer.begin;
	yoc.tabsize = get_tabsize();
	if (argc == 2) {
		size_t len = strlen(argv[1]) + 1;
		if (len > yoc.file.cap) {
			yoc.file.path = (char *)realloc(yoc.file.path, len);
			if (yoc.file.path == NULL)
				die("realloc");
			yoc.file.cap = len;
		}
		memmove(yoc.file.path, argv[1], len);
		if (is_file_exist(yoc.file.path)) {
			file_load(&yoc.file);
		}
	}
}
bool do_save(void) {
	if (yoc.file.path[0] != '\0') {
		file_save(&yoc.file);
		yoc.file.is_modified = false;
		return true;
	}
	bool retval = false;
	Line *input = line_insert(NULL, NULL);
	if (status_input(input, "Save as: ", NULL)) {
		if (input->len > 0) {
			size_t new_len = input->len;
			if (new_len + 1 > yoc.file.cap) {
				yoc.file.path = (char *)realloc(yoc.file.path, new_len + 1);
				if (yoc.file.path == NULL)
					die("realloc");
				yoc.file.cap = new_len + 1;
			}
			memcpy(yoc.file.path, input->s, new_len + 1);
			file_save(&yoc.file);
			yoc.file.is_modified = false;
			retval = true;
		}
	}
	line_free(input);
	return retval;
}
void do_quit(void) {
	if (yoc.file.is_modified) {
		Line *input = line_insert(NULL, NULL);
		char prompt[256];
		const char *name = yoc.file.path[0] ? yoc.file.path : "[No Name]";
		snprintf(prompt, sizeof(prompt), "Save changes to %s before closing? (y,n,esc): ", name);
		if (status_input(input, prompt, NULL)) {
			char answer = tolower((unsigned char)input->s[0]);
			if (answer == 'y') {
				if (!do_save()) {
					line_free(input);
					return;
				}
			} else if (answer == 'n') {
			} else {
				line_free(input);
				return;
			}
		} else {
			line_free(input);
			return;
		}
		line_free(input);
	}
	file_free(&yoc.file);
	exit(0);
}
static void show_usage(char *name) {
	printf("Usage: %s [file]\n", name);
	exit(0);
}
int main(int argc, char **argv) {
	init(argc, argv);
	for (;;) {
		refresh_screen();
		process_keypress();
	}
}