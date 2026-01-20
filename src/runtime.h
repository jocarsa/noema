// src/runtime.h
#ifndef NOEMA_RUNTIME_H
#define NOEMA_RUNTIME_H

#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VAL_INT = 1,
    VAL_STRING,
    VAL_BOOL,
    VAL_NULL
} ValueKind;

typedef struct {
    ValueKind kind;
    int int_value;          // for int/bool
    char *string_value;     // for string (heap), NULL otherwise
} Value;

typedef struct Runtime Runtime;

Runtime* runtime_create(void);
void     runtime_destroy(Runtime *rt);

// Added `path` so diagnostics show real filename instead of "<input>"
int      runtime_exec(Runtime *rt, Stmt *program, const char *path, char *err_out, int err_cap);

#ifdef __cplusplus
}
#endif

#endif

