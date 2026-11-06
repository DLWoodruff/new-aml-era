#include "json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_DEPTH 64

typedef struct {
    const char *s;
    size_t n, i;
    char *err;
    size_t errlen;
    int failed;
} Parser;

static void fail(Parser *p, const char *msg) {
    if (!p->failed) {
        size_t line = 1, col = 1;
        for (size_t k = 0; k < p->i && k < p->n; k++) {
            if (p->s[k] == '\n') { line++; col = 1; } else col++;
        }
        snprintf(p->err, p->errlen, "JSON error at line %zu column %zu: %s", line, col, msg);
    }
    p->failed = 1;
}

static void *xrealloc(void *ptr, size_t size) {
    void *q = realloc(ptr, size ? size : 1);
    if (!q) { fprintf(stderr, "out of memory\n"); exit(1); }
    return q;
}

static void skip_ws(Parser *p) {
    while (p->i < p->n && (p->s[p->i] == ' ' || p->s[p->i] == '\t' ||
                           p->s[p->i] == '\n' || p->s[p->i] == '\r'))
        p->i++;
}

static int hex4(Parser *p, unsigned *out) {
    unsigned v = 0;
    if (p->i + 4 > p->n) { fail(p, "truncated \\u escape"); return 0; }
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i++];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else { fail(p, "bad \\u escape"); return 0; }
    }
    *out = v;
    return 1;
}

static size_t put_utf8(char *d, unsigned cp) {
    if (cp < 0x80) { d[0] = (char)cp; return 1; }
    if (cp < 0x800) { d[0] = (char)(0xC0 | (cp >> 6)); d[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) {
        d[0] = (char)(0xE0 | (cp >> 12));
        d[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        d[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    d[0] = (char)(0xF0 | (cp >> 18));
    d[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    d[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    d[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* p->i is at the opening quote. An escape never expands past its source length. */
static char *parse_string(Parser *p) {
    size_t start = ++p->i, end = start;
    while (end < p->n && p->s[end] != '"') {
        if (p->s[end] == '\\') end++;
        end++;
    }
    if (end >= p->n) { fail(p, "unterminated string"); return NULL; }
    char *out = xrealloc(NULL, end - start + 1);
    size_t o = 0;
    while (p->i < end) {
        unsigned char c = (unsigned char)p->s[p->i++];
        if (c < 0x20) { fail(p, "control character in string"); free(out); return NULL; }
        if (c != '\\') { out[o++] = (char)c; continue; }
        c = (unsigned char)p->s[p->i++];
        switch (c) {
            case '"': out[o++] = '"'; break;
            case '\\': out[o++] = '\\'; break;
            case '/': out[o++] = '/'; break;
            case 'b': out[o++] = '\b'; break;
            case 'f': out[o++] = '\f'; break;
            case 'n': out[o++] = '\n'; break;
            case 'r': out[o++] = '\r'; break;
            case 't': out[o++] = '\t'; break;
            case 'u': {
                unsigned cp, lo;
                if (!hex4(p, &cp)) { free(out); return NULL; }
                if (cp >= 0xD800 && cp < 0xDC00) {
                    if (p->i + 2 > end || p->s[p->i] != '\\' || p->s[p->i + 1] != 'u') {
                        fail(p, "unpaired surrogate"); free(out); return NULL;
                    }
                    p->i += 2;
                    if (!hex4(p, &lo)) { free(out); return NULL; }
                    if (lo < 0xDC00 || lo > 0xDFFF) {
                        fail(p, "bad surrogate pair"); free(out); return NULL;
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                o += put_utf8(out + o, cp);
                break;
            }
            default:
                fail(p, "bad escape"); free(out); return NULL;
        }
    }
    out[o] = '\0';
    p->i = end + 1;
    return out;
}

static void parse_value(Parser *p, JsonValue *v, int depth);

static void parse_number(Parser *p, JsonValue *v) {
    size_t i = p->i;
    const char *s = p->s;
    size_t n = p->n;
    if (i < n && s[i] == '-') i++;
    if (i >= n || !isdigit((unsigned char)s[i])) { fail(p, "bad number"); return; }
    if (s[i] == '0') i++;
    else while (i < n && isdigit((unsigned char)s[i])) i++;
    if (i < n && s[i] == '.') {
        i++;
        if (i >= n || !isdigit((unsigned char)s[i])) { fail(p, "bad number"); return; }
        while (i < n && isdigit((unsigned char)s[i])) i++;
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) i++;
        if (i >= n || !isdigit((unsigned char)s[i])) { fail(p, "bad number"); return; }
        while (i < n && isdigit((unsigned char)s[i])) i++;
    }
    char buf[64];
    size_t len = i - p->i;
    if (len >= sizeof buf) { fail(p, "number too long"); return; }
    memcpy(buf, s + p->i, len);
    buf[len] = '\0';
    v->type = JSON_NUMBER;
    v->number = strtod(buf, NULL);
    p->i = i;
}

static int match_word(Parser *p, const char *w) {
    size_t len = strlen(w);
    if (p->i + len <= p->n && memcmp(p->s + p->i, w, len) == 0) { p->i += len; return 1; }
    return 0;
}

static void parse_container(Parser *p, JsonValue *v, int depth, int is_object) {
    size_t cap = 0;
    char close = is_object ? '}' : ']';
    v->type = is_object ? JSON_OBJECT : JSON_ARRAY;
    p->i++;
    skip_ws(p);
    if (p->i < p->n && p->s[p->i] == close) { p->i++; return; }
    for (;;) {
        char *key = NULL;
        if (is_object) {
            skip_ws(p);
            if (p->i >= p->n || p->s[p->i] != '"') { fail(p, "expected object key"); return; }
            key = parse_string(p);
            if (!key) return;
            skip_ws(p);
            if (p->i >= p->n || p->s[p->i] != ':') { fail(p, "expected ':'"); free(key); return; }
            p->i++;
        }
        if (v->count == cap) {
            cap = cap ? 2 * cap : 8;
            v->items = xrealloc(v->items, cap * sizeof *v->items);
            if (is_object) v->keys = xrealloc(v->keys, cap * sizeof *v->keys);
        }
        memset(&v->items[v->count], 0, sizeof *v->items);
        if (is_object) v->keys[v->count] = key;
        v->count++;
        parse_value(p, &v->items[v->count - 1], depth + 1);
        if (p->failed) return;
        skip_ws(p);
        if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
        if (p->i < p->n && p->s[p->i] == close) { p->i++; return; }
        fail(p, is_object ? "expected ',' or '}'" : "expected ',' or ']'");
        return;
    }
}

static void parse_value(Parser *p, JsonValue *v, int depth) {
    if (depth > JSON_MAX_DEPTH) { fail(p, "nesting too deep"); return; }
    skip_ws(p);
    if (p->i >= p->n) { fail(p, "unexpected end of input"); return; }
    char c = p->s[p->i];
    if (c == '{') parse_container(p, v, depth, 1);
    else if (c == '[') parse_container(p, v, depth, 0);
    else if (c == '"') {
        v->type = JSON_STRING;
        v->string = parse_string(p);
    } else if (c == '-' || isdigit((unsigned char)c)) parse_number(p, v);
    else if (match_word(p, "true")) { v->type = JSON_BOOL; v->number = 1; }
    else if (match_word(p, "false")) { v->type = JSON_BOOL; v->number = 0; }
    else if (match_word(p, "null")) v->type = JSON_NULL;
    else fail(p, "unexpected character");
}

static void free_members(JsonValue *v) {
    for (size_t k = 0; k < v->count; k++) {
        free_members(&v->items[k]);
        if (v->keys) free(v->keys[k]);
    }
    free(v->items);
    free(v->keys);
    free(v->string);
}

void json_free(JsonValue *v) {
    if (!v) return;
    free_members(v);
    free(v);
}

JsonValue *json_parse(const char *text, size_t len, char *err, size_t errlen) {
    Parser p = { text, len, 0, err, errlen, 0 };
    JsonValue *root = xrealloc(NULL, sizeof *root);
    memset(root, 0, sizeof *root);
    parse_value(&p, root, 0);
    if (!p.failed) {
        skip_ws(&p);
        if (p.i != p.n) fail(&p, "trailing content after document");
    }
    if (p.failed) { json_free(root); return NULL; }
    return root;
}

JsonValue *json_parse_file(const char *path, char *err, size_t errlen) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errlen, "cannot open %s", path); return NULL; }
    size_t cap = 1 << 20, len = 0, got;
    char *buf = xrealloc(NULL, cap);
    while ((got = fread(buf + len, 1, cap - len, f)) > 0) {
        len += got;
        if (len == cap) { cap *= 2; buf = xrealloc(buf, cap); }
    }
    int bad = ferror(f);
    fclose(f);
    if (bad) { free(buf); snprintf(err, errlen, "cannot read %s", path); return NULL; }
    JsonValue *v = json_parse(buf, len, err, errlen);
    free(buf);
    return v;
}

const JsonValue *json_get(const JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (size_t k = 0; k < obj->count; k++)
        if (strcmp(obj->keys[k], key) == 0) return &obj->items[k];
    return NULL;
}
