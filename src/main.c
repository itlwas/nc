#include "yoc.h"
#include <string.h>
Editor editor;
static void init(char *file_path);
int main(int argc, char **argv) {
	init(argc > 1 ? argv[1] : NULL);
	while (1) {
		render_refresh();
		edit_process_key();
	}
	return 0;
}
static void init(char *file_path) {
	editor.tabsize = 4;
	editor.window.x = 0;
	editor.window.y = 0;
	editor.top_line = NULL;
	editor.rows = 0;
	editor.cols = 0;
	editor.screen_lines = NULL;
	editor.screen_rows = 0;
	editor.screen_cols = 0;
	editor.screen_lens = NULL;
	file_init(&editor.file);
	if (file_path) {
		char canonical_path[4096];
		size_t len;
		fs_canonicalize(file_path, canonical_path, sizeof(canonical_path));
		len = strlen(canonical_path);
		if (len + 1 > editor.file.cap) {
			editor.file.cap = len + 1;
			editor.file.path = (char *)xrealloc(editor.file.path, editor.file.cap);
		}
		memcpy(editor.file.path, canonical_path, len + 1);
		if (fs_exists(editor.file.path)) {
			file_load(&editor.file);
			editor.file.is_modified = FALSE;
		}
	}
	term_init();
}
