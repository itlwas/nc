#include "yoc.h"
#include <string.h>
#include <ctype.h>
typedef struct {
    Line   *line;
    size_t  y;
    size_t  start_rx;
    size_t  end_rx;
} FindMatch;
typedef struct {
    FindMatch *items;
    size_t     count;
    size_t     capacity;
} FindMatchList;
static inline unsigned char to_lower_ascii_uc(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}
static void build_skip_table(const unsigned char *needle, size_t nlen, size_t skip[256]) {
    for (size_t i = 0; i < 256; ++i) skip[i] = nlen;
    if (nlen == 0) return;
    for (size_t j = 0; j + 1 < nlen; ++j) {
        skip[to_lower_ascii_uc(needle[j])] = nlen - 1 - j;
    }
}
static size_t rx_advance(Line *line, size_t from, size_t to, size_t rx_start) {
    const unsigned char *s = line->s;
    size_t rx = rx_start;
    while (from < to) {
        unsigned char c = s[from];
        if (c == '\t') {
            size_t tab_w = editor.tabsize - (rx % editor.tabsize);
            rx += tab_w;
            from += 1;
            continue;
        }
        size_t len = utf8_len(c);
        if (len == 0 || from + len > line->len) len = 1;
        rx += char_display_width(&s[from]);
        from += len;
    }
    return rx;
}
static void line_find_all_bmh(Line *line, const unsigned char *needle, size_t nlen,
                              size_t needle_rx, size_t line_y,
                              FindMatchList *out) {
    if (nlen == 0 || line->len == 0) return;
    size_t skip[256];
    build_skip_table(needle, nlen, skip);
    const unsigned char *hay = line->s;
    size_t i = 0;
    size_t rx_at_i = 0;
    while (i + nlen <= line->len) {
        size_t k = nlen;
        do {
            --k;
            if (to_lower_ascii_uc(hay[i + k]) != to_lower_ascii_uc(needle[k])) {
                unsigned char last = to_lower_ascii_uc(hay[i + nlen - 1]);
                size_t shift = skip[last];
                if (shift == 0) shift = 1;
                size_t prev = i;
                i += shift;
                rx_at_i = rx_advance(line, prev, i, rx_at_i);
                goto next_iter;
            }
        } while (k != 0);
        if (out->count == out->capacity) {
            size_t new_cap = (out->capacity == 0) ? 8 : (out->capacity * 2);
            out->items = (FindMatch *)xrealloc(out->items, new_cap * sizeof(FindMatch));
            out->capacity = new_cap;
        }
        out->items[out->count].line     = line;
        out->items[out->count].y        = line_y;
        out->items[out->count].start_rx = rx_at_i;
        out->items[out->count].end_rx   = rx_at_i + needle_rx;
        out->count++;
        {
            unsigned char last = to_lower_ascii_uc(hay[i + nlen - 1]);
            size_t shift = skip[last];
            if (shift == 0) shift = 1;
            size_t prev = i;
            i += shift;
            rx_at_i = rx_advance(line, prev, i, rx_at_i);
        }
next_iter:
        continue;
    }
}
static void collect_matches(const unsigned char *needle, size_t nlen, FindMatchList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    Line *line = editor.file.buffer.begin;
    size_t y = 0;
    size_t needle_rx = length_to_width(needle, nlen);
    while (line) {
        line_find_all_bmh(line, needle, nlen, needle_rx, y, list);
        line = line->next;
        ++y;
    }
}
static size_t compute_lineno_pad(void) {
    if (!show_line_numbers) return 0;
    size_t digits = 1;
    for (size_t n = editor.file.buffer.num_lines; n >= 10; n /= 10) {
        ++digits;
    }
    return digits + 2;
}
static void draw_segment(Line *line, size_t screen_y, size_t rx_start, size_t rx_width, int style) {
    if (rx_width == 0 || screen_y >= editor.rows) return;
    size_t lineno_pad = compute_lineno_pad();
    bool_t scrollbar = (editor.file.buffer.num_lines > editor.rows);
    size_t avail_cols = editor.cols - (scrollbar ? 1 : 0);
    size_t text_cols  = avail_cols - (show_line_numbers ? lineno_pad : 0);
    size_t vis_start = (rx_start < editor.window.x) ? editor.window.x : rx_start;
    size_t vis_end   = rx_start + rx_width;
    size_t win_end   = editor.window.x + text_cols;
    if (vis_end > win_end) vis_end = win_end;
    if (vis_end <= vis_start) return;
    size_t screen_x = (show_line_numbers ? lineno_pad : 0) + (vis_start - editor.window.x);
    if (screen_x >= avail_cols) return;
    static const unsigned char sgr_reset[] = "\x1b[0m";
    static const unsigned char sgr_invert[] = "\x1b[7m";
    static const unsigned char sgr_underline[] = "\x1b[4m";
    term_set_cursor(screen_x, screen_y);
    term_write(sgr_reset, sizeof(sgr_reset) - 1);
    if (style == 1) {
        term_write(sgr_invert, sizeof(sgr_invert) - 1);
    } else if (style == 2) {
        term_write(sgr_underline, sizeof(sgr_underline) - 1);
    }
    const unsigned char *s = line->s;
    size_t i = width_to_length(s, vis_start);
    size_t col = vis_start;
    while (s[i] != '\0' && col < vis_end) {
        unsigned char c = s[i];
        size_t char_len = 1;
        if (c == '\t') {
            size_t spaces_to_add = editor.tabsize - (col % editor.tabsize);
            while (spaces_to_add > 0 && col < vis_end) {
                term_write((const unsigned char *)" ", 1);
                ++col;
                --spaces_to_add;
            }
            i += 1;
            continue;
        }
        if (LIKELY(c < 0x80u)) {
            if (c >= 0x20u && c != 0x7Fu) {
                if (col + 1 > vis_end) break;
                term_write(&s[i], 1);
                ++col;
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
            if (col + char_width > vis_end) break;
            term_write(s + i, char_len);
            col += char_width;
        }
        i += char_len;
    }
    term_write(sgr_reset, sizeof(sgr_reset) - 1);
}
static void clear_visible_highlights(const FindMatchList *list) {
    for (size_t idx = 0; idx < list->count; ++idx) {
        const FindMatch *m = &list->items[idx];
        if (m->y < editor.window.y || m->y >= editor.window.y + editor.rows) continue;
        size_t screen_y = m->y - editor.window.y;
        size_t width_cols = (m->end_rx > m->start_rx) ? (m->end_rx - m->start_rx) : 0;
        draw_segment(m->line, screen_y, m->start_rx, width_cols, 0);
    }
}
static void draw_highlights(const FindMatchList *list, size_t current_idx) {
    for (size_t idx = 0; idx < list->count; ++idx) {
        const FindMatch *m = &list->items[idx];
        if (m->y < editor.window.y || m->y >= editor.window.y + editor.rows) continue;
        size_t screen_y = m->y - editor.window.y;
        if (idx == current_idx) {
            size_t width_cols = (m->end_rx > m->start_rx) ? (m->end_rx - m->start_rx) : 0;
            draw_segment(m->line, screen_y, m->start_rx, width_cols, 1);
        } else {
            size_t width_cols = (m->end_rx > m->start_rx) ? (m->end_rx - m->start_rx) : 0;
            draw_segment(m->line, screen_y, m->start_rx, width_cols, 2);
        }
    }
}
static void move_cursor_to(size_t target_y, size_t target_rx) {
    editor.file.cursor.y = target_y;
    Line *line = editor.file.buffer.begin;
    size_t y = 0;
    while (y < target_y && line->next) {
        line = line->next;
        ++y;
    }
    editor.file.buffer.curr = line;
    editor.file.cursor.x = rx_to_x(line, target_rx);
    editor.file.cursor.rx = x_to_rx(line, editor.file.cursor.x);
    edit_fix_cursor_x();
}
static void restore_cursor(void) {
    size_t lineno_pad = compute_lineno_pad();
    size_t cx = (show_line_numbers ? lineno_pad : 0) + editor.file.cursor.rx - editor.window.x;
    size_t cy = editor.file.cursor.y - editor.window.y;
    term_set_cursor(cx, cy);
    term_show_cursor();
}
static void show_status_match(size_t idx, size_t total) {
    if (total == 0) {
        status_msg("No matches found");
        return;
    }
    char buf[64];
    size_t shown = idx + 1;
    (void)snprintf(buf, sizeof(buf), "Match %lu of %lu",
                   (unsigned long)shown, (unsigned long)total);
    status_msg(buf);
}
static size_t nearest_match_index(const FindMatchList *list) {
    if (list->count == 0) return 0;
    size_t best = 0;
    size_t best_dist = (size_t)-1;
    for (size_t i = 0; i < list->count; ++i) {
        size_t y = list->items[i].y;
        size_t dist = (y > editor.window.y) ? (y - editor.window.y) : (editor.window.y - y);
        if (dist < best_dist) { best = i; best_dist = dist; }
    }
    return best;
}
bool_t find_start(void) {
    Line *input = line_new(NULL, NULL);
    if (!status_input(input, "Find: ", NULL)) {
        line_free(input);
        return FALSE;
    }
    if (input->len == 0) {
        line_free(input);
        return FALSE;
    }
    FindMatchList list;
    collect_matches(input->s, input->len, &list);
    if (list.count == 0) {
        status_msg("No matches found");
        render_refresh();
        line_free(input);
        return FALSE;
    }
    size_t cur = nearest_match_index(&list);
    move_cursor_to(list.items[cur].y, list.items[cur].start_rx);
    show_status_match(cur, list.count);
    render_refresh();
    draw_highlights(&list, cur);
    restore_cursor();
    for (;;) {
        int special_key = 0;
        unsigned char *typed = NULL;
        size_t len = term_read(&typed, &special_key);
        if (len != 0) {
            clear_visible_highlights(&list);
            render_refresh();
            restore_cursor();
            edit_insert(typed);
            break;
        }
        if (special_key == ESC || special_key == CTRL_KEY('q') || special_key == ENTER) {
            clear_visible_highlights(&list);
            render_refresh();
            restore_cursor();
            break;
        }
        if (special_key == ARROW_UP) {
            cur = (cur == 0) ? (list.count - 1) : (cur - 1);
            move_cursor_to(list.items[cur].y, list.items[cur].start_rx);
            show_status_match(cur, list.count);
            render_refresh();
            draw_highlights(&list, cur);
            restore_cursor();
            continue;
        }
        if (special_key == ARROW_DOWN) {
            cur = (cur + 1) % list.count;
            move_cursor_to(list.items[cur].y, list.items[cur].start_rx);
            show_status_match(cur, list.count);
            render_refresh();
            draw_highlights(&list, cur);
            restore_cursor();
            continue;
        }
    }
    if (list.items) free(list.items);
    line_free(input);
    return TRUE;
}
