#ifndef NOEMA_PARSER_H
#define NOEMA_PARSER_H

#include "lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Parser Parser;

/* =========================
   Statements
   ========================= */

typedef enum {
    STMT_IMPORT = 1,
    STMT_ASSIGN,
    STMT_CALL_PRINT,
    STMT_IF
} StmtKind;

/* =========================
   Expressions (Phase 1)
   ========================= */

typedef enum {
    EXPR_LITERAL = 1,
    EXPR_VAR,
    EXPR_UNARY,
    EXPR_BINARY
} ExprKind;

typedef enum {
    LIT_INT = 1,
    LIT_STRING,
    LIT_BOOL,
    LIT_NULL
} LiteralKind;

typedef enum {
    OP_ADD = 1,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,

    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,

    OP_AND,
    OP_OR,

    OP_NOT,
    OP_NEG
} ExprOp;

typedef struct Expr Expr;

struct Expr {
    ExprKind kind;
    int line;
    int col;

    union {
        struct {
            LiteralKind lit_kind;
            int int_value;                          // for int/bool
            char text[NOEMA_TOKEN_VALUE_MAX];       // for string
        } lit;

        struct {
            char name[NOEMA_TOKEN_VALUE_MAX];       // variable name
        } var;

        struct {
            ExprOp op;
            Expr *rhs;
        } unary;

        struct {
            ExprOp op;
            Expr *lhs;
            Expr *rhs;
        } binary;

    } as;
};

/* =========================
   IF branches (si/aliosi/alio)
   ========================= */

typedef struct IfBranch {
    Expr *cond;                 // NULL means 'alio' (else)
    struct Stmt *body;          // linked list of statements in this block
    struct IfBranch *next;
} IfBranch;

/* =========================
   AST nodes
   ========================= */

typedef struct Stmt {
    StmtKind kind;
    int line, col;

    // import
    char module[NOEMA_TOKEN_VALUE_MAX];

    // assign
    char target[NOEMA_TOKEN_VALUE_MAX];
    Expr *value;

    // print call
    Expr *arg;

    // if
    IfBranch *if_branches;

    struct Stmt *next;
} Stmt;

typedef struct {
    int ok;
    char message[512];
    Stmt *first;
    Stmt *last;
} ParseResult;

/* =========================
   API
   ========================= */

Parser*     parser_create(Lexer *lx);
void        parser_destroy(Parser *p);

ParseResult parser_parse_program(Parser *p);
void        parser_free_program(Stmt *first);

#ifdef __cplusplus
}
#endif

#endif

