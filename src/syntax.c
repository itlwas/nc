#include "syntax.h"
#include <string.h>

// Registry entry
typedef struct {
    const char *const *exts;
    size_t num_exts;
    void (*write)(const unsigned char *s, size_t len);
} Entry;

// Forward declarations for writers (can be provided by syntax/*.c)
__attribute__((weak)) void syntax_write_clike(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_py(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_sh(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_json(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_html(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_ini(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_sql(const unsigned char *s, size_t len);
__attribute__((weak)) void syntax_write_md(const unsigned char *s, size_t len);

// Minimal fallback which just writes plain text without color
static void write_plain(const unsigned char *s, size_t len) {
    term_write(s, len);
}

// If a writer is not supplied by a language module, use plain
static void (*pick_or_plain(void (*fn)(const unsigned char*, size_t)))(const unsigned char*, size_t) {
    return fn ? fn : write_plain;
}

static const char *exts_clike[] = { ".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".java", ".cs", ".go", ".rs", ".js", ".ts" };
static const char *exts_py[]    = { ".py" };
static const char *exts_sh[]    = { ".sh", ".bash", ".zsh", ".fish" };
static const char *exts_json[]  = { ".json" };
static const char *exts_html[]  = { ".html", ".htm", ".xml", ".xhtml", ".css" };
static const char *exts_md[]    = { ".md", ".markdown" };
static const char *exts_ini[]   = { ".ini", ".cfg", ".conf", ".toml", ".yaml", ".yml", "makefile" };
static const char *exts_sql[]   = { ".sql" };

static Entry registry[] = {
    { exts_clike, sizeof(exts_clike)/sizeof(exts_clike[0]), NULL },
    { exts_py,    sizeof(exts_py)/sizeof(exts_py[0]),       NULL },
    { exts_sh,    sizeof(exts_sh)/sizeof(exts_sh[0]),       NULL },
    { exts_json,  sizeof(exts_json)/sizeof(exts_json[0]),   NULL },
    { exts_html,  sizeof(exts_html)/sizeof(exts_html[0]),   NULL },
    { exts_md,    sizeof(exts_md)/sizeof(exts_md[0]),       NULL },
    { exts_ini,   sizeof(exts_ini)/sizeof(exts_ini[0]),     NULL },
    { exts_sql,   sizeof(exts_sql)/sizeof(exts_sql[0]),     NULL },
};

static void init_registry(void) {
    registry[0].write = pick_or_plain(syntax_write_clike);
    registry[1].write = pick_or_plain(syntax_write_py);
    registry[2].write = pick_or_plain(syntax_write_sh);
    registry[3].write = pick_or_plain(syntax_write_json);
    registry[4].write = pick_or_plain(syntax_write_html);
    registry[5].write = pick_or_plain(syntax_write_md);
    registry[6].write = pick_or_plain(syntax_write_ini);
    registry[7].write = pick_or_plain(syntax_write_sql);
}

static const Entry *current = NULL;

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

void syntax_set_file(const char *path) {
    init_registry();
    current = NULL;
    if (!path || !path[0]) return;
    const char *name = extract_filename(path);
    for (size_t i = 0; i < sizeof(registry)/sizeof(registry[0]); ++i) {
        for (size_t k = 0; k < registry[i].num_exts; ++k) {
            const char *ext = registry[i].exts[k];
            if (*ext == '.' ? ends_with_ci(name, ext) : equals_ci(name, ext)) {
                current = &registry[i];
                return;
            }
        }
    }
}

bool_t syntax_enabled(void) {
    return current != NULL && current->write != NULL && current->write != write_plain;
}

void syntax_write_line(const unsigned char *s, size_t len) {
    if (!current || !current->write) {
        write_plain(s, len);
        return;
    }
    current->write(s, len);
}