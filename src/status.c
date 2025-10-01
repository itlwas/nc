#include "nc.h"
#include <string.h>
#include <limits.h>
#define STATUS_BG_ON  "\x1b[47m\x1b[30m"
#define STATUS_BG_OFF "\x1b[0m"
typedef struct {
    size_t cx;
    size_t charsoff;
    char *msg;
    Line *input;
} StatusInput;
S_Mode status_mode;
static void   status_realloc(size_t len);
static void   status_set_default(void);
static void   status_input_print(StatusInput *statin);
static bool_t status_process_input(StatusInput *statin);
static void   status_do_insert(StatusInput *statin, unsigned char *s);
static void   status_do_backspace(StatusInput *statin);
static void   status_do_delete_forward(StatusInput *statin);
static void   status_do_home(StatusInput *statin);
static void   status_do_end(StatusInput *statin);
static void   status_do_arrow_left(StatusInput *statin);
static void   status_do_arrow_right(StatusInput *statin);
const char *extract_filename(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *sep = !slash ? backslash : !backslash ? slash : (slash > backslash ? slash : backslash);
    return sep ? sep + 1 : path;
}
void status_init(void) {
    editor.file.status.msg = (char *)xmalloc(BUFF_SIZE);
    editor.file.status.msg[0] = '\0';
    editor.file.status.cap = BUFF_SIZE;
    editor.file.status.len = 0;
    status_mode = NORMAL;
}
void status_free(void) {
    free(editor.file.status.msg);
    editor.file.status.cap = 0;
    editor.file.status.len = 0;
}
void status_msg(const char *msg) {
    size_t len = strlen(msg);
    status_realloc(len + 1);
    memcpy(editor.file.status.msg, msg, len);
    editor.file.status.msg[len] = '\0';
    editor.file.status.len = len;
    status_mode = MESSAGE;
}
void status_print(void) {
    if (status_mode == INPUT_MODE) return;
    if (status_mode == NORMAL) status_set_default();
    term_write((const unsigned char *)STATUS_BG_ON, sizeof(STATUS_BG_ON) - 1);
    term_clear_line();
    size_t mlen = editor.file.status.len > editor.cols ? editor.cols : editor.file.status.len;
    term_write((unsigned char *)editor.file.status.msg, mlen);
    if (status_mode == NORMAL) {
        char right_str[32];
        size_t right_len = (size_t)snprintf(right_str, sizeof(right_str), "%lu:%lu",
            (unsigned long)(editor.file.cursor.y + 1),
            (unsigned long)(editor.file.cursor.rx + 1));
        if (right_len < editor.cols) {
            term_set_cursor(editor.cols - right_len, editor.rows);
            term_write((unsigned char *)right_str, right_len);
        }
    }
    term_write((const unsigned char *)STATUS_BG_OFF, sizeof(STATUS_BG_OFF) - 1);
    status_mode = NORMAL;
}
bool_t status_input(Line *input, char *msg, const char *placeholder) {
    StatusInput statin = {0, 0, msg, input};
    if (placeholder) {
        line_insert_str(input, 0, (unsigned char *)placeholder);
        statin.cx = strlen(placeholder);
    }
    bool_t ret = status_process_input(&statin);
    status_mode = NORMAL;
    return ret;
}
static bool_t status_process_input(StatusInput *statin) {
    int special_key;
    unsigned char *s;
    size_t len;
    status_mode = INPUT_MODE;
    for (;;) {
        status_input_print(statin);
        len = term_read(&s, &special_key);
        if (len) {
            status_do_insert(statin, s);
        } else {
            switch (special_key) {
                case ESC:         return FALSE;
                case ENTER:       return TRUE;
                case HOME:        status_do_home(statin);           break;
                case END:         status_do_end(statin);            break;
                case ARROW_LEFT:  status_do_arrow_left(statin);     break;
                case ARROW_RIGHT: status_do_arrow_right(statin);    break;
                case BACKSPACE:   status_do_backspace(statin);      break;
                case DEL:         status_do_delete_forward(statin); break;
            }
        }
    }
}
static void status_input_print(StatusInput *statin) {
    char *msg = statin->msg;
    size_t msglen = strlen(msg);
    if (msglen >= editor.cols - 2) {
        msg = ": ";
        msglen = 2;
    }
    size_t free_space = editor.cols - msglen - 1;
    if (statin->cx < statin->charsoff) {
        statin->charsoff = statin->cx;
    } else if (statin->cx - statin->charsoff > free_space) {
        statin->charsoff = statin->cx - free_space;
    }
    size_t input_width = line_get_width(statin->input);
    size_t start = mbnum_to_index(statin->input->s, statin->charsoff);
    size_t len = (input_width - statin->charsoff > free_space)
        ? width_to_length(statin->input->s + start, free_space)
        : statin->input->len - start;
    term_set_cursor(0, editor.rows);
    term_write((const unsigned char *)STATUS_BG_ON, sizeof(STATUS_BG_ON) - 1);
    term_clear_line();
    term_write((unsigned char *)msg, msglen);
    term_write(statin->input->s + start, len);
    term_write((const unsigned char *)STATUS_BG_OFF, sizeof(STATUS_BG_OFF) - 1);
    term_set_cursor(statin->cx + length_to_width((unsigned char *)msg, msglen) - statin->charsoff, editor.rows);
}
static void status_realloc(size_t len) {
    if (len < editor.file.status.cap) return;
    size_t new_cap = editor.file.status.cap ? editor.file.status.cap : BUFF_SIZE;
    while (len >= new_cap) {
        new_cap = LIKELY(new_cap <= SIZE_MAX / 2) ? new_cap * 2 : len + 1;
    }
    editor.file.status.msg = (char *)xrealloc(editor.file.status.msg, new_cap);
    editor.file.status.cap = new_cap;
}
static void status_set_default(void) {
    status_realloc(editor.cols + 1);
    const char *path = editor.file.path[0] ? extract_filename(editor.file.path) : "[No Name]";
    size_t left_len;
    if (editor.file.is_modified && editor.file.path[0]) {
        left_len = (size_t)snprintf(editor.file.status.msg, editor.file.status.cap, "%s [+]", path);
    } else {
        left_len = (size_t)snprintf(editor.file.status.msg, editor.file.status.cap, "%s", path);
    }
    if (left_len > editor.cols - 1) {
        size_t allowed_width = editor.cols - 1;
        left_len = width_to_length((unsigned char *)editor.file.status.msg, allowed_width);
        while (left_len > 0 && is_continuation_byte((unsigned char)editor.file.status.msg[left_len])) {
            --left_len;
        }
        editor.file.status.msg[left_len] = '\0';
    }
    editor.file.status.len = left_len;
}
static void status_do_insert(StatusInput *statin, unsigned char *s) {
    size_t str_len = strlen((const char *)s);
    if (!str_len) return;
    size_t chars = index_to_mbnum(s, str_len);
    line_insert_str(statin->input, mbnum_to_index(statin->input->s, statin->cx), s);
    statin->cx += chars;
}
static void status_do_home(StatusInput *statin) {
    statin->cx = 0;
}
static void status_do_end(StatusInput *statin) {
    statin->cx = line_get_mblen(statin->input);
}
static void status_do_arrow_left(StatusInput *statin) {
    if (statin->cx > 0) --statin->cx;
}
static void status_do_arrow_right(StatusInput *statin) {
    if (statin->cx < line_get_mblen(statin->input)) ++statin->cx;
}
static void status_do_backspace(StatusInput *statin) {
    if (!statin->input->len || !statin->cx) return;
    size_t start = mbnum_to_index(statin->input->s, statin->cx - 1);
    size_t char_len = utf8_len(statin->input->s[start]);
    if (!char_len || start + char_len > statin->input->len) char_len = 1;
    line_del_str(statin->input, start, char_len);
    --statin->cx;
    if (statin->cx < statin->charsoff) statin->charsoff = statin->cx;
}
static void status_do_delete_forward(StatusInput *statin) {
    if (!statin->input->len || statin->cx >= line_get_mblen(statin->input)) return;
    size_t start = mbnum_to_index(statin->input->s, statin->cx);
    size_t char_len = utf8_len(statin->input->s[start]);
    if (!char_len || start + char_len > statin->input->len) char_len = 1;
    line_del_str(statin->input, start, char_len);
}
