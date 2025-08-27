#include "yoc.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#define PREFERRED_COL_UNSET ((size_t)-1)
static size_t desired_rx = PREFERRED_COL_UNSET;
static void delete_char(void);
static void delete_empty_line(void);
static void break_line(void);
static void join_with_prev_line(void);
static bool_t is_blank(Line *line);
static void pre_line_change(Line *line);
static void post_line_change(Line *line);
static void edit_duplicate_line(void);
static void edit_goto_line(void);
static void edit_delete_forward(void);
static void edit_insert_n(const unsigned char *s, size_t s_len);
void edit_move_home(void) {
    editor.file.cursor.x = 0;
    editor.file.cursor.rx = 0;
    desired_rx = 0;
}
void edit_move_end(void) {
    editor.file.cursor.x = line_get_mblen(editor.file.buffer.curr);
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
void edit_move_up(void) {
    if (editor.file.cursor.y > 0) {
        editor.file.buffer.curr = editor.file.buffer.curr->prev;
        --editor.file.cursor.y;
        editor.file.cursor.x = rx_to_x(editor.file.buffer.curr, desired_rx == PREFERRED_COL_UNSET ? editor.file.cursor.rx : desired_rx);
        edit_fix_cursor_x();
        editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
        if (desired_rx == PREFERRED_COL_UNSET) desired_rx = editor.file.cursor.rx;
    }
}
void edit_move_down(void) {
    if (editor.file.cursor.y < editor.file.buffer.num_lines - 1) {
        editor.file.buffer.curr = editor.file.buffer.curr->next;
        ++editor.file.cursor.y;
        editor.file.cursor.x = rx_to_x(editor.file.buffer.curr, desired_rx == PREFERRED_COL_UNSET ? editor.file.cursor.rx : desired_rx);
        edit_fix_cursor_x();
        editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
        if (desired_rx == PREFERRED_COL_UNSET) desired_rx = editor.file.cursor.rx;
    }
}
void edit_move_left(void) {
    if (editor.file.cursor.x > 0) {
        --editor.file.cursor.x;
    } else if (editor.file.cursor.y > 0) {
        edit_move_up();
        edit_move_end();
    }
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
void edit_move_right(void) {
    if (editor.file.cursor.x < line_get_mblen(editor.file.buffer.curr)) {
        ++editor.file.cursor.x;
    } else if (editor.file.buffer.curr->next) {
        editor.file.buffer.curr = editor.file.buffer.curr->next;
        ++editor.file.cursor.y;
        editor.file.cursor.x = 0;
    }
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
void edit_move_prev_word(void) {
    size_t pos = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
    bool_t seen_a_word = FALSE;
    bool_t step_forward = FALSE;
    for (;;) {
        if (pos == 0) {
            if (editor.file.buffer.curr->prev == NULL) break;
            editor.file.buffer.curr = editor.file.buffer.curr->prev;
            pos = editor.file.buffer.curr->len;
            --editor.file.cursor.y;
        }
        pos = move_mbleft(editor.file.buffer.curr->s, pos);
        if (is_alnum_mbchar(editor.file.buffer.curr->s + pos)) {
            seen_a_word = TRUE;
            if (pos == 0) break;
        } else if (seen_a_word) {
            step_forward = TRUE;
            break;
        }
    }
    if (step_forward) {
        pos = move_mbright(editor.file.buffer.curr->s, pos);
    }
    editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, pos);
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
void edit_move_next_word(void) {
    size_t pos = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
    bool_t started_on_word = is_alnum_mbchar(editor.file.buffer.curr->s + pos);
    bool_t seen_space = !started_on_word;
    for (;;) {
        if (editor.file.buffer.curr->s[pos] == '\0') {
            if (editor.file.buffer.curr->next == NULL) break;
            editor.file.buffer.curr = editor.file.buffer.curr->next;
            ++editor.file.cursor.y;
            seen_space = TRUE;
            pos = 0;
        } else {
            pos = move_mbright(editor.file.buffer.curr->s, pos);
        }
        if (!is_alnum_mbchar(editor.file.buffer.curr->s + pos)) {
            seen_space = TRUE;
        } else if (seen_space) {
            break;
        }
    }
    editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, pos);
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
void edit_move_pgup(void) {
    for (size_t y = editor.rows; y--;) {
        edit_move_up();
    }
    edit_fix_cursor_x();
    desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_pgdown(void) {
    size_t y = editor.rows;
    while (y-- && editor.file.cursor.y < editor.file.buffer.num_lines - 1) {
        edit_move_down();
    }
    edit_fix_cursor_x();
    desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_top(void) {
    editor.file.cursor.x = 0;
    editor.file.cursor.y = 0;
    editor.file.buffer.curr = editor.file.buffer.begin;
    desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_bottom(void) {
    while (editor.file.buffer.curr->next) {
        ++editor.file.cursor.y;
        editor.file.buffer.curr = editor.file.buffer.curr->next;
    }
    edit_move_end();
    desired_rx = PREFERRED_COL_UNSET;
}
void edit_fix_cursor_x(void) {
    size_t len = line_get_mblen(editor.file.buffer.curr);
    if (editor.file.cursor.x > len) {
        editor.file.cursor.x = len;
    }
}
void edit_insert(const unsigned char *s) {
    edit_insert_n(s, strlen((const char *)s));
}
void edit_backspace(void) {
    Line *line = editor.file.buffer.curr;
    if (editor.file.cursor.x > 0 && line->len > 0) {
        size_t first_nb_idx = find_first_nonblank(line->s);
        size_t first_nb_mb = index_to_mbnum(line->s, first_nb_idx);
        if (editor.file.cursor.x <= first_nb_mb) {
            size_t cur_rx = x_to_rx(line, editor.file.cursor.x);
            size_t target_rx;
            if (cur_rx == 0) {
                target_rx = 0;
            } else {
                target_rx = ((cur_rx - 1) / editor.tabsize) * editor.tabsize;
            }
            size_t target_x = rx_to_x(line, target_rx);
            if (target_x < editor.file.cursor.x) {
                size_t start_idx = mbnum_to_index(line->s, target_x);
                size_t end_idx = mbnum_to_index(line->s, editor.file.cursor.x);
                size_t del_len = end_idx - start_idx;
                if (del_len > 0) {
                    pre_line_change(line);
                    line_del_str(line, start_idx, del_len);
                    post_line_change(line);
                    editor.file.cursor.x = target_x;
                } else {
                    delete_char();
                }
            } else {
                delete_char();
            }
        } else {
            delete_char();
        }
    } else if (editor.file.cursor.x == 0) {
        if (line->len == 0 && line->prev) {
            delete_empty_line();
        } else if (line->len > 0 && line->prev) {
            join_with_prev_line();
        }
    }
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
void edit_enter(void) {
    Line *prev_line = editor.file.buffer.curr;
    pre_line_change(prev_line);
    line_new(prev_line, prev_line->next);
    editor.file.buffer.num_lines++;
    editor.file.buffer.digest += prev_line->next->hash;
    if (editor.file.cursor.x < line_get_mblen(prev_line)) {
        break_line();
    }
    post_line_change(prev_line);
    editor.file.buffer.curr = prev_line->next;
    ++editor.file.cursor.y;
    size_t indent_end = find_first_nonblank(prev_line->s);
    size_t cursor_pos_bytes = mbnum_to_index(prev_line->s, editor.file.cursor.x);
    if (cursor_pos_bytes < indent_end) {
        indent_end = cursor_pos_bytes;
    }
    if (indent_end > 0) {
        pre_line_change(editor.file.buffer.curr);
        line_insert_strn(editor.file.buffer.curr, 0, prev_line->s, indent_end);
        post_line_change(editor.file.buffer.curr);
    }
    editor.file.cursor.x = index_to_mbnum(editor.file.buffer.curr->s, indent_end);
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
    editor.file.is_modified = (editor.file.buffer.digest != editor.file.saved_digest);
}
void edit_process_key(void) {
    int special_key;
    unsigned char *s;
    size_t len = term_read(&s, &special_key);
    if (len != 0) {
        edit_insert_n(s, len);
    } else {
        switch (special_key) {
            case BACKSPACE: edit_backspace(); break;
            case TAB: edit_insert_n((unsigned char*)"\t", 1); break;
            case ENTER: edit_enter(); break;
            case DEL: edit_delete_forward(); break;
            case HOME: edit_move_home(); break;
            case END: edit_move_end(); break;
            case PAGE_UP: edit_move_pgup(); break;
            case PAGE_DOWN: edit_move_pgdown(); break;
            case ARROW_UP: edit_move_up(); break;
            case ARROW_DOWN: edit_move_down(); break;
            case ARROW_LEFT: edit_move_left(); break;
            case ARROW_RIGHT: edit_move_right(); break;
            case CTRL_HOME: edit_move_top(); break;
            case CTRL_END: edit_move_bottom(); break;
            case CTRL_ARROW_UP: edit_move_prev_para(); break;
            case CTRL_ARROW_DOWN: edit_move_next_para(); break;
            case CTRL_ARROW_LEFT: edit_move_prev_word(); break;
            case CTRL_ARROW_RIGHT: edit_move_next_word(); break;
            case CTRL_KEY('d'): edit_duplicate_line(); break;
            case CTRL_KEY('f'): find_start(); break;
            case CTRL_KEY('g'): edit_goto_line(); break;
            case CTRL_KEY('o'): file_open_prompt(); break;
            case CTRL_KEY('q'): file_quit_prompt(); break;
            case CTRL_KEY('r'): show_line_numbers = !show_line_numbers; break;
            case CTRL_KEY('s'): file_save_prompt(); break;
        }
    }
    render_scroll();
}
void edit_move_prev_para(void) {
    if (editor.file.cursor.y == 0) {
        return;
    }
    Line *line = editor.file.buffer.curr;
    size_t y = editor.file.cursor.y;
    while (y > 0 && is_blank(line)) {
        line = line->prev;
        --y;
    }
    while (y > 0 && line->prev && !is_blank(line->prev)) {
        line = line->prev;
        --y;
    }
    editor.file.buffer.curr = line;
    editor.file.cursor.y = y;
    edit_move_home();
    desired_rx = PREFERRED_COL_UNSET;
}
void edit_move_next_para(void) {
    if (editor.file.cursor.y >= editor.file.buffer.num_lines - 1) {
        return;
    }
    Line *line = editor.file.buffer.curr;
    size_t y = editor.file.cursor.y;
    while (y < editor.file.buffer.num_lines - 1 && !is_blank(line)) {
        line = line->next;
        ++y;
    }
    while (y < editor.file.buffer.num_lines - 1 && is_blank(line)) {
        line = line->next;
        ++y;
    }
    editor.file.buffer.curr = line;
    editor.file.cursor.y = y;
    edit_move_home();
    desired_rx = PREFERRED_COL_UNSET;
}
static void edit_insert_n(const unsigned char *s, size_t s_len) {
    if (s_len == 0) return;
    for (size_t i = 0; i < s_len; ++i) {
        if (s[i] == '\n' || s[i] == '\r') {
            size_t nl_len = 1;
            if (s[i] == '\r' && i + 1 < s_len && s[i + 1] == '\n') {
                nl_len = 2;
            }
            edit_insert_n(s, i);
            edit_enter();
            if (i + nl_len < s_len) {
                edit_insert_n(s + i + nl_len, s_len - i - nl_len);
            }
            return;
        }
    }
    pre_line_change(editor.file.buffer.curr);
    line_insert_strn(
        editor.file.buffer.curr,
        mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x),
        s, s_len
    );
    post_line_change(editor.file.buffer.curr);
    editor.file.cursor.x += index_to_mbnum(s, s_len);
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
static void delete_char(void) {
    if (editor.file.cursor.x == 0) {
        return;
    }
    pre_line_change(editor.file.buffer.curr);
    size_t start = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x - 1);
    size_t char_len = utf8_len(editor.file.buffer.curr->s[start]);
    if (char_len == 0 || start + char_len > editor.file.buffer.curr->len) {
        char_len = 1;
    }
    line_del_str(editor.file.buffer.curr, start, char_len);
    post_line_change(editor.file.buffer.curr);
    --editor.file.cursor.x;
    edit_fix_cursor_x();
}
static void delete_empty_line(void) {
    editor.file.buffer.curr = editor.file.buffer.curr->prev;
    editor.file.cursor.x = line_get_mblen(editor.file.buffer.curr);
    --editor.file.cursor.y;
    buf_del_line(&editor.file.buffer, editor.file.buffer.curr->next);
    editor.file.is_modified = (editor.file.buffer.digest != editor.file.saved_digest);
}
static void break_line(void) {
    size_t at = mbnum_to_index(editor.file.buffer.curr->s, editor.file.cursor.x);
    size_t len_to_move = editor.file.buffer.curr->len - at;
    if (len_to_move > 0) {
        pre_line_change(editor.file.buffer.curr->next);
        line_insert_strn(editor.file.buffer.curr->next, 0, editor.file.buffer.curr->s + at, len_to_move);
        post_line_change(editor.file.buffer.curr->next);
    }
    line_del_str(editor.file.buffer.curr, at, len_to_move);
}
static void join_with_prev_line(void) {
    Line *current_line = editor.file.buffer.curr;
    Line *prev_line = current_line->prev;
    pre_line_change(prev_line);
    editor.file.cursor.x = line_get_mblen(prev_line);
    line_insert_strn(prev_line, prev_line->len, current_line->s, current_line->len);
    post_line_change(prev_line);
    editor.file.buffer.curr = prev_line;
    --editor.file.cursor.y;
    buf_del_line(&editor.file.buffer, current_line);
    editor.file.is_modified = (editor.file.buffer.digest != editor.file.saved_digest);
}
static void edit_delete_forward(void) {
    Line *line = editor.file.buffer.curr;
    if (editor.file.cursor.x < line_get_mblen(line)) {
        pre_line_change(line);
        size_t start = mbnum_to_index(line->s, editor.file.cursor.x);
        size_t char_len = utf8_len(line->s[start]);
        if (char_len == 0 || start + char_len > line->len) {
            char_len = 1;
        }
        line_del_str(line, start, char_len);
        post_line_change(line);
    } else if (line->next) {
        Line *next = line->next;
        pre_line_change(line);
        line_insert_strn(line, line->len, next->s, next->len);
        post_line_change(line);
        buf_del_line(&editor.file.buffer, next);
    }
    edit_fix_cursor_x();
    editor.file.cursor.rx = x_to_rx(line, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
}
static void edit_duplicate_line(void) {
    Line *orig = editor.file.buffer.curr;
    Line *dup = line_new(orig, orig->next);
    if (orig->len > 0) {
        size_t required = orig->len + 1;
        if (required > dup->cap) {
            if (dup->s == dup->inline_space) {
                dup->s = (unsigned char *)xmalloc(required);
            } else {
                dup->s = (unsigned char *)xrealloc(dup->s, required);
            }
            dup->cap = required;
        }
        memcpy(dup->s, orig->s, orig->len);
        dup->len = orig->len;
        dup->s[dup->len] = '\0';
    }
    dup->width = orig->width;
    dup->mb_len = orig->mb_len;
    editor.file.buffer.num_lines++;
    dup->hash = fnv1a_hash(dup->s, dup->len);
    editor.file.buffer.digest += dup->hash;
    editor.file.buffer.curr = dup;
    ++editor.file.cursor.y;
    edit_fix_cursor_x();
    editor.file.cursor.rx = x_to_rx(dup, editor.file.cursor.x);
    desired_rx = editor.file.cursor.rx;
    editor.file.is_modified = TRUE;
}
static void edit_goto_line(void) {
    Line *input = line_new(NULL, NULL);
    if (!status_input(input, "Go to: ", NULL)) {
        line_free(input);
        return;
    }
    if (input->len == 0) {
        line_free(input);
        return;
    }
    char *endptr;
    long lineno = strtol((const char *)input->s, &endptr, 10);
    long colno = -1;
    if (*endptr == ':') {
        ++endptr;
        colno = strtol(endptr, &endptr, 10);
    }
    if (*endptr != '\0' || lineno <= 0 || (size_t)lineno > editor.file.buffer.num_lines || colno == 0) {
        line_free(input);
        status_msg("Invalid position");
        return;
    }
    size_t target_y = (size_t)(lineno - 1);
    while (editor.file.cursor.y < target_y && editor.file.buffer.curr->next) {
        editor.file.buffer.curr = editor.file.buffer.curr->next;
        ++editor.file.cursor.y;
    }
    while (editor.file.cursor.y > target_y && editor.file.buffer.curr->prev) {
        editor.file.buffer.curr = editor.file.buffer.curr->prev;
        --editor.file.cursor.y;
    }
    if (colno < 1) {
        colno = 1;
    }
    size_t target_rx = (size_t)(colno - 1);
    editor.file.cursor.x = rx_to_x(editor.file.buffer.curr, target_rx);
    editor.file.cursor.rx = x_to_rx(editor.file.buffer.curr, editor.file.cursor.x);
    line_free(input);
}
static bool_t is_blank(Line *line) {
    for (size_t i = 0; i < line->len; ++i) {
        if (line->s[i] != ' ' && line->s[i] != '\t') {
            return FALSE;
        }
    }
    return TRUE;
}
static void pre_line_change(Line *line) {
    editor.file.buffer.digest -= line->hash;
}
static void post_line_change(Line *line) {
    line->hash = fnv1a_hash(line->s, line->len);
    editor.file.buffer.digest += line->hash;
    editor.file.is_modified = (editor.file.buffer.digest != editor.file.saved_digest);
}
