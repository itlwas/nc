#ifndef YOC_SYNTAX_H
#define YOC_SYNTAX_H

#include "yoc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SyntaxImpl {
    const char *name;
    const char *const *exts;
    size_t num_exts;
    void (*write)(const unsigned char *s, size_t len);
} SyntaxImpl;

// Called on every refresh or when file path potentially changes.
void syntax_set_file(const char *path);

// Whether highlighting is currently active for the opened file.
bool_t syntax_enabled(void);

// Write a colored rendering of the given line bytes to the terminal.
// Does not modify buffers; only emits ANSI sequences via term_write.
void syntax_write_line(const unsigned char *s, size_t len);

// Generic writers (used by language stubs)
void syntax_write_clike(const unsigned char *s, size_t len);
void syntax_write_py(const unsigned char *s, size_t len);
void syntax_write_sh(const unsigned char *s, size_t len);
void syntax_write_json(const unsigned char *s, size_t len);
void syntax_write_html(const unsigned char *s, size_t len);
void syntax_write_ini(const unsigned char *s, size_t len);
void syntax_write_sql(const unsigned char *s, size_t len);
void syntax_write_md(const unsigned char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif