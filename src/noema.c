// src/noema.c
#include "noema.h"
#include "lexer.h"
#include "parser.h"
#include "runtime.h"

#include <string.h>
#include <stdio.h>

static void dump_tokens(FILE *f, const char *path) {
    Lexer *lx = lexer_create(f, path);
    for (;;) {
        Token t = lexer_next(lx);
        printf("%d:%d  %-11s  %s\n", t.line, t.column, token_type_name(t.type), t.value);
        if (t.type == TOKEN_EOF) break;
        if (lexer_has_error(lx)) break;
    }
    if (lexer_has_error(lx)) {
        fprintf(stderr, "%s\n", lexer_error_message(lx));
    }
    lexer_destroy(lx);
}

static void dump_ast(const ParseResult *pr) {
    for (Stmt *s = pr->first; s; s = s->next) {
        if (s->kind == STMT_IMPORT) {
            printf("IMPORT %s\n", s->module);
        } else if (s->kind == STMT_ASSIGN) {
            printf("ASSIGN %s = ", s->target);
            if (s->value.kind == EXPR_INT) printf("%d", s->value.int_value);
            else if (s->value.kind == EXPR_BOOL) printf("%s", s->value.int_value ? "verum" : "falsum");
            else printf("%s", s->value.text);
            printf("\n");
        } else if (s->kind == STMT_CALL_PRINT) {
            printf("CALL sonus.dic(");
            if (s->arg.kind == EXPR_INT) printf("%d", s->arg.int_value);
            else if (s->arg.kind == EXPR_BOOL) printf("%s", s->arg.int_value ? "verum" : "falsum");
            else printf("%s", s->arg.text);
            printf(")\n");
        }
    }
}

NoemaResult noema_run_file(FILE *f, const char *path, const NoemaOptions *opt) {
    NoemaResult r;
    memset(&r, 0, sizeof(r));
    r.ok = 0;
    r.message[0] = '\0';

    if (opt && opt->dump_tokens) {
        dump_tokens(f, path);
        r.ok = 1;
        return r;
    }

    Lexer *lx = lexer_create(f, path);
    if (!lx) {
        snprintf(r.message, sizeof(r.message), "noema: cannot create lexer");
        return r;
    }

    Parser *ps = parser_create(lx);
    if (!ps) {
        lexer_destroy(lx);
        snprintf(r.message, sizeof(r.message), "noema: cannot create parser");
        return r;
    }

    ParseResult pr = parser_parse_program(ps);

    if (!pr.ok) {
        snprintf(r.message, sizeof(r.message), "%s", pr.message);
        parser_free_program(pr.first);
        parser_destroy(ps);
        lexer_destroy(lx);
        return r;
    }

    if (opt && opt->dump_ast) {
        dump_ast(&pr);
        r.ok = 1;
        parser_free_program(pr.first);
        parser_destroy(ps);
        lexer_destroy(lx);
        return r;
    }

    Runtime *rt = runtime_create();
    if (!rt) {
        snprintf(r.message, sizeof(r.message), "noema: cannot create runtime");
        parser_free_program(pr.first);
        parser_destroy(ps);
        lexer_destroy(lx);
        return r;
    }

    char rt_err[512];
    rt_err[0] = '\0';

    int ok = runtime_exec(rt, pr.first, path, rt_err, (int)sizeof(rt_err));
    runtime_destroy(rt);

    if (!ok) {
        snprintf(r.message, sizeof(r.message), "%s", rt_err[0] ? rt_err : "runtime error");
        r.ok = 0;
    } else {
        r.ok = 1;
    }

    parser_free_program(pr.first);
    parser_destroy(ps);
    lexer_destroy(lx);
    return r;
}

