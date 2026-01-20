// src/parser.c
#include "parser.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
   Parser state + diagnostics
   ============================================================ */

struct Parser {
    Lexer *lx;
    int error;
    char err[512];
};

static void set_error(Parser *p, const Token *t, const char *msg) {
    if (p->error) return;
    p->error = 1;
    diag_format(p->err, (int)sizeof(p->err), "<input>", t->line, t->column, "parser error", msg);
}

static Token next_tok(Parser *p) { return lexer_next(p->lx); }
static Token peek_tok(Parser *p) { return lexer_peek(p->lx); }

/* In Phase 2 we MUST NOT skip INDENT/DEDENT globally, because they
   define blocks. We only skip NEWLINE "noise" between top-level stmts. */
static void skip_newlines(Parser *p) {
    for (;;) {
        Token t = peek_tok(p);
        if (t.type == TOKEN_NEWLINE) { next_tok(p); continue; }
        break;
    }
}

static Token expect(Parser *p, TokenType ty, const char *val_opt, const char *what) {
    Token t = next_tok(p);
    if (t.type != ty) {
        set_error(p, &t, what);
        return t;
    }
    if (val_opt && strcmp(t.value, val_opt) != 0) {
        set_error(p, &t, what);
        return t;
    }
    return t;
}

/* Conservative recovery: consume tokens until NEWLINE or DEDENT or EOF. */
static void consume_to_recovery_point(Parser *p) {
    for (;;) {
        Token t = peek_tok(p);
        if (t.type == TOKEN_EOF) return;
        if (t.type == TOKEN_NEWLINE) return;
        if (t.type == TOKEN_DEDENT) return;
        next_tok(p);
    }
}

/* ============================================================
   Expr allocation/free
   ============================================================ */

static Expr* expr_new(void) {
    return (Expr*)calloc(1, sizeof(Expr));
}

static Expr* expr_lit_int(int v, int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_LITERAL;
    e->line = line;
    e->col  = col;
    e->as.lit.lit_kind = LIT_INT;
    e->as.lit.int_value = v;
    e->as.lit.text[0] = '\0';
    return e;
}

static Expr* expr_lit_bool(int b, int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_LITERAL;
    e->line = line;
    e->col  = col;
    e->as.lit.lit_kind = LIT_BOOL;
    e->as.lit.int_value = b ? 1 : 0;
    e->as.lit.text[0] = '\0';
    return e;
}

static Expr* expr_lit_null(int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_LITERAL;
    e->line = line;
    e->col  = col;
    e->as.lit.lit_kind = LIT_NULL;
    e->as.lit.int_value = 0;
    e->as.lit.text[0] = '\0';
    return e;
}

static Expr* expr_lit_string(const char *s, int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_LITERAL;
    e->line = line;
    e->col  = col;
    e->as.lit.lit_kind = LIT_STRING;
    e->as.lit.int_value = 0;
    e->as.lit.text[0] = '\0';
    if (s) {
        strncpy(e->as.lit.text, s, NOEMA_TOKEN_VALUE_MAX - 1);
        e->as.lit.text[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';
    }
    return e;
}

static Expr* expr_var(const char *name, int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_VAR;
    e->line = line;
    e->col  = col;
    e->as.var.name[0] = '\0';
    if (name) {
        strncpy(e->as.var.name, name, NOEMA_TOKEN_VALUE_MAX - 1);
        e->as.var.name[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';
    }
    return e;
}

static Expr* expr_unary(ExprOp op, Expr *rhs, int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_UNARY;
    e->line = line;
    e->col  = col;
    e->as.unary.op = op;
    e->as.unary.rhs = rhs;
    return e;
}

static Expr* expr_binary(ExprOp op, Expr *lhs, Expr *rhs, int line, int col) {
    Expr *e = expr_new();
    if (!e) return NULL;
    e->kind = EXPR_BINARY;
    e->line = line;
    e->col  = col;
    e->as.binary.op = op;
    e->as.binary.lhs = lhs;
    e->as.binary.rhs = rhs;
    return e;
}

static void expr_free(Expr *e) {
    if (!e) return;
    if (e->kind == EXPR_UNARY) {
        expr_free(e->as.unary.rhs);
    } else if (e->kind == EXPR_BINARY) {
        expr_free(e->as.binary.lhs);
        expr_free(e->as.binary.rhs);
    }
    free(e);
}

/* ============================================================
   Expression parsing (Phase 1)
   - precedence climbing via chained functions
   ============================================================ */

static int tok_is_kw(const Token *t, const char *kw) {
    return (t->type == TOKEN_KEYWORD && strcmp(t->value, kw) == 0);
}

static int tok_is_op(const Token *t, const char *op) {
    return (t->type == TOKEN_OPERATOR && strcmp(t->value, op) == 0);
}

static int tok_is_cmp(const Token *t, const char *c) {
    return (t->type == TOKEN_COMPARATOR && strcmp(t->value, c) == 0);
}

static Expr* parse_expr(Parser *p); /* forward */

static Expr* parse_primary(Parser *p) {
    Token t = next_tok(p);

    if (t.type == TOKEN_NUMBER) {
        return expr_lit_int(atoi(t.value), t.line, t.column);
    }

    if (t.type == TOKEN_STRING) {
        return expr_lit_string(t.value, t.line, t.column);
    }

    if (t.type == TOKEN_IDENTIFIER) {
        return expr_var(t.value, t.line, t.column);
    }

    if (t.type == TOKEN_KEYWORD) {
        if (strcmp(t.value, "verum") == 0) return expr_lit_bool(1, t.line, t.column);
        if (strcmp(t.value, "falsum") == 0) return expr_lit_bool(0, t.line, t.column);
        if (strcmp(t.value, "nulla") == 0) return expr_lit_null(t.line, t.column);
    }

    if (t.type == TOKEN_PAREN && strcmp(t.value, "(") == 0) {
        Expr *inside = parse_expr(p);
        if (p->error) { expr_free(inside); return expr_lit_null(t.line, t.column); }
        expect(p, TOKEN_PAREN, ")", "expected ')' to close expression");
        return inside;
    }

    set_error(p, &t, "expected expression");
    return expr_lit_null(t.line, t.column);
}

static Expr* parse_unary(Parser *p) {
    Token t = peek_tok(p);

    if (tok_is_kw(&t, "non")) {
        next_tok(p);
        Expr *rhs = parse_unary(p);
        return expr_unary(OP_NOT, rhs, t.line, t.column);
    }

    if (tok_is_op(&t, "-")) {
        next_tok(p);
        Expr *rhs = parse_unary(p);
        return expr_unary(OP_NEG, rhs, t.line, t.column);
    }

    return parse_primary(p);
}

static Expr* parse_mul(Parser *p) {
    Expr *left = parse_unary(p);

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op;

        if (tok_is_op(&t, "*")) op = OP_MUL;
        else if (tok_is_op(&t, "/")) op = OP_DIV;
        else if (tok_is_op(&t, "%")) op = OP_MOD;
        else break;

        next_tok(p);
        Expr *right = parse_unary(p);
        left = expr_binary(op, left, right, t.line, t.column);
    }

    return left;
}

static Expr* parse_add(Parser *p) {
    Expr *left = parse_mul(p);

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op;

        if (tok_is_op(&t, "+")) op = OP_ADD;
        else if (tok_is_op(&t, "-")) op = OP_SUB;
        else break;

        next_tok(p);
        Expr *right = parse_mul(p);
        left = expr_binary(op, left, right, t.line, t.column);
    }

    return left;
}

static Expr* parse_cmp(Parser *p) {
    Expr *left = parse_add(p);

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op;

        if (tok_is_cmp(&t, "<")) op = OP_LT;
        else if (tok_is_cmp(&t, "<=")) op = OP_LE;
        else if (tok_is_cmp(&t, ">")) op = OP_GT;
        else if (tok_is_cmp(&t, ">=")) op = OP_GE;
        else break;

        next_tok(p);
        Expr *right = parse_add(p);
        left = expr_binary(op, left, right, t.line, t.column);
    }

    return left;
}

static Expr* parse_eq(Parser *p) {
    Expr *left = parse_cmp(p);

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op;

        if (tok_is_cmp(&t, "==")) op = OP_EQ;
        else if (tok_is_cmp(&t, "!=")) op = OP_NE;
        else break;

        next_tok(p);
        Expr *right = parse_cmp(p);
        left = expr_binary(op, left, right, t.line, t.column);
    }

    return left;
}

static Expr* parse_and(Parser *p) {
    Expr *left = parse_eq(p);

    for (;;) {
        Token t = peek_tok(p);
        if (!tok_is_kw(&t, "et")) break;

        next_tok(p);
        Expr *right = parse_eq(p);
        left = expr_binary(OP_AND, left, right, t.line, t.column);
    }

    return left;
}

static Expr* parse_or(Parser *p) {
    Expr *left = parse_and(p);

    for (;;) {
        Token t = peek_tok(p);
        if (!tok_is_kw(&t, "aut")) break;

        next_tok(p);
        Expr *right = parse_and(p);
        left = expr_binary(OP_OR, left, right, t.line, t.column);
    }

    return left;
}

static Expr* parse_expr(Parser *p) {
    return parse_or(p);
}

/* ============================================================
   Statement allocation
   ============================================================ */

static Stmt* new_stmt(StmtKind kind, int line, int col) {
    Stmt *s = (Stmt*)calloc(1, sizeof(Stmt));
    if (!s) return NULL;
    s->kind = kind;
    s->line = line;
    s->col = col;
    s->next = NULL;
    return s;
}

static void append_stmt(ParseResult *r, Stmt *s, Parser *p, const Token *t_for_err) {
    if (!s) {
        if (!p->error) {
            Token fake = *t_for_err;
            set_error(p, &fake, "out of memory creating statement");
        }
        return;
    }
    if (!r->first) r->first = s;
    if (r->last) r->last->next = s;
    r->last = s;
}

/* ============================================================
   IF branch allocation/free
   ============================================================ */

static IfBranch* new_if_branch(void) {
    return (IfBranch*)calloc(1, sizeof(IfBranch));
}

static void free_stmt_list(Stmt *first); /* forward */

static void free_if_branches(IfBranch *b) {
    while (b) {
        IfBranch *n = b->next;
        expr_free(b->cond);
        b->cond = NULL;
        free_stmt_list(b->body);
        b->body = NULL;
        free(b);
        b = n;
    }
}

/* ============================================================
   Parse block:
     NEWLINE INDENT <stmts> DEDENT
   ============================================================ */

static Stmt* parse_stmt(Parser *p); /* forward */

static Stmt* parse_block(Parser *p) {
    /* Expect NEWLINE then INDENT */
    expect(p, TOKEN_NEWLINE, NULL, "expected NEWLINE after ':'");
    if (p->error) return NULL;

    expect(p, TOKEN_INDENT, NULL, "expected INDENT to start block");
    if (p->error) return NULL;

    Stmt *first = NULL;
    Stmt *last  = NULL;

    /* Allow blank lines inside blocks */
    skip_newlines(p);

    for (;;) {
        Token t = peek_tok(p);

        if (t.type == TOKEN_EOF) {
            set_error(p, &t, "unexpected EOF inside block (missing dedent?)");
            break;
        }

        if (t.type == TOKEN_DEDENT) {
            next_tok(p); /* consume DEDENT */
            break;
        }

        Stmt *s = parse_stmt(p);
        if (p->error) {
            consume_to_recovery_point(p);
            /* If we stopped at NEWLINE, consume it to continue */
            if (peek_tok(p).type == TOKEN_NEWLINE) next_tok(p);
            skip_newlines(p);
            /* if we then see DEDENT, allow block to end */
            continue;
        }

        if (!first) first = s;
        else last->next = s;
        last = s;

        /* After a statement, optionally consume one NEWLINE (or many) */
        skip_newlines(p);
    }

    return first;
}

/* ============================================================
   Parse individual statements
   ============================================================ */

static void parse_import_stmt(Parser *p, ParseResult *r, Token kw) {
    Token mod = expect(p, TOKEN_IDENTIFIER, NULL, "expected module name after import");

    Stmt *s = new_stmt(STMT_IMPORT, kw.line, kw.column);
    if (s) {
        strncpy(s->module, mod.value, NOEMA_TOKEN_VALUE_MAX - 1);
        s->module[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';
    }
    append_stmt(r, s, p, &kw);

    /* Optional trailing NEWLINE handled by caller (skip_newlines) */
}

static void parse_assign_stmt(Parser *p, ParseResult *r, Token ident) {
    expect(p, TOKEN_ASSIGN, "=", "expected '=' in assignment");

    Stmt *s = new_stmt(STMT_ASSIGN, ident.line, ident.column);
    if (s) {
        strncpy(s->target, ident.value, NOEMA_TOKEN_VALUE_MAX - 1);
        s->target[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';

        s->value = parse_expr(p);
    }
    append_stmt(r, s, p, &ident);
}

static void parse_print_call(Parser *p, ParseResult *r, Token ident) {
    /* We accept IDENTIFIER token "sonus.dic" from lexer allowing '.' inside identifiers. */
    expect(p, TOKEN_PAREN, "(", "expected '(' after sonus.dic");
    Stmt *s = new_stmt(STMT_CALL_PRINT, ident.line, ident.column);
    if (s) {
        s->arg = parse_expr(p);
    }
    expect(p, TOKEN_PAREN, ")", "expected ')' after argument");
    append_stmt(r, s, p, &ident);
}

static Stmt* parse_if_stmt(Parser *p, Token kw_si) {
    /* parse: si <expr> : NEWLINE INDENT block DEDENT (aliosi ...)* (alio ...)? */

    Stmt *s = new_stmt(STMT_IF, kw_si.line, kw_si.column);
    if (!s) {
        set_error(p, &kw_si, "out of memory creating if statement");
        return NULL;
    }

    IfBranch *head = NULL;
    IfBranch *tail = NULL;

    /* --- first "si" branch --- */
    {
        IfBranch *b = new_if_branch();
        if (!b) {
            set_error(p, &kw_si, "out of memory creating if branch");
            free(s);
            return NULL;
        }

        b->cond = parse_expr(p);
        expect(p, TOKEN_COLON, ":", "expected ':' after si condition");
        if (p->error) { free_if_branches(b); free(s); return NULL; }

        b->body = parse_block(p);
        if (p->error) { free_if_branches(b); free(s); return NULL; }

        head = tail = b;
    }

    /* --- zero or more "aliosi" branches --- */
    for (;;) {
        Token t = peek_tok(p);
        if (!(t.type == TOKEN_KEYWORD && strcmp(t.value, "aliosi") == 0)) break;
        next_tok(p); /* consume aliosi */

        IfBranch *b = new_if_branch();
        if (!b) {
            set_error(p, &t, "out of memory creating aliosi branch");
            free_if_branches(head);
            free(s);
            return NULL;
        }

        b->cond = parse_expr(p);
        expect(p, TOKEN_COLON, ":", "expected ':' after aliosi condition");
        if (p->error) { free_if_branches(b); free_if_branches(head); free(s); return NULL; }

        b->body = parse_block(p);
        if (p->error) { free_if_branches(b); free_if_branches(head); free(s); return NULL; }

        tail->next = b;
        tail = b;
    }

    /* --- optional "alio" branch --- */
    {
        Token t = peek_tok(p);
        if (t.type == TOKEN_KEYWORD && strcmp(t.value, "alio") == 0) {
            next_tok(p); /* consume alio */

            IfBranch *b = new_if_branch();
            if (!b) {
                set_error(p, &t, "out of memory creating alio branch");
                free_if_branches(head);
                free(s);
                return NULL;
            }

            b->cond = NULL; /* else */
            expect(p, TOKEN_COLON, ":", "expected ':' after alio");
            if (p->error) { free_if_branches(b); free_if_branches(head); free(s); return NULL; }

            b->body = parse_block(p);
            if (p->error) { free_if_branches(b); free_if_branches(head); free(s); return NULL; }

            tail->next = b;
            tail = b;
        }
    }

    s->if_branches = head;
    return s;
}

static Stmt* parse_stmt(Parser *p) {
    Token t = peek_tok(p);

    if (t.type == TOKEN_KEYWORD && strcmp(t.value, "import") == 0) {
        Token kw = next_tok(p);
        ParseResult tmp = {0}; /* dummy just to reuse existing append pattern if needed */
        /* Here we return a stmt instead of appending directly */
        Token mod = expect(p, TOKEN_IDENTIFIER, NULL, "expected module name after import");
        Stmt *s = new_stmt(STMT_IMPORT, kw.line, kw.column);
        if (s) {
            strncpy(s->module, mod.value, NOEMA_TOKEN_VALUE_MAX - 1);
            s->module[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';
        }
        (void)tmp;
        return s;
    }

    if (t.type == TOKEN_KEYWORD && strcmp(t.value, "si") == 0) {
        Token kw_si = next_tok(p);
        return parse_if_stmt(p, kw_si);
    }

    if (t.type == TOKEN_IDENTIFIER) {
        Token ident = next_tok(p);

        if (strcmp(ident.value, "sonus.dic") == 0) {
            /* print call statement */
            expect(p, TOKEN_PAREN, "(", "expected '(' after sonus.dic");
            Stmt *s = new_stmt(STMT_CALL_PRINT, ident.line, ident.column);
            if (s) s->arg = parse_expr(p);
            expect(p, TOKEN_PAREN, ")", "expected ')' after argument");
            return s;
        }

        /* assignment */
        Token nx = peek_tok(p);
        if (nx.type == TOKEN_ASSIGN) {
            next_tok(p); /* consume '=' */
            Stmt *s = new_stmt(STMT_ASSIGN, ident.line, ident.column);
            if (s) {
                strncpy(s->target, ident.value, NOEMA_TOKEN_VALUE_MAX - 1);
                s->target[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';
                s->value = parse_expr(p);
            }
            return s;
        }

        set_error(p, &nx, "expected assignment (=) or call (sonus.dic)");
        return NULL;
    }

    /* If a DEDENT bubbles up here, it's a block end. Let caller handle it. */
    if (t.type == TOKEN_DEDENT) {
        set_error(p, &t, "unexpected DEDENT");
        return NULL;
    }

    /* Unknown token */
    Token bad = next_tok(p);
    set_error(p, &bad, "unexpected token");
    return NULL;
}

/* ============================================================
   Program parse
   ============================================================ */

Parser* parser_create(Lexer *lx) {
    if (!lx) return NULL;
    Parser *p = (Parser*)calloc(1, sizeof(Parser));
    if (!p) return NULL;
    p->lx = lx;
    p->error = 0;
    p->err[0] = '\0';
    return p;
}

void parser_destroy(Parser *p) {
    if (!p) return;
    free(p);
}

ParseResult parser_parse_program(Parser *p) {
    ParseResult r;
    memset(&r, 0, sizeof(r));
    r.ok = 0;

    if (!p) {
        snprintf(r.message, sizeof(r.message), "parser not initialized");
        return r;
    }

    skip_newlines(p);

    while (!p->error) {
        Token t = peek_tok(p);
        if (t.type == TOKEN_EOF) break;

        /* At top-level, INDENT/DEDENT are not expected */
        if (t.type == TOKEN_INDENT) {
            Token bad = next_tok(p);
            set_error(p, &bad, "unexpected INDENT at top-level");
            consume_to_recovery_point(p);
            skip_newlines(p);
            continue;
        }
        if (t.type == TOKEN_DEDENT) {
            Token bad = next_tok(p);
            set_error(p, &bad, "unexpected DEDENT at top-level");
            consume_to_recovery_point(p);
            skip_newlines(p);
            continue;
        }

        Stmt *s = parse_stmt(p);
        if (p->error) {
            consume_to_recovery_point(p);
            if (peek_tok(p).type == TOKEN_NEWLINE) next_tok(p);
            skip_newlines(p);
            continue;
        }

        append_stmt(&r, s, p, &t);

        /* Consume any trailing newlines */
        skip_newlines(p);
    }

    if (lexer_has_error(p->lx)) {
        snprintf(r.message, sizeof(r.message), "lexer error: %s", lexer_error_message(p->lx));
        r.ok = 0;
        return r;
    }

    if (p->error) {
        snprintf(r.message, sizeof(r.message), "%s", p->err);
        r.ok = 0;
        return r;
    }

    r.ok = 1;
    r.message[0] = '\0';
    return r;
}

/* ============================================================
   Free program
   ============================================================ */

static void free_stmt_list(Stmt *first) {
    Stmt *s = first;
    while (s) {
        Stmt *n = s->next;

        if (s->kind == STMT_ASSIGN) {
            expr_free(s->value);
            s->value = NULL;
        } else if (s->kind == STMT_CALL_PRINT) {
            expr_free(s->arg);
            s->arg = NULL;
        } else if (s->kind == STMT_IF) {
            free_if_branches(s->if_branches);
            s->if_branches = NULL;
        }

        free(s);
        s = n;
    }
}

void parser_free_program(Stmt *first) {
    free_stmt_list(first);
}

