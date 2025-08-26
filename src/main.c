#include "yoc.h"
#include <string.h>
#include <stdio.h>
Editor editor;
int main(int argc, char **argv) {
    char *file_path = NULL;
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("[v%s] :: [%s] :: [%s]\n", YOC_VERSION, YOC_HASH, YOC_DATE);
            return 0;
        }
        file_path = argv[1];
    }
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
        fs_canonicalize(file_path, canonical_path, sizeof(canonical_path));
        size_t len = strlen(canonical_path);
        if (len + 1 > editor.file.cap) {
            editor.file.cap = len + 1;
            editor.file.path = (char *)xrealloc(editor.file.path, editor.file.cap);
        }
        memcpy(editor.file.path, canonical_path, len + 1);
        if (fs_exists(editor.file.path)) {
            file_load(&editor.file);
        }
    }
    term_init();
    while (1) {
        render_refresh();
        edit_process_key();
    }
    return 0;
}
