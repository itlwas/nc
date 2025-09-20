#include "yoc.h"
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <shellapi.h>
#include <wchar.h>
#endif
Editor editor;
int main(int argc, char **argv) {
    char *file_path = NULL;
    int options_ended = 0;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int file_path_owned = 0;
    int wargc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv) {
        for (int i = 1; i < wargc; i++) {
            const wchar_t *warg = wargv[i];
            int is_filepath = 1;
            if (!options_ended && warg[0] == L'-') {
                if (wcscmp(warg, L"--") == 0) {
                    options_ended = 1;
                    is_filepath = 0;
                } else if (wcscmp(warg, L"-v") == 0 || wcscmp(warg, L"--version") == 0) {
                    printf("[v%s] :: [%s] :: [%s]\n", YOC_VERSION, YOC_HASH, YOC_DATE);
                    LocalFree(wargv);
                    return 0;
                }
            }
            if (is_filepath) {
                int u8len = WideCharToMultiByte(CP_UTF8, 0, warg, -1, NULL, 0, NULL, NULL);
                if (u8len > 0) {
                    file_path = (char *)xmalloc((size_t)u8len);
                    if (WideCharToMultiByte(CP_UTF8, 0, warg, -1, file_path, u8len, NULL, NULL) != u8len) {
                        free(file_path);
                        file_path = NULL;
                    } else {
                        file_path_owned = 1;
                    }
                }
                break;
            }
        }
        LocalFree(wargv);
    }
#else
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        int is_filepath = 1;
        if (!options_ended && arg[0] == '-') {
            if (strcmp(arg, "--") == 0) {
                options_ended = 1;
                is_filepath = 0;
            } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
                printf("[v%s] :: [%s] :: [%s]\n", YOC_VERSION, YOC_HASH, YOC_DATE);
                return 0;
            }
        }
        if (is_filepath) {
            file_path = argv[i];
            break;
        }
    }
#endif
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
#ifdef _WIN32
        if (file_path_owned) {
            free(file_path);
        }
#endif
    }
    term_init();
    while (1) {
        render_refresh();
        edit_process_key();
    }
    return 0;
}
