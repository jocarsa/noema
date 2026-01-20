#include "parser.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

static void skip_noise(Parser *p) {
    for (;;) {
        Token t = peek_tok(p);
        if (t.type == TOKEN_NEWLINE || t.type == TOKEN_INDENT || t.type == TOKEN_DEDENT) {
            next_tok(p);
            continue;
        }
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

static void consume_to_newline(Parser *p) {
    for (;;) {
        Token t = peek_tok(p);
        if (t.type == TOKEN_EOF || t.type == TOKEN_NEWLINE) return;
        next_tok(p);
    }
}

/* =========================
   Expr allocation/free
   ========================= */

static Expr* expr_new(void) {
    Expr *e = (Expr*)calloc(1, sizeof(Expr));
    return e;
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

/* =========================
   Expression parsing
   precedence:
   unary: non, -
   mul: * / %
   add: + -
   cmp: < <= > >=
   eq:  == !=
   and: et
   or:  aut
   ========================= */

static int tok_is_kw(Parser *p, const char *s) {
    Token t = peek_tok(p);
    return (t.type == TOKEN_KEYWORD && strcmp(t.value, s) == 0);
}

static int tok_is_op(Parser *p, const char *s) {
    Token t = peek_tok(p);
    return (t.type == TOKEN_OPERATOR && strcmp(t.value, s) == 0);
}

static int tok_is_cmp(Parser *p, const char *s) {
    Token t = peek_tok(p);
    return (t.type == TOKEN_COMPARATOR && strcmp(t.value, s) == 0);
}

static Expr* parse_expr(Parser *p); // forward

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
        if (p->error) { expr_free(inside); return NULL; }
        expect(p, TOKEN_PAREN, ")", "expected ')' after expression");
        if (p->error) { expr_free(inside); return NULL; }
        return inside;
    }

    set_error(p, &t, "expected expression");
    return expr_lit_null(t.line, t.column);
}

static Expr* parse_unary(Parser *p) {
    Token t = peek_tok(p);

    if (t.type == TOKEN_KEYWORD && strcmp(t.value, "non") == 0) {
        next_tok(p);
        Expr *rhs = parse_unary(p);
        if (p->error) { expr_free(rhs); return NULL; }
        return expr_unary(OP_NOT, rhs, t.line, t.column);
    }

    if (t.type == TOKEN_OPERATOR && strcmp(t.value, "-") == 0) {
        next_tok(p);
        Expr *rhs = parse_unary(p);
        if (p->error) { expr_free(rhs); return NULL; }
        return expr_unary(OP_NEG, rhs, t.line, t.column);
    }

    return parse_primary(p);
}

static Expr* parse_mul(Parser *p) {
    Expr *lhs = parse_unary(p);
    if (p->error) { expr_free(lhs); return NULL; }

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op = 0;

        if (t.type == TOKEN_OPERATOR && strcmp(t.value, "*") == 0) op = OP_MUL;
        else if (t.type == TOKEN_OPERATOR && strcmp(t.value, "/") == 0) op = OP_DIV;
        else if (t.type == TOKEN_OPERATOR && strcmp(t.value, "%") == 0) op = OP_MOD;
        else break;

        next_tok(p);
        Expr *rhs = parse_unary(p);
        if (p->error) { expr_free(lhs); expr_free(rhs); return NULL; }

        Expr *node = expr_binary(op, lhs, rhs, t.line, t.column);
        if (!node) {
            set_error(p, &t, "out of memory");
            expr_free(lhs);
            expr_free(rhs);
            return NULL;
        }
        lhs = node;
    }

    return lhs;
}

static Expr* parse_add(Parser *p) {
    Expr *lhs = parse_mul(p);
    if (p->error) { expr_free(lhs); return NULL; }

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op = 0;

        if (t.type == TOKEN_OPERATOR && strcmp(t.value, "+") == 0) op = OP_ADD;
        else if (t.type == TOKEN_OPERATOR && strcmp(t.value, "-") == 0) op = OP_SUB;
        else break;

        next_tok(p);
        Expr *rhs = parse_mul(p);
        if (p->error) { expr_free(lhs); expr_free(rhs); return NULL; }

        Expr *node = expr_binary(op, lhs, rhs, t.line, t.column);
        if (!node) {
            set_error(p, &t, "out of memory");
            expr_free(lhs);
            expr_free(rhs);
            return NULL;
        }
        lhs = node;
    }

    return lhs;
}

static Expr* parse_cmp(Parser *p) {
    Expr *lhs = parse_add(p);
    if (p->error) { expr_free(lhs); return NULL; }

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op = 0;

        if (t.type == TOKEN_COMPARATOR && strcmp(t.value, "<") == 0) op = OP_LT;
        else if (t.type == TOKEN_COMPARATOR && strcmp(t.value, "<=") == 0) op = OP_LE;
        else if (t.type == TOKEN_COMPARATOR && strcmp(t.value, ">") == 0) op = OP_GT;
        else if (t.type == TOKEN_COMPARATOR && strcmp(t.value, ">=") == 0) op = OP_GE;
        else break;

        next_tok(p);
        Expr *rhs = parse_add(p);
        if (p->error) { expr_free(lhs); expr_free(rhs); return NULL; }

        Expr *node = expr_binary(op, lhs, rhs, t.line, t.column);
        if (!node) {
            set_error(p, &t, "out of memory");
            expr_free(lhs);
            expr_free(rhs);
            return NULL;
        }
        lhs = node;
    }

    return lhs;
}

static Expr* parse_eq(Parser *p) {
    Expr *lhs = parse_cmp(p);
    if (p->error) { expr_free(lhs); return NULL; }

    for (;;) {
        Token t = peek_tok(p);
        ExprOp op = 0;

        if (t.type == TOKEN_COMPARATOR && strcmp(t.value, "==") == 0) op = OP_EQ;
        else if (t.type == TOKEN_COMPARATOR && strcmp(t.value, "!=") == 0) op = OP_NE;
        else break;

        next_tok(p);
        Expr *rhs = parse_cmp(p);
        if (p->error) { expr_free(lhs); expr_free(rhs); return NULL; }

        Expr *node = expr_binary(op, lhs, rhs, t.line, t.column);
        if (!node) {
            set_error(p, &t, "out of memory");
            expr_free(lhs);
            expr_free(rhs);
            return NULL;
        }
        lhs = node;
    }

    return lhs;
}

static Expr* parse_and(Parser *p) {
    Expr *lhs = parse_eq(p);
    if (p->error) { expr_free(lhs); return NULL; }

    while (tok_is_kw(p, "et")) {
        Token t = next_tok(p);
        Expr *rhs = parse_eq(p);
        if (p->error) { expr_free(lhs); expr_free(rhs); return NULL; }

        Expr *node = expr_binary(OP_AND, lhs, rhs, t.line, t.column);
        if (!node) {
            set_error(p, &t, "out of memory");
            expr_free(lhs);
            expr_free(rhs);
            return NULL;
        }
        lhs = node;
    }

    return lhs;
}

static Expr* parse_or(Parser *p) {
    Expr *lhs = parse_and(p);
    if (p->error) { expr_free(lhs); return NULL; }

    while (tok_is_kw(p, "aut")) {
        Token t = next_tok(p);
        Expr *rhs = parse_and(p);
        if (p->error) { expr_free(lhs); expr_free(rhs); return NULL; }

        Expr *node = expr_binary(OP_OR, lhs, rhs, t.line, t.column);
        if (!node) {
            set_error(p, &t, "out of memory");
            expr_free(lhs);
            expr_free(rhs);
            return NULL;
        }
        lhs = node;
    }

    return lhs;
}

static Expr* parse_expr(Parser *p) {
    return parse_or(p);
}

/* =========================
   Statements
   ========================= */

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

static void parse_import_stmt(Parser *p, ParseResult *r, Token kw_import) {
    Token mod = expect(p, TOKEN_IDENTIFIER, NULL, "expected module name after 'import'");
    if (p->error) return;

    Stmt *s = new_stmt(STMT_IMPORT, kw_import.line, kw_import.column);
    if (!s) { set_error(p, &kw_import, "out of memory"); return; }
    strncpy(s->module, mod.value, sizeof(s->module) - 1);
    s->module[sizeof(s->module) - 1] = '\0';
    append_stmt(r, s, p, &kw_import);
}

static void parse_assign_stmt(Parser *p, ParseResult *r, Token ident) {
    expect(p, TOKEN_ASSIGN, "=", "expected '=' after identifier");
    if (p->error) return;

    Expr *rhs = parse_expr(p);
    if (p->error) { expr_free(rhs); return; }

    Stmt *s = new_stmt(STMT_ASSIGN, ident.line, ident.column);
    if (!s) { set_error(p, &ident, "out of memory"); expr_free(rhs); return; }

    strncpy(s->target, ident.value, sizeof(s->target) - 1);
    s->target[sizeof(s->target) - 1] = '\0';
    s->value = rhs;

    append_stmt(r, s, p, &ident);
}

static void parse_print_call(Parser *p, ParseResult *r, Token ident) {
    expect(p, TOKEN_PAREN, "(", "expected '(' after sonus.dic");
    if (p->error) return;

    Expr *arg = parse_expr(p);
    if (p->error) { expr_free(arg); return; }

    expect(p, TOKEN_PAREN, ")", "expected ')' after argument");
    if (p->error) { expr_free(arg); return; }

    Stmt *s = new_stmt(STMT_CALL_PRINT, ident.line, ident.column);
    if (!s) { set_error(p, &ident, "out of memory"); expr_free(arg); return; }
    s->arg = arg;
    append_stmt(r, s, p, &ident);
}

/* =========================
   Public API
   ========================= */

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

    if (!p || lexer_has_error(p->lx)) {
        snprintf(r.message, sizeof(r.message), "lexer error: %s", lexer_error_message(p->lx));
        return r;
    }

    while (!p->error) {
        skip_noise(p);

        Token t = peek_tok(p);
        if (t.type == TOKEN_EOF) break;

        if (t.type == TOKEN_KEYWORD && strcmp(t.value, "import") == 0) {
            Token kw = next_tok(p);
            parse_import_stmt(p, &r, kw);
        }
        else if (t.type == TOKEN_IDENTIFIER) {
            Token ident = next_tok(p);

            if (strcmp(ident.value, "sonus.dic") == 0) {
                parse_print_call(p, &r, ident);
            } else {
                Token nx = peek_tok(p);
                if (nx.type == TOKEN_ASSIGN) {
                    parse_assign_stmt(p, &r, ident);
                } else {
                    set_error(p, &nx, "expected assignment or call");
                }
            }
        }
        else {
            Token bad = next_tok(p);
            set_error(p, &bad, "unexpected token");
        }

        if (p->error) {
            consume_to_newline(p);
        }

        skip_noise(p);
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

void parser_free_program(Stmt *first) {
    Stmt *s = first;
    while (s) {
        Stmt *n = s->next;

        if (s->kind == STMT_ASSIGN) {
            expr_free(s->value);
            s->value = NULL;
        }
        if (s->kind == STMT_CALL_PRINT) {
            expr_free(s->arg);
            s->arg = NULL;
        }

        free(s);
        s = n;
    }
}

