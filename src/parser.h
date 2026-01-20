#ifndef NOEMA_PARSER_H
#define NOEMA_PARSER_H

#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Parser Parser;

typedef enum {
    STMT_IMPORT = 1,
    STMT_ASSIGN,
    STMT_CALL_PRINT
} StmtKind;

typedef enum {
    EXPR_INT = 1,
    EXPR_STRING,
    EXPR_IDENT,
    EXPR_BOOL,
    EXPR_NULL
} ExprKind;

typedef struct Expr {
    ExprKind kind;
    char text[NOEMA_TOKEN_VALUE_MAX];
    int  int_value;
} Expr;

typedef struct Stmt {
    StmtKind kind;
    int line, col;

    char module[NOEMA_TOKEN_VALUE_MAX];

    char target[NOEMA_TOKEN_VALUE_MAX];
    Expr value;

    Expr arg;

    struct Stmt *next;
} Stmt;

typedef struct {
    int ok;
    char message[512];
    Stmt *first;
    Stmt *last;
} ParseResult;

Parser*     parser_create(Lexer *lx);
void        parser_destroy(Parser *p);

ParseResult parser_parse_program(Parser *p);
void        parser_free_program(Stmt *first);

#ifdef __cplusplus
}
#endif

#endif

