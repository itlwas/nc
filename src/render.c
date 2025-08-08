#include "yoc.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
static void render_main(void);
static void render_rows(void);
static void render_status_bar(void);
static void ensure_screen_buffer(void);
static void draw_line(size_t row, const unsigned char *line, size_t len);
static char    *rowbuf                 = NULL;
static size_t   rowbuf_cap             = 0;
static char    *spaces                 = NULL;
static size_t   spaces_cap             = 0;
static size_t   lineno_pad             = 0;
static bool_t   prev_scrollbar_visible = FALSE;
static uint64_t cached_digest          = 0;
bool_t          show_line_numbers      = TRUE;

// --- Minimal syntax highlighting (terminal ANSI) ---
// We keep stored screen buffers plain. We only inject ANSI during writes.
// This ensures width calculations remain correct.

typedef enum {
    SYNTAX_NONE = 0,
    SYNTAX_CLIKE,
    SYNTAX_PY,
    SYNTAX_SHELL,
    SYNTAX_JSON,
    SYNTAX_HTML,
    SYNTAX_MD,
    SYNTAX_INI,
    SYNTAX_SQL,
} SyntaxMode;

static SyntaxMode syntax_mode = SYNTAX_NONE;

static int ends_with_ci(const char *s, const char *ext) {
    size_t ls = strlen(s), le = strlen(ext);
    if (le > ls) return 0;
    s += ls - le;
    for (size_t i = 0; i < le; ++i) {
        char a = s[i], b = ext[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int equals_ci(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    if (la != lb) return 0;
    for (size_t i = 0; i < la; ++i) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return 1;
}

static void detect_syntax_from_path(void) {
    const char *path = editor.file.path;
    const char *name = path && path[0] ? extract_filename(path) : "";
    syntax_mode = SYNTAX_NONE;
    if (name[0] == '\0') return;
    if (equals_ci(name, "makefile")) { syntax_mode = SYNTAX_INI; return; }
    if (
        ends_with_ci(name, ".c")   || ends_with_ci(name, ".h")   ||
        ends_with_ci(name, ".cpp") || ends_with_ci(name, ".hpp") ||
        ends_with_ci(name, ".cc")  || ends_with_ci(name, ".cxx") ||
        ends_with_ci(name, ".java")|| ends_with_ci(name, ".cs")  ||
        ends_with_ci(name, ".go")  || ends_with_ci(name, ".rs")
    ) { syntax_mode = SYNTAX_CLIKE; return; }
    if (ends_with_ci(name, ".js") || ends_with_ci(name, ".ts")) { syntax_mode = SYNTAX_CLIKE; return; }
    if (ends_with_ci(name, ".py")) { syntax_mode = SYNTAX_PY; return; }
    if (
        ends_with_ci(name, ".sh") || ends_with_ci(name, ".bash") ||
        ends_with_ci(name, ".zsh")|| ends_with_ci(name, ".fish")
    ) { syntax_mode = SYNTAX_SHELL; return; }
    if (ends_with_ci(name, ".json")) { syntax_mode = SYNTAX_JSON; return; }
    if (
        ends_with_ci(name, ".html") || ends_with_ci(name, ".htm") ||
        ends_with_ci(name, ".xml")  || ends_with_ci(name, ".xhtml") ||
        ends_with_ci(name, ".css")
    ) { syntax_mode = SYNTAX_HTML; return; }
    if (ends_with_ci(name, ".md") || ends_with_ci(name, ".markdown")) { syntax_mode = SYNTAX_MD; return; }
    if (
        ends_with_ci(name, ".ini") || ends_with_ci(name, ".cfg") || ends_with_ci(name, ".conf") ||
        ends_with_ci(name, ".toml")|| ends_with_ci(name, ".yaml")|| ends_with_ci(name, ".yml")
    ) { syntax_mode = SYNTAX_INI; return; }
    if (ends_with_ci(name, ".sql")) { syntax_mode = SYNTAX_SQL; return; }
}

static int is_ascii_letter(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' );
}
static int is_ascii_alnum(char c) {
    return is_ascii_letter(c) || (c >= '0' && c <= '9');
}
static int word_in_list_ci(const unsigned char *s, size_t n, const char *const *list, size_t list_sz) {
    for (size_t i = 0; i < list_sz; ++i) {
        const char *w = list[i];
        size_t wl = strlen(w);
        if (wl == n) {
            size_t j = 0; int eq = 1;
            for (; j < n; ++j) {
                char a = (char)s[j], b = w[j];
                if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
                if (a != b) { eq = 0; break; }
            }
            if (eq) return 1;
        }
    }
    return 0;
}

static void write_ansi(const char *seq) {
    term_write((const unsigned char *)seq, strlen(seq));
}

static void write_bytes(const unsigned char *s, size_t n) {
    if (n > 0) term_write(s, n);
}

static void highlight_write(const unsigned char *s, size_t len) {
    if (len == 0) return;
    // ANSI color palette
    static const char *C_RESET   = "\x1b[0m";
    static const char *C_COMMENT = "\x1b[90m"; // bright black / gray
    static const char *C_STRING  = "\x1b[32m"; // green
    static const char *C_NUMBER  = "\x1b[35m"; // magenta
    static const char *C_KEYWORD = "\x1b[36m"; // cyan
    static const char *C_CONST   = "\x1b[33m"; // yellow
    static const char *C_TAG     = "\x1b[34m"; // blue

    // Keyword lists (minimal but popular)
    static const char *const KW_CLIKE[] = {
        "if","else","for","while","switch","case","break","continue","return","struct","union","enum","typedef","static","const","volatile","extern","inline","sizeof","void","char","short","int","long","float","double","signed","unsigned","class","public","private","protected","template","typename","using","namespace","virtual","override","final","try","catch","throw","new","delete","this","operator","import","export","from","package","interface","extends","implements","function","const","let","var","async","await","yield"
    };
    static const char *const KW_PY[] = {
        "def","class","return","if","elif","else","for","while","break","continue","try","except","finally","with","as","lambda","import","from","global","nonlocal","pass","raise","yield","assert","del","True","False","None"
    };
    static const char *const KW_SH[] = {
        "if","then","else","elif","fi","for","in","do","done","case","esac","while","until","function","select","time","coproc","local","export","readonly","return"
    };
    static const char *const KW_SQL[] = {
        "select","from","where","group","by","order","insert","into","update","delete","create","table","alter","drop","join","left","right","inner","outer","on","as","and","or","not","null","is","like","in","exists","limit","offset","union","distinct","having","case","when","then","end"
    };

    const char *current = C_RESET;
    write_ansi(C_RESET);

    size_t i = 0;
    int in_string = 0; // quote char stored in in_string (' or " or `)

    while (i < len) {
        unsigned char c = s[i];

        // Simple string handling (supports escapes)
        if (!in_string && (c == '"' || c == '\'' || c == '`')) {
            in_string = (int)c;
            if (current != C_STRING) { write_ansi(C_STRING); current = C_STRING; }
            write_bytes(&s[i], 1);
            i += 1;
            continue;
        }
        if (in_string) {
            if (c == '\\' && i + 1 < len) {
                // escape sequence
                write_bytes(&s[i], 2);
                i += 2;
                continue;
            }
            write_bytes(&s[i], 1);
            if (c == (unsigned char)in_string) {
                in_string = 0;
                if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
            }
            i += 1;
            continue;
        }

        // Comments (single-line)
        if (syntax_mode == SYNTAX_CLIKE) {
            if (i + 1 < len && s[i] == '/' && s[i + 1] == '/') {
                if (current != C_COMMENT) { write_ansi(C_COMMENT); current = C_COMMENT; }
                write_bytes(&s[i], len - i);
                i = len;
                break;
            }
        }
        if (syntax_mode == SYNTAX_PY || syntax_mode == SYNTAX_SHELL || syntax_mode == SYNTAX_INI) {
            if (s[i] == '#') {
                if (current != C_COMMENT) { write_ansi(C_COMMENT); current = C_COMMENT; }
                write_bytes(&s[i], len - i);
                i = len;
                break;
            }
        }
        if (syntax_mode == SYNTAX_SQL) {
            if (i + 1 < len && s[i] == '-' && s[i + 1] == '-') {
                if (current != C_COMMENT) { write_ansi(C_COMMENT); current = C_COMMENT; }
                write_bytes(&s[i], len - i);
                i = len;
                break;
            }
        }
        if (syntax_mode == SYNTAX_HTML) {
            if (i + 3 < len && s[i] == '<' && s[i + 1] == '!' && s[i + 2] == '-' && s[i + 3] == '-') {
                if (current != C_COMMENT) { write_ansi(C_COMMENT); current = C_COMMENT; }
                write_bytes(&s[i], len - i);
                i = len;
                break;
            }
        }

        // Numbers
        if ((c >= '0' && c <= '9') || (c == '.' && i + 1 < len && s[i + 1] >= '0' && s[i + 1] <= '9')) {
            size_t start = i;
            if (c == '0' && i + 1 < len && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
                i += 2;
                while (i < len) {
                    unsigned char d = s[i];
                    if ((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') || (d >= 'A' && d <= 'F')) {
                        ++i;
                    } else {
                        break;
                    }
                }
            } else {
                ++i;
                while (i < len && ((s[i] >= '0' && s[i] <= '9') || s[i] == '_' )) ++i;
                if (i < len && (s[i] == '.' || s[i] == 'e' || s[i] == 'E')) {
                    ++i; while (i < len && ((s[i] >= '0' && s[i] <= '9') || s[i] == '+' || s[i] == '-' )) ++i;
                }
            }
            if (current != C_NUMBER) { write_ansi(C_NUMBER); current = C_NUMBER; }
            write_bytes(&s[start], i - start);
            if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
            continue;
        }

        // Words / keywords / booleans / null
        if (is_ascii_letter((char)c)) {
            size_t start = i; ++i;
            while (i < len && is_ascii_alnum((char)s[i])) ++i;
            size_t n = i - start;
            int colored = 0;

            // JSON specific: keys are in quotes, handled as strings. Highlight booleans/null.
            if (
                (syntax_mode == SYNTAX_JSON || syntax_mode == SYNTAX_INI || syntax_mode == SYNTAX_PY || syntax_mode == SYNTAX_SHELL || syntax_mode == SYNTAX_SQL || syntax_mode == SYNTAX_CLIKE) &&
                (word_in_list_ci(s + start, n, (const char*[]){"true","false","null"}, 3))
            ) {
                if (current != C_CONST) { write_ansi(C_CONST); current = C_CONST; }
                write_bytes(&s[start], n);
                if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                colored = 1;
            } else if (syntax_mode == SYNTAX_CLIKE && word_in_list_ci(s + start, n, KW_CLIKE, sizeof(KW_CLIKE)/sizeof(KW_CLIKE[0]))) {
                if (current != C_KEYWORD) { write_ansi(C_KEYWORD); current = C_KEYWORD; }
                write_bytes(&s[start], n);
                if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                colored = 1;
            } else if (syntax_mode == SYNTAX_PY && word_in_list_ci(s + start, n, KW_PY, sizeof(KW_PY)/sizeof(KW_PY[0]))) {
                if (current != C_KEYWORD) { write_ansi(C_KEYWORD); current = C_KEYWORD; }
                write_bytes(&s[start], n);
                if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                colored = 1;
            } else if (syntax_mode == SYNTAX_SHELL && word_in_list_ci(s + start, n, KW_SH, sizeof(KW_SH)/sizeof(KW_SH[0]))) {
                if (current != C_KEYWORD) { write_ansi(C_KEYWORD); current = C_KEYWORD; }
                write_bytes(&s[start], n);
                if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                colored = 1;
            } else if (syntax_mode == SYNTAX_SQL && word_in_list_ci(s + start, n, KW_SQL, sizeof(KW_SQL)/sizeof(KW_SQL[0]))) {
                if (current != C_KEYWORD) { write_ansi(C_KEYWORD); current = C_KEYWORD; }
                write_bytes(&s[start], n);
                if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                colored = 1;
            } else if (syntax_mode == SYNTAX_HTML) {
                // Inside tags, color tag/attr names lightly
                // We don't know context from rowbuf; just color words immediately after '<' or before '='
                size_t prev = start > 0 ? start - 1 : start;
                if ((prev < start && s[prev] == '<') || (i < len && s[i] == '=')) {
                    if (current != C_TAG) { write_ansi(C_TAG); current = C_TAG; }
                    write_bytes(&s[start], n);
                    if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                    colored = 1;
                }
            } else if (syntax_mode == SYNTAX_INI) {
                // Keys before '=' or ':'
                if (i < len && (s[i] == '=' || s[i] == ':')) {
                    if (current != C_KEYWORD) { write_ansi(C_KEYWORD); current = C_KEYWORD; }
                    write_bytes(&s[start], n);
                    if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
                    colored = 1;
                }
            }

            if (!colored) {
                write_bytes(&s[start], n);
            }
            continue;
        }

        // HTML tags and angle brackets
        if (syntax_mode == SYNTAX_HTML && (c == '<' || c == '>')) {
            if (current != C_TAG) { write_ansi(C_TAG); current = C_TAG; }
            write_bytes(&s[i], 1);
            if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
            i += 1;
            continue;
        }

        // Default: passthrough
        if (current != C_RESET) { write_ansi(C_RESET); current = C_RESET; }
        write_bytes(&s[i], 1);
        i += 1;
    }

    if (current != C_RESET) write_ansi(C_RESET);
}

void render_refresh(void) {
    size_t digits = 1;
    term_get_win_size(&editor.cols, &editor.rows);
    editor.rows = SCREEN_ROWS(editor.rows);
    for (size_t n = editor.file.buffer.num_lines; n >= 10; n /= 10) {
        ++digits;
    }
    lineno_pad = show_line_numbers ? digits + 2 : 0;
    ensure_screen_buffer();
    render_scroll();
    // Detect syntax on each refresh (cheap and robust when file changes)
    detect_syntax_from_path();
    static uint64_t last_state = 0;
    uint64_t state = editor.file.buffer.digest
                   ^ (uint64_t)(uintptr_t)editor.top_line
                   ^ ((uint64_t)editor.window.x << 1)
                   ^ ((uint64_t)editor.window.y << 17)
                   ^ ((uint64_t)editor.cols << 33)
                   ^ ((uint64_t)editor.rows << 49)
                   ^ ((uint64_t)lineno_pad << 57);
    if (state != last_state) {
        render_main();
        last_state = state;
    }
    render_status_bar();
    term_set_cursor(
        (show_line_numbers ? lineno_pad : 0) + editor.file.cursor.rx - editor.window.x,
        editor.file.cursor.y - editor.window.y
    );
    term_show_cursor();
}
void render_scroll(void) {
    bool_t buffer_changed = (editor.file.buffer.digest != cached_digest);
    if (editor.top_line == NULL) {
        editor.top_line = editor.file.buffer.begin;
    }
    size_t old_window_y = editor.window.y;
    size_t margin = VSCROLL_MARGIN;
    if (editor.rows <= VSCROLL_MARGIN * 2) {
        margin = 0;
    }
    editor.file.cursor.rx = 0;
    if (editor.file.cursor.y < editor.file.buffer.num_lines) {
        editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    }
    if (editor.file.cursor.y < editor.window.y + margin) {
        editor.window.y = (editor.file.cursor.y < margin) ? 0 : editor.file.cursor.y - margin;
    } else if (editor.file.cursor.y >= editor.window.y + editor.rows - margin) {
        editor.window.y = editor.file.cursor.y - editor.rows + margin + 1;
    }
    if (editor.file.cursor.rx < editor.window.x + HSCROLL_MARGIN) {
        editor.window.x = (editor.file.cursor.rx < HSCROLL_MARGIN) ? 0 : editor.file.cursor.rx - HSCROLL_MARGIN;
    } else if (
        editor.file.cursor.rx >= editor.window.x + editor.cols - (show_line_numbers ? lineno_pad : 0) - HSCROLL_MARGIN
    ) {
        editor.window.x =
            editor.file.cursor.rx - (editor.cols - (show_line_numbers ? lineno_pad : 0)) + HSCROLL_MARGIN + 1;
    }
    if (
        editor.file.buffer.curr != editor.top_line ||
        old_window_y != editor.window.y ||
        (buffer_changed && old_window_y == editor.window.y)
    ) {
        long diff = (long)editor.window.y - (long)old_window_y;
        while (diff > 0 && editor.top_line && editor.top_line->next) {
            editor.top_line = editor.top_line->next;
            --diff;
        }
        while (diff < 0 && editor.top_line && editor.top_line->prev) {
            editor.top_line = editor.top_line->prev;
            ++diff;
        }
        if (diff != 0 || buffer_changed) {
            Line *line = editor.file.buffer.begin;
            size_t y = 0;
            while (y < editor.window.y && line->next) {
                line = line->next;
                ++y;
            }
            editor.top_line = line;
        }
    }
    cached_digest = editor.file.buffer.digest;
}
static void render_main(void) {
    term_hide_cursor();
    term_set_cursor(0, 0);
    render_rows();
}
static void render_rows(void) {
    Line   *line       = editor.top_line;
    size_t  digits     = lineno_pad ? lineno_pad - 2 : 1;
    bool_t  scrollbar  = (editor.file.buffer.num_lines > editor.rows);
    size_t  bar_height = 0, bar_start = 0;
    size_t  avail_cols;
    if (scrollbar) {
        size_t total_lines   = editor.file.buffer.num_lines;
        bar_height           = (editor.rows * editor.rows) / total_lines;
        if (bar_height == 0) bar_height = 1;
        size_t scroll_pos    = editor.window.y;
        size_t max_scroll_pos = total_lines > editor.rows ? total_lines - editor.rows : 0;
        if (scroll_pos > max_scroll_pos) scroll_pos = max_scroll_pos;
        if (max_scroll_pos > 0) {
            bar_start = (scroll_pos * (editor.rows - bar_height)) / max_scroll_pos;
        } else {
            bar_start = 0;
        }
    }
    if (!scrollbar && prev_scrollbar_visible) {
        term_clear_screen();
        if (editor.screen_lens) {
            for (size_t r = 0; r < editor.screen_rows; ++r) {
                editor.screen_lens[r] = 0;
            }
        }
    }
    if (!rowbuf) {
        ensure_screen_buffer();
    }
    avail_cols = editor.cols - (scrollbar ? 1 : 0);
    for (size_t y = 0; y < editor.rows; ++y) {
        size_t row_len = 0;
        rowbuf[0] = '\0';
        if (line && show_line_numbers) {
            size_t lnum = editor.window.y + y + 1;
            row_len = (size_t)snprintf(
                rowbuf, rowbuf_cap, " %*lu ", (int)digits, (unsigned long)lnum
            );
            if (row_len >= rowbuf_cap) {
                row_len = rowbuf_cap - 1;
            }
        }
        if (!line) {
            if (
                y == editor.rows / 3 &&
                editor.file.buffer.num_lines == 1 &&
                editor.file.buffer.begin->len == 0
            ) {
                char   msg[32];
                snprintf(msg, sizeof(msg), "yoc ~ %s", YOC_VERSION);
                size_t welcomelen = strlen(msg);
                size_t padding    = (avail_cols > welcomelen) ? (avail_cols - welcomelen) / 2 : 0;
                size_t pos        = 0;
                if (padding) {
                    rowbuf[pos++] = '~';
                    --padding;
                }
                while (padding-- > 0 && pos < avail_cols) {
                    rowbuf[pos++] = ' ';
                }
                if (pos + welcomelen > avail_cols) {
                    welcomelen = (size_t)(avail_cols - pos);
                }
                memcpy(rowbuf + pos, msg, welcomelen);
                pos += welcomelen;
                rowbuf[pos] = '\0';
                row_len = pos;
            } else {
                rowbuf[0] = '~';
                rowbuf[1] = '\0';
                row_len   = 1;
            }
        } else {
            const unsigned char *s = line->s;
            size_t i, width, pos = row_len;
            size_t text_cols = avail_cols - (show_line_numbers ? lineno_pad : 0);
            if (editor.window.x > 0) {
                i     = width_to_length(s, editor.window.x);
                width = editor.window.x;
            } else {
                i     = 0;
                width = 0;
            }
            while (i < line->len && width < editor.window.x + text_cols) {
                unsigned char c        = s[i];
                size_t        char_len = 1;
                if (c == '\t') {
                    size_t spaces_to_add = editor.tabsize - (width % editor.tabsize);
                    for (size_t k = 0; k < spaces_to_add; ++k) {
                        if (width >= editor.window.x && pos < rowbuf_cap - 1) {
                            rowbuf[pos++] = ' ';
                        }
                        ++width;
                    }
                    i += 1;
                    continue;
                }
                if (LIKELY(c < 0x80u)) {
                    if (c >= 0x20u && c != 0x7Fu) {
                        if (width + 1 > editor.window.x + text_cols) break;
                        if (width >= editor.window.x && pos < rowbuf_cap - 1) {
                            rowbuf[pos++] = (char)c;
                        }
                        ++width;
                    }
                    i += 1;
                    continue;
                }
                if (!is_continuation_byte(c)) {
                    size_t char_width;
                    char_len = utf8_len(c);
                    if (char_len == 0 || i + char_len > line->len) char_len = 1;
                    char_width = char_display_width(s + i);
                    if (char_width == 0) {
                        i += char_len;
                        continue;
                    }
                    if (width + char_width > editor.window.x + text_cols) break;
                    if (width >= editor.window.x && pos + char_len < rowbuf_cap) {
                        memcpy(rowbuf + pos, s + i, char_len);
                        pos += char_len;
                    }
                    width += char_width;
                }
                i += char_len;
            }
            rowbuf[pos] = '\0';
            row_len     = pos;
            line        = line->next;
        }
        draw_line(y, (unsigned char *)rowbuf, row_len);
        if (scrollbar) {
            term_set_cursor(editor.cols - 1, y);
            if (y >= bar_start && y < bar_start + bar_height) {
                term_write((const unsigned char *)"|", 1);
            } else {
                term_write((const unsigned char *)" ", 1);
            }
        }
    }
    prev_scrollbar_visible = scrollbar;
}
static void render_status_bar(void) {
    term_set_cursor(0, editor.rows);
    status_print();
}
static void ensure_screen_buffer(void) {
    const size_t rows_needed = editor.rows + 1;
    bool_t size_changed =
        !(editor.screen_lines && editor.screen_rows == rows_needed && editor.screen_cols == editor.cols);
    if (!size_changed) return;
    term_clear_screen();
    if (editor.screen_lines) {
        for (size_t r = 0; r < editor.screen_rows; ++r) {
            free(editor.screen_lines[r]);
        }
        free(editor.screen_lines);
    }
    if (editor.screen_lens) {
        free(editor.screen_lens);
    }
    editor.screen_lines = (char **)xmalloc(rows_needed * sizeof(char *));
    editor.screen_lens  = (size_t *)xmalloc(rows_needed * sizeof(size_t));
    size_t line_cap;
    if (UNLIKELY(editor.cols > (SIZE_MAX - 1) / MAXCHARLEN)) {
        die("terminal width too large");
    }
    line_cap = editor.cols * MAXCHARLEN + 1;
    for (size_t r = 0; r < rows_needed; ++r) {
        editor.screen_lines[r] = (char *)xmalloc(line_cap);
        editor.screen_lines[r][0] = '\0';
        editor.screen_lens[r]     = 0;
    }
    editor.screen_rows = rows_needed;
    editor.screen_cols = editor.cols;
    if (rowbuf_cap < line_cap) {
        rowbuf_cap = line_cap;
        rowbuf     = (char *)xrealloc(rowbuf, rowbuf_cap);
    }
    if (spaces_cap < editor.cols + 1) {
        spaces_cap = editor.cols + 1;
        spaces     = (char *)xrealloc(spaces, spaces_cap);
        memset(spaces, ' ', editor.cols);
        spaces[editor.cols] = '\0';
    }
}
static void draw_line(size_t row, const unsigned char *line, size_t len) {
    size_t prev_len, common_len;
    size_t common_width, new_width, old_width;
    if (row >= editor.screen_rows) {
        return;
    }
    prev_len = editor.screen_lens[row];
    // If highlighting is enabled, force redraw of the line content so ANSI can be emitted.
    if (syntax_mode == SYNTAX_NONE) {
        if (prev_len == len && memcmp(editor.screen_lines[row], line, len) == 0) {
            return;
        }
    }
    common_len = 0;
    while (
        syntax_mode == SYNTAX_NONE &&
        common_len < prev_len &&
        common_len < len &&
        editor.screen_lines[row][common_len] == line[common_len]
    ) {
        common_len++;
    }
    if (syntax_mode == SYNTAX_NONE) {
        while (common_len > 0 && is_continuation_byte(line[common_len])) {
            common_len--;
        }
    } else {
        // Force full rewrite when highlighting to ensure correct colorization
        common_len = 0;
    }
    common_width = length_to_width((unsigned char *)editor.screen_lines[row], common_len);
    term_set_cursor(common_width, row);
    if (syntax_mode == SYNTAX_NONE) {
        if (common_len < len) {
            term_write(line + common_len, len - common_len);
        }
    } else {
        // Colored write of the whole (or remaining) line segment
        highlight_write(line + common_len, len - common_len);
    }
    new_width = common_width + length_to_width(line + common_len, len - common_len);
    old_width = length_to_width((unsigned char *)editor.screen_lines[row], prev_len);
    if (new_width < old_width) {
        term_write((unsigned char *)spaces, old_width - new_width);
    }
    if (len > editor.cols * MAXCHARLEN) {
        len = editor.cols * MAXCHARLEN;
    }
    memcpy(editor.screen_lines[row], line, len);
    editor.screen_lines[row][len] = '\0';
    editor.screen_lens[row]       = len;
}
void render_free(void) {
    if (rowbuf) {
        free(rowbuf);
        rowbuf     = NULL;
        rowbuf_cap = 0;
    }
    if (spaces) {
        free(spaces);
        spaces     = NULL;
        spaces_cap = 0;
    }
    if (editor.screen_lines) {
        for (size_t r = 0; r < editor.screen_rows; ++r) {
            free(editor.screen_lines[r]);
        }
        free(editor.screen_lines);
        editor.screen_lines = NULL;
    }
    if (editor.screen_lens) {
        free(editor.screen_lens);
        editor.screen_lens = NULL;
    }
    editor.screen_rows = 0;
    editor.screen_cols = 0;
}
