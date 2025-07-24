#include "yoc.h"
#include <string.h>
#include <limits.h>
#define STATUS_BG_ON  "\x1b[47m\x1b[30m"
#define STATUS_BG_OFF "\x1b[0m"
const char *extract_filename(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *sep = (slash > backslash) ? slash : backslash;
    return sep ? sep + 1 : path;
}
typedef struct {
    size_t cx;
    char *msg;
    Line *input;
    size_t charsoff;
} StatusInput;
static void   status_realloc(size_t len);
static void   status_set_default(void);
static void   status_input_print(StatusInput *statin);
static bool_t status_process_input(StatusInput *statin);
static void   status_do_insert(StatusInput *statin, unsigned char *s);
static void   status_do_backspace(StatusInput *statin);
static void   status_do_home(StatusInput *statin);
static void   status_do_end(StatusInput *statin);
static void   status_do_arrow_left(StatusInput *statin);
static void   status_do_arrow_right(StatusInput *statin);
static void   status_do_delete_forward(StatusInput *statin);
S_Mode status_mode;
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
    if (status_mode == INPUT_MODE) {
        return;
    }
    const char rev_on[]  = STATUS_BG_ON;
    const char rev_off[] = STATUS_BG_OFF;
    size_t mlen;
    if (status_mode == NORMAL) {
        status_set_default();
    }
    term_write((const unsigned char *)rev_on, sizeof(rev_on) - 1);
    term_clear_line();
    mlen = editor.file.status.len;
    if (mlen > editor.cols) {
        mlen = editor.cols;
    }
    term_write((unsigned char *)editor.file.status.msg, mlen);
    if (status_mode == NORMAL) {
        char right_str[32];
        size_t right_len = (size_t)snprintf(
            right_str, sizeof(right_str), "%lu:%lu",
            (unsigned long)(editor.file.cursor.y + 1),
            (unsigned long)(editor.file.cursor.rx + 1)
        );
        if (right_len < editor.cols) {
            term_set_cursor(editor.cols - right_len, editor.rows);
            term_write((unsigned char *)right_str, right_len);
        }
    }
    term_write((const unsigned char *)rev_off, sizeof(rev_off) - 1);
    status_mode = NORMAL;
}
bool_t status_input(Line *input, char *msg, const char *placeholder) {
    StatusInput statin_val;
    statin_val.cx = 0;
    statin_val.charsoff = 0;
    statin_val.msg = msg;
    statin_val.input = input;
    if (placeholder) {
        line_insert_str(statin_val.input, 0, (unsigned char *)placeholder);
        statin_val.cx += strlen(placeholder);
    }
    bool_t ret = status_process_input(&statin_val);
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
        if (len != 0) {
            status_do_insert(statin, s);
        } else {
            switch (special_key) {
                case ESC:         return FALSE;
                case ENTER:       return TRUE;
                case HOME:        status_do_home(statin);        break;
                case END:         status_do_end(statin);         break;
                case ARROW_LEFT:  status_do_arrow_left(statin);  break;
                case ARROW_RIGHT: status_do_arrow_right(statin); break;
                case BACKSPACE:   status_do_backspace(statin);   break;
                case DEL:         status_do_delete_forward(statin); break;
            }
        }
    }
}
static void status_input_print(StatusInput *statin) {
    char   *msg    = statin->msg;
    size_t  msglen = strlen(statin->msg);
    size_t  free_space, len, input_width, start;
    if (msglen >= editor.cols - 2) {
        msg    = ": ";
        msglen = 2;
    }
    free_space = editor.cols - msglen - 1;
    if (statin->cx < statin->charsoff) {
        statin->charsoff = statin->cx;
    } else if (statin->cx - statin->charsoff > free_space) {
        statin->charsoff = statin->cx - free_space;
    }
    input_width = line_get_width(statin->input);
    start = mbnum_to_index(statin->input->s, statin->charsoff);
    if (input_width - statin->charsoff > free_space) {
        len = width_to_length(statin->input->s + start, free_space);
    } else {
        len = statin->input->len - start;
    }
    term_set_cursor(0, editor.rows);
    term_write((const unsigned char *)STATUS_BG_ON, sizeof(STATUS_BG_ON) - 1);
    term_clear_line();
    term_write((unsigned char *)msg, msglen);
    term_write(statin->input->s + start, len);
    term_write((const unsigned char *)STATUS_BG_OFF, sizeof(STATUS_BG_OFF) - 1);
    term_set_cursor(statin->cx + msglen - statin->charsoff, editor.rows);
}
static void status_realloc(size_t len) {
    if (len >= editor.file.status.cap) {
        size_t new_cap = (editor.file.status.cap == 0) ? BUFF_SIZE : editor.file.status.cap;
        while (len >= new_cap) {
            if (LIKELY(new_cap <= SIZE_MAX / 2)) {
                new_cap *= 2;
            } else {
                new_cap = len + 1;
            }
        }
        editor.file.status.msg = (char *)xrealloc(editor.file.status.msg, new_cap);
        editor.file.status.cap = new_cap;
    }
}
static void status_set_default(void) {
    const char *path;
    size_t      left_len;
    status_realloc(editor.cols + 1);
    path = editor.file.path[0] ? extract_filename(editor.file.path) : "[No Name]";
    if (editor.file.is_modified && editor.file.path[0] != '\0') {
        left_len = (size_t)snprintf(editor.file.status.msg, editor.file.status.cap, "%s [+]", path);
    } else {
        left_len = (size_t)snprintf(editor.file.status.msg, editor.file.status.cap, "%s", path);
    }
    if (left_len > editor.cols - 1) {
        size_t allowed_width = editor.cols - 1;
        left_len = width_to_length((unsigned char *)editor.file.status.msg, allowed_width);
        editor.file.status.msg[left_len] = '\0';
    }
    editor.file.status.len = left_len;
}
static void status_do_insert(StatusInput *statin, unsigned char *s) {
    size_t str_len = strlen((const char *)s);
    if (str_len == 0) return;
    size_t chars = index_to_mbnum(s, str_len);
    line_insert_str(
        statin->input,
        mbnum_to_index(statin->input->s, statin->cx),
        s
    );
    statin->cx += chars;
}
static void status_do_home(StatusInput *statin) {
    statin->cx = 0;
}
static void status_do_end(StatusInput *statin) {
    statin->cx = line_get_mblen(statin->input);
}
static void status_do_arrow_left(StatusInput *statin) {
    if (statin->cx > 0) {
        --statin->cx;
    }
}
static void status_do_arrow_right(StatusInput *statin) {
    if (statin->cx < line_get_mblen(statin->input)) {
        ++statin->cx;
    }
}
static void status_do_backspace(StatusInput *statin) {
    if (statin->input->len == 0 || statin->cx == 0) return;
    size_t start    = mbnum_to_index(statin->input->s, statin->cx - 1);
    size_t char_len = utf8_len(statin->input->s[start]);
    if (char_len == 0 || start + char_len > statin->input->len) {
        char_len = 1;
    }
    line_del_str(statin->input, start, char_len);
    --statin->cx;
    if (statin->cx < statin->charsoff) {
        statin->charsoff = statin->cx;
    }
}
static void status_do_delete_forward(StatusInput *statin) {
    if (statin->input->len == 0 || statin->cx >= line_get_mblen(statin->input)) return;
    size_t start = mbnum_to_index(statin->input->s, statin->cx);
    size_t char_len = utf8_len(statin->input->s[start]);
    if (char_len == 0 || start + char_len > statin->input->len) {
        char_len = 1;
    }
    line_del_str(statin->input, start, char_len);
}
