/* Minimal JSON reader: parses a whole document into a tree. */
#ifndef UC_JSON_H
#define UC_JSON_H

#include <stddef.h>

typedef enum { JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT } JsonType;

typedef struct JsonValue {
    JsonType type;
    double number;            /* JSON_NUMBER; JSON_BOOL as 0 or 1 */
    char *string;             /* JSON_STRING, UTF-8 */
    size_t count;             /* JSON_ARRAY, JSON_OBJECT: number of members */
    struct JsonValue *items;  /* members, in document order */
    char **keys;              /* JSON_OBJECT: key of each member */
} JsonValue;

/* Parse a NUL-free text buffer. Returns NULL and fills err on failure. */
JsonValue *json_parse(const char *text, size_t len, char *err, size_t errlen);
/* Read and parse a file. Returns NULL and fills err on failure. */
JsonValue *json_parse_file(const char *path, char *err, size_t errlen);
void json_free(JsonValue *v);

/* Object member by key, or NULL. */
const JsonValue *json_get(const JsonValue *obj, const char *key);

#endif
