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

/* ============================================================
   AST dump helpers
   ============================================================ */

static const char* op_name(ExprOp op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_EQ:  return "==";
        case OP_NE:  return "!=";
        case OP_LT:  return "<";
        case OP_LE:  return "<=";
        case OP_GT:  return ">";
        case OP_GE:  return ">=";
        case OP_AND: return "et";
        case OP_OR:  return "aut";
        case OP_NOT: return "non";
        case OP_NEG: return "neg";
        default:     return "?";
    }
}

static void dump_expr(const Expr *e) {
    if (!e) { printf("<null-expr>"); return; }

    switch (e->kind) {
        case EXPR_LITERAL:
            if (e->as.lit.lit_kind == LIT_INT) {
                printf("%d", e->as.lit.int_value);
            } else if (e->as.lit.lit_kind == LIT_BOOL) {
                printf("%s", e->as.lit.int_value ? "verum" : "falsum");
            } else if (e->as.lit.lit_kind == LIT_NULL) {
                printf("nulla");
            } else if (e->as.lit.lit_kind == LIT_STRING) {
                printf("\"%s\"", e->as.lit.text);
            } else {
                printf("<lit?>");
            }
            return;

        case EXPR_VAR:
            printf("%s", e->as.var.name);
            return;

        case EXPR_UNARY:
            if (e->as.unary.op == OP_NOT) {
                printf("non ");
                dump_expr(e->as.unary.rhs);
            } else if (e->as.unary.op == OP_NEG) {
                printf("(-");
                dump_expr(e->as.unary.rhs);
                printf(")");
            } else {
                printf("(%s ", op_name(e->as.unary.op));
                dump_expr(e->as.unary.rhs);
                printf(")");
            }
            return;

        case EXPR_BINARY:
            printf("(");
            dump_expr(e->as.binary.lhs);
            printf(" %s ", op_name(e->as.binary.op));
            dump_expr(e->as.binary.rhs);
            printf(")");
            return;

        default:
            printf("<expr?>");
            return;
    }
}

static void indent_n(int n) {
    for (int i = 0; i < n; i++) putchar(' ');
}

static void dump_stmt_list(const Stmt *s, int ind);

static void dump_if(const Stmt *s, int ind) {
    const IfBranch *b = s->if_branches;
    int first = 1;

    while (b) {
        indent_n(ind);
        if (first) {
            printf("SI ");
            if (b->cond) dump_expr(b->cond); else printf("<missing-cond>");
            printf(":\n");
            first = 0;
        } else if (b->cond) {
            printf("ALIOSI ");
            dump_expr(b->cond);
            printf(":\n");
        } else {
            printf("ALIO:\n");
        }

        dump_stmt_list(b->body, ind + 2);
        b = b->next;
    }
}

static void dump_stmt_list(const Stmt *s, int ind) {
    for (; s; s = s->next) {
        switch (s->kind) {
            case STMT_IMPORT:
                indent_n(ind);
                printf("IMPORT %s\n", s->module);
                break;

            case STMT_ASSIGN:
                indent_n(ind);
                printf("ASSIGN %s = ", s->target);
                dump_expr(s->value);
                printf("\n");
                break;

            case STMT_CALL_PRINT:
                indent_n(ind);
                printf("CALL sonus.dic(");
                dump_expr(s->arg);
                printf(")\n");
                break;

            case STMT_IF:
                dump_if(s, ind);
                break;

            default:
                indent_n(ind);
                printf("UNKNOWN_STMT\n");
                break;
        }
    }
}

static void dump_ast(const ParseResult *pr) {
    dump_stmt_list(pr->first, 0);
}

/* ============================================================
   Public entry
   ============================================================ */

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

