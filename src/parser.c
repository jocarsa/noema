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
    // Mantén line/col reales
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

static Expr parse_expr_simple(Parser *p) {
    Expr e;
    memset(&e, 0, sizeof(e));

    Token t = next_tok(p);

    if (t.type == TOKEN_NUMBER) {
        e.kind = EXPR_INT;
        e.int_value = atoi(t.value);
        strncpy(e.text, t.value, sizeof(e.text) - 1);
        return e;
    }

    if (t.type == TOKEN_STRING) {
        e.kind = EXPR_STRING;
        strncpy(e.text, t.value, sizeof(e.text) - 1);
        return e;
    }

    if (t.type == TOKEN_IDENTIFIER) {
        e.kind = EXPR_IDENT;
        strncpy(e.text, t.value, sizeof(e.text) - 1);
        return e;
    }

    if (t.type == TOKEN_KEYWORD) {
        if (strcmp(t.value, "verum") == 0) {
            e.kind = EXPR_BOOL;
            e.int_value = 1;
            strncpy(e.text, t.value, sizeof(e.text) - 1);
            return e;
        }
        if (strcmp(t.value, "falsum") == 0) {
            e.kind = EXPR_BOOL;
            e.int_value = 0;
            strncpy(e.text, t.value, sizeof(e.text) - 1);
            return e;
        }
        if (strcmp(t.value, "nulla") == 0) {
            e.kind = EXPR_NULL;
            strncpy(e.text, t.value, sizeof(e.text) - 1);
            return e;
        }
    }

    set_error(p, &t, "expected expression (number, string, identifier, verum/falsum/nulla)");
    e.kind = EXPR_NULL;
    strncpy(e.text, "nulla", sizeof(e.text) - 1);
    return e;
}

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

static void consume_to_newline(Parser *p) {
    for (;;) {
        Token t = peek_tok(p);
        if (t.type == TOKEN_EOF || t.type == TOKEN_NEWLINE) return;
        next_tok(p);
    }
}

static void parse_import_stmt(Parser *p, ParseResult *r, Token kw_import) {
    Token mod = expect(p, TOKEN_IDENTIFIER, NULL, "expected module name after 'import'");
    if (p->error) return;

    Stmt *s = new_stmt(STMT_IMPORT, kw_import.line, kw_import.column);
    if (!s) { set_error(p, &kw_import, "out of memory"); return; }
    strncpy(s->module, mod.value, sizeof(s->module) - 1);
    append_stmt(r, s, p, &kw_import);
}

static void parse_assign_stmt(Parser *p, ParseResult *r, Token ident) {
    expect(p, TOKEN_ASSIGN, "=", "expected '=' after identifier");
    if (p->error) return;

    Expr rhs = parse_expr_simple(p);
    if (p->error) return;

    Stmt *s = new_stmt(STMT_ASSIGN, ident.line, ident.column);
    if (!s) { set_error(p, &ident, "out of memory"); return; }
    strncpy(s->target, ident.value, sizeof(s->target) - 1);
    s->value = rhs;
    append_stmt(r, s, p, &ident);
}

static void parse_print_call(Parser *p, ParseResult *r, Token ident) {
    expect(p, TOKEN_PAREN, "(", "expected '(' after sonus.dic");
    if (p->error) return;

    Expr arg = parse_expr_simple(p);
    if (p->error) return;

    expect(p, TOKEN_PAREN, ")", "expected ')' after argument");
    if (p->error) return;

    Stmt *s = new_stmt(STMT_CALL_PRINT, ident.line, ident.column);
    if (!s) { set_error(p, &ident, "out of memory"); return; }
    s->arg = arg;
    append_stmt(r, s, p, &ident);
}

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
        free(s);
        s = n;
    }
}

