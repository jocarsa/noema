#ifndef NOEMA_H
#define NOEMA_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int dump_tokens;  // lexer debug
    int dump_ast;     // parser debug
    int trace_exec;   // runtime debug (reserved)
    int show_help;    // internal
    int bad_args;     // internal
} NoemaOptions;

typedef struct {
    int ok;                 // 1 success, 0 error
    char message[512];      // error summary
} NoemaResult;

NoemaResult noema_run_file(FILE *f, const char *path, const NoemaOptions *opt);

#ifdef __cplusplus
}
#endif

#endif

