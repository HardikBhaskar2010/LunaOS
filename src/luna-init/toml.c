/*
 * Copyright (c) 2026 Hardik Bhaskar
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/*
 * toml.c — Minimal TOML Parser Implementation
 * Authority: Volume VI / 01_coding_standards.md §5 (bounded buffers, fuzzing)
 * Authority: Volume II / 04_init_system.md, Volume II / 02_boot_flow.md
 */

#include "toml.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ─── Internal limits ────────────────────────────────────────────────────── */

#define TOML_MAX_ENTRIES      128   /* max key=value pairs per document      */
#define TOML_MAX_ARRAY_TABLES  32   /* max [[array-of-tables]] entries        */
#define TOML_MAX_LINE_LEN     512   /* max single line length (bounded read)  */

/* ─── Internal types ─────────────────────────────────────────────────────── */

typedef struct {
    char            section[TOML_MAX_SECTION_LEN];  /* "" = top-level        */
    char            key[TOML_MAX_KEY_LEN];
    toml_value_t    value;
    int             array_table_index; /* -1 = not in [[array]], else 0-based */
} toml_entry_t;

struct toml_doc {
    toml_entry_t entries[TOML_MAX_ENTRIES];
    size_t       entry_count;

    /* Track [[array-of-tables]] section counts */
    struct {
        char   name[TOML_MAX_SECTION_LEN];
        int    count;
    } array_tables[TOML_MAX_ARRAY_TABLES];
    size_t array_table_count;
};

/* ─── String helpers (all bounds-checked) ────────────────────────────────── */

static void str_trim_inplace(char *s) {
    if (!s || !*s) return;
    /* trim leading */
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, strlen(s) - start + 1);
    /* trim trailing */
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) { s[--len] = '\0'; }
}

/* Copy at most dst_size-1 bytes, always null-terminate. Returns -1 on truncation. */
static int str_copy_bounded(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return -1;
    size_t src_len = strlen(src);
    if (src_len >= dst_size) {
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return -1;  /* truncated */
    }
    memcpy(dst, src, src_len + 1);
    return 0;
}

/* ─── Lookup helpers ─────────────────────────────────────────────────────── */

static int toml_find_array_table_index(toml_doc_t *doc, const char *name) {
    for (size_t i = 0; i < doc->array_table_count; i++) {
        if (strncmp(doc->array_tables[i].name, name, TOML_MAX_SECTION_LEN) == 0)
            return (int)i;
    }
    return -1;
}

static int toml_get_or_create_array_table(toml_doc_t *doc, const char *name) {
    int idx = toml_find_array_table_index(doc, name);
    if (idx >= 0) return idx;
    if (doc->array_table_count >= TOML_MAX_ARRAY_TABLES) return -1;
    idx = (int)doc->array_table_count++;
    str_copy_bounded(doc->array_tables[idx].name, TOML_MAX_SECTION_LEN, name);
    doc->array_tables[idx].count = 0;
    return idx;
}

/* ─── Line parser ────────────────────────────────────────────────────────── */

typedef struct {
    char   cur_section[TOML_MAX_SECTION_LEN];
    bool   cur_is_array_table;
    int    cur_array_table_entry; /* current [[x]] entry index (0-based) */
} parse_state_t;

/*
 * parse_string_value() — Extract the string value between quotes.
 * Handles \" escape only. Output is bounded to TOML_MAX_STR_LEN.
 */
static toml_error_t parse_string_value(const char *src, char *dst) {
    const char *p   = src;
    size_t      out = 0;

    if (*p != '"') return TOML_ERR_INVALID;
    p++;

    while (*p && *p != '"' && out < TOML_MAX_STR_LEN - 1) {
        if (*p == '\\' && *(p + 1) == '"') {
            dst[out++] = '"';
            p += 2;
        } else {
            dst[out++] = *p++;
        }
    }

    if (*p != '"') return (*p == '\0') ? TOML_ERR_INVALID : TOML_ERR_OVERFLOW;
    dst[out] = '\0';
    return TOML_OK;
}

/*
 * parse_array_value() — Extract ["item1", "item2"] into toml_value_t.
 * Only string arrays are supported (sufficient for Mahina config files).
 */
static toml_error_t parse_array_value(const char *src, toml_value_t *val) {
    val->type       = TOML_TYPE_ARRAY;
    val->v.array.count = 0;

    const char *p = src;
    if (*p != '[') return TOML_ERR_INVALID;
    p++;

    while (*p) {
        /* Skip whitespace and commas */
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']') break;
        if (*p == '\0') return TOML_ERR_INVALID;

        if (*p != '"') return TOML_ERR_UNSUPPORTED; /* non-string array */

        if (val->v.array.count >= TOML_MAX_ARRAY_ITEMS)
            return TOML_ERR_OVERFLOW;

        char *item = val->v.array.items[val->v.array.count];
        toml_error_t err = parse_string_value(p, item);
        if (err != TOML_OK) return err;
        val->v.array.count++;

        /* Advance past the closing quote */
        p++; /* skip opening " */
        while (*p && *p != '"') {
            if (*p == '\\') p++;
            p++;
        }
        if (*p == '"') p++; /* skip closing " */
    }

    if (*p != ']') return TOML_ERR_INVALID;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '\0') return TOML_ERR_INVALID;

    return TOML_OK;
}

/*
 * parse_value() — Detect and parse any supported TOML value type.
 */
static toml_error_t parse_value(const char *raw, toml_value_t *val) {
    /* String */
    if (raw[0] == '"') {
        val->type = TOML_TYPE_STRING;
        return parse_string_value(raw, val->v.str);
    }

    /* Array */
    if (raw[0] == '[') {
        return parse_array_value(raw, val);
    }

    /* Boolean */
    if (strncmp(raw, "true", 4) == 0 &&
        (raw[4] == '\0' || isspace((unsigned char)raw[4]) || raw[4] == '#')) {
        /* Require proper token termination to avoid "trueish" being accepted.
         * (CODE_AUDIT_REPORT §6.1) */
        val->type       = TOML_TYPE_BOOLEAN;
        val->v.boolean  = true;
        return TOML_OK;
    }
    if (strncmp(raw, "false", 5) == 0 &&
        (raw[5] == '\0' || isspace((unsigned char)raw[5]) || raw[5] == '#')) {
        val->type       = TOML_TYPE_BOOLEAN;
        val->v.boolean  = false;
        return TOML_OK;
    }

    /* Integer */
    char *end = NULL;
    errno     = 0;
    long long iv = strtoll(raw, &end, 10);
    if (end != raw && errno == 0) {
        /* Reject trailing garbage e.g. "5000garbage" — the value string has
         * already been trimmed of whitespace and inline comments by parse_line(),
         * so *end must be '\0' for a valid integer literal.
         * (CODE_AUDIT_REPORT §6.2) */
        if (*end != '\0') return TOML_ERR_INVALID;
        val->type      = TOML_TYPE_INTEGER;
        val->v.integer = (int64_t)iv;
        return TOML_OK;
    }

    return TOML_ERR_UNSUPPORTED;
}

/*
 * parse_line() — Parse a single TOML line and update doc/state.
 */
static toml_error_t parse_line(toml_doc_t *doc, parse_state_t *state,
                                char *line, int lineno) {
    (void)lineno; /* reserved for future error reporting */

    str_trim_inplace(line);

    /* Empty line or comment */
    if (line[0] == '\0' || line[0] == '#') return TOML_OK;

    /* [[array-of-tables]] header */
    if (line[0] == '[' && line[1] == '[') {
        char *end = strstr(line + 2, "]]");
        if (!end) return TOML_ERR_INVALID;
        *end = '\0';
        const char *name = line + 2;

        if (str_copy_bounded(state->cur_section, TOML_MAX_SECTION_LEN, name) < 0)
            return TOML_ERR_OVERFLOW;

        state->cur_is_array_table = true;

        int ati = toml_get_or_create_array_table(doc, name);
        if (ati < 0) return TOML_ERR_OVERFLOW;
        state->cur_array_table_entry = doc->array_tables[ati].count++;
        return TOML_OK;
    }

    /* [section] header */
    if (line[0] == '[') {
        char *end = strchr(line + 1, ']');
        if (!end) return TOML_ERR_INVALID;
        *end = '\0';
        const char *name = line + 1;

        if (str_copy_bounded(state->cur_section, TOML_MAX_SECTION_LEN, name) < 0)
            return TOML_ERR_OVERFLOW;

        state->cur_is_array_table      = false;
        state->cur_array_table_entry   = -1;
        return TOML_OK;
    }

    /* key = value */
    char *eq = strchr(line, '=');
    if (!eq) return TOML_ERR_INVALID;
    *eq = '\0';

    char key[TOML_MAX_KEY_LEN];
    char raw_val[TOML_MAX_STR_LEN * 2];

    if (str_copy_bounded(key, sizeof(key), line) < 0) return TOML_ERR_OVERFLOW;
    str_trim_inplace(key);

    if (str_copy_bounded(raw_val, sizeof(raw_val), eq + 1) < 0) return TOML_ERR_OVERFLOW;
    str_trim_inplace(raw_val);

    /* Strip inline comment (# outside a string) */
    bool in_str = false;
    for (size_t i = 0; raw_val[i]; i++) {
        if (raw_val[i] == '"') in_str = !in_str;
        if (!in_str && raw_val[i] == '#') { raw_val[i] = '\0'; break; }
    }
    str_trim_inplace(raw_val);

    if (doc->entry_count >= TOML_MAX_ENTRIES) return TOML_ERR_OVERFLOW;

    toml_entry_t *entry = &doc->entries[doc->entry_count];
    memset(entry, 0, sizeof(*entry));

    if (str_copy_bounded(entry->section, TOML_MAX_SECTION_LEN,
                         state->cur_section) < 0)
        return TOML_ERR_OVERFLOW;

    if (str_copy_bounded(entry->key, TOML_MAX_KEY_LEN, key) < 0)
        return TOML_ERR_OVERFLOW;

    entry->array_table_index = state->cur_is_array_table
                               ? state->cur_array_table_entry : -1;

    toml_error_t err = parse_value(raw_val, &entry->value);
    if (err != TOML_OK) return err;

    doc->entry_count++;
    return TOML_OK;
}

/* ─── Public API ─────────────────────────────────────────────────────────── */

toml_doc_t *toml_load_buffer(const char *buf, size_t buf_len,
                              toml_error_t *out_error) {
    if (!buf || buf_len == 0 || buf_len > (size_t)TOML_MAX_DOC_BYTES) {
        if (out_error) *out_error = TOML_ERR_TOO_LARGE;
        return NULL;
    }

    toml_doc_t *doc = calloc(1, sizeof(toml_doc_t));
    if (!doc) { if (out_error) *out_error = TOML_ERR_IO; return NULL; }

    parse_state_t state = {0};
    state.cur_array_table_entry = -1;

    /* Parse line by line using a fixed-size line buffer */
    char line[TOML_MAX_LINE_LEN];
    size_t pos    = 0;
    int    lineno = 1;

    while (pos < buf_len) {
        size_t lstart = pos;
        while (pos < buf_len && buf[pos] != '\n') pos++;
        size_t llen = pos - lstart;
        if (pos < buf_len) pos++; /* consume '\n' */

        if (llen >= sizeof(line)) {
            free(doc);
            if (out_error) *out_error = TOML_ERR_OVERFLOW;
            return NULL;
        }

        memcpy(line, buf + lstart, llen);
        /* Strip Windows-style \r */
        if (llen > 0 && line[llen - 1] == '\r') llen--;
        line[llen] = '\0';

        toml_error_t err = parse_line(doc, &state, line, lineno);
        if (err != TOML_OK) {
            free(doc);
            if (out_error) *out_error = err;
            return NULL;
        }
        lineno++;
    }

    if (out_error) *out_error = TOML_OK;
    return doc;
}

toml_doc_t *toml_load_file(const char *path, toml_error_t *out_error) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { if (out_error) *out_error = TOML_ERR_IO; return NULL; }

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size > (off_t)TOML_MAX_DOC_BYTES) {
        close(fd);
        if (out_error) *out_error = TOML_ERR_TOO_LARGE;
        return NULL;
    }

    char *buf = calloc(1, (size_t)st.st_size + 1);
    if (!buf) { close(fd); if (out_error) *out_error = TOML_ERR_IO; return NULL; }

    ssize_t nread = read(fd, buf, (size_t)st.st_size);
    close(fd);

    if (nread < 0) {
        free(buf);
        if (out_error) *out_error = TOML_ERR_IO;
        return NULL;
    }
    buf[nread] = '\0';

    toml_doc_t *doc = toml_load_buffer(buf, (size_t)nread, out_error);
    free(buf);
    return doc;
}

void toml_free(toml_doc_t *doc) {
    free(doc);
}

toml_error_t toml_get(const toml_doc_t *doc, const char *section,
                       const char *key, toml_value_t *out) {
    if (!doc || !key || !out) return TOML_ERR_INVALID;
    const char *sec = section ? section : "";

    for (size_t i = 0; i < doc->entry_count; i++) {
        const toml_entry_t *e = &doc->entries[i];
        if (e->array_table_index >= 0) continue; /* skip [[array]] entries */
        if (strncmp(e->section, sec, TOML_MAX_SECTION_LEN) == 0 &&
            strncmp(e->key, key, TOML_MAX_KEY_LEN) == 0) {
            *out = e->value;
            return TOML_OK;
        }
    }
    return TOML_ERR_NOT_FOUND;
}

toml_error_t toml_get_nested(const toml_doc_t *doc,
                              const char *parent, const char *child,
                              const char *key, toml_value_t *out) {
    /* Nested sections are stored as "parent.child" in entry->section */
    char combined[TOML_MAX_SECTION_LEN];
    if (snprintf(combined, sizeof(combined), "%s.%s", parent, child)
            >= (int)sizeof(combined))
        return TOML_ERR_OVERFLOW;
    return toml_get(doc, combined, key, out);
}

int toml_array_table_count(const toml_doc_t *doc, const char *section) {
    if (!doc || !section) return 0;
    for (size_t i = 0; i < doc->array_table_count; i++) {
        if (strncmp(doc->array_tables[i].name, section,
                    TOML_MAX_SECTION_LEN) == 0)
            return doc->array_tables[i].count;
    }
    return 0;
}

toml_error_t toml_array_table_get(const toml_doc_t *doc, const char *section,
                                   int index, const char *key,
                                   toml_value_t *out) {
    if (!doc || !section || !key || !out || index < 0) return TOML_ERR_INVALID;

    for (size_t i = 0; i < doc->entry_count; i++) {
        const toml_entry_t *e = &doc->entries[i];
        if (e->array_table_index == index &&
            strncmp(e->section, section, TOML_MAX_SECTION_LEN) == 0 &&
            strncmp(e->key, key, TOML_MAX_KEY_LEN) == 0) {
            *out = e->value;
            return TOML_OK;
        }
    }
    return TOML_ERR_NOT_FOUND;
}

const char *toml_strerror(toml_error_t err) {
    switch (err) {
        case TOML_OK:              return "success";
        case TOML_ERR_INVALID:     return "invalid TOML syntax";
        case TOML_ERR_OVERFLOW:    return "value exceeds buffer limit";
        case TOML_ERR_NOT_FOUND:   return "key or section not found";
        case TOML_ERR_TYPE:        return "value type mismatch";
        case TOML_ERR_UNSUPPORTED: return "unsupported TOML feature";
        case TOML_ERR_IO:          return "file I/O error";
        case TOML_ERR_TOO_LARGE:   return "document exceeds size limit";
        default:                   return "unknown error";
    }
}

int toml_enumerate_section(const toml_doc_t *doc, const char *section,
                            toml_enumerate_cb_t callback, void *userdata) {
    if (!doc || !section || !callback) return 0;
    int count = 0;
    for (size_t i = 0; i < doc->entry_count; i++) {
        const toml_entry_t *e = &doc->entries[i];
        if (e->array_table_index >= 0) continue; /* skip [[array]] entries */
        if (strncmp(e->section, section, TOML_MAX_SECTION_LEN) != 0) continue;
        callback(e->key, &e->value, userdata);
        count++;
    }
    return count;
}
