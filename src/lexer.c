#include "lexer.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_LINE_LENGTH 1024
#define INDENT_SPACES   4
#define INDENT_STACK_MAX 256

struct Lexer {
    FILE *f;
    const char *path;

    char linebuf[MAX_LINE_LENGTH];
    int  line_len;
    int  pos;
    int  line_num;

    int  indent_stack[INDENT_STACK_MAX];
    int  indent_top;
    int  pending_indents;
    int  pending_dedents;

    int  paren_depth;

    int  has_peek;
    Token peek_tok;

    int  error;
    char err[512];
};

static Token make_tok(TokenType t, const char *val, int line, int col) {
    Token tok;
    tok.type = t;
    tok.line = line;
    tok.column = col;
    tok.value[0] = '\0';
    if (val) {
        strncpy(tok.value, val, NOEMA_TOKEN_VALUE_MAX - 1);
        tok.value[NOEMA_TOKEN_VALUE_MAX - 1] = '\0';
    }
    return tok;
}

static void set_error(struct Lexer *lx, int line, int col, const char *msg) {
    if (lx->error) return;
    lx->error = 1;
    diag_format(lx->err, (int)sizeof(lx->err), lx->path, line, col, "lexer error", msg);
}

static int read_next_line(struct Lexer *lx) {
    if (!fgets(lx->linebuf, (int)sizeof(lx->linebuf), lx->f)) {
        lx->linebuf[0] = '\0';
        lx->line_len = 0;
        return 0;
    }
    lx->line_num++;
    lx->pos = 0;

    lx->line_len = (int)strlen(lx->linebuf);
    if (lx->line_len >= 2 &&
        lx->linebuf[lx->line_len - 2] == '\r' &&
        lx->linebuf[lx->line_len - 1] == '\n') {
        lx->linebuf[lx->line_len - 2] = '\n';
        lx->linebuf[lx->line_len - 1] = '\0';
        lx->line_len -= 1;
    }
    return 1;
}

static int peek_ch(struct Lexer *lx) {
    if (lx->pos >= lx->line_len) return EOF;
    return (unsigned char)lx->linebuf[lx->pos];
}

static int next_ch(struct Lexer *lx) {
    if (lx->pos >= lx->line_len) return EOF;
    return (unsigned char)lx->linebuf[lx->pos++];
}

static void skip_inline_ws(struct Lexer *lx) {
    int c = peek_ch(lx);
    while (c == ' ' || c == '\t') {
        if (c == '\t') {
            set_error(lx, lx->line_num, lx->pos + 1, "tab character is not allowed (use 4 spaces)");
            return;
        }
        next_ch(lx);
        c = peek_ch(lx);
    }
}

static int is_line_blank_or_comment(const char *s) {
    int i = 0;
    while (s[i] == ' ') i++;
    if (s[i] == '\n' || s[i] == '\0') return 1;
    if (s[i] == '#') return 1;
    return 0;
}

static int count_indent_spaces(struct Lexer *lx) {
    int count = 0;
    while (1) {
        int c = peek_ch(lx);
        if (c == ' ') { count++; next_ch(lx); continue; }
        if (c == '\t') {
            set_error(lx, lx->line_num, lx->pos + 1, "tab character is not allowed (use 4 spaces)");
            return 0;
        }
        break;
    }
    return count;
}

static int is_keyword(const char *s) {
    static const char *kw[] = {
        "si","aliosi","alio",
        "pro","dum","frange","perge",
        "munus","redit",
        "conare","nisi","denique","iacta",
        "import",
        "verum","falsum","nulla",
        "et","aut","non",
        "in",
        NULL
    };
    for (int i = 0; kw[i]; i++) {
        if (strcmp(s, kw[i]) == 0) return 1;
    }
    return 0;
}

static Token lex_number(struct Lexer *lx, int start_col) {
    char buf[NOEMA_TOKEN_VALUE_MAX];
    int n = 0;
    int c = peek_ch(lx);

    while (isdigit(c)) {
        if (n < (int)sizeof(buf) - 1) buf[n++] = (char)c;
        next_ch(lx);
        c = peek_ch(lx);
    }
    buf[n] = '\0';
    return make_tok(TOKEN_NUMBER, buf, lx->line_num, start_col);
}

static Token lex_identifier_or_keyword(struct Lexer *lx, int start_col) {
    char buf[NOEMA_TOKEN_VALUE_MAX];
    int n = 0;
    int c = peek_ch(lx);

    while (isalnum(c) || c == '_' || c == '.') {
        if (n < (int)sizeof(buf) - 1) buf[n++] = (char)c;
        next_ch(lx);
        c = peek_ch(lx);
    }
    buf[n] = '\0';

    if (is_keyword(buf)) return make_tok(TOKEN_KEYWORD, buf, lx->line_num, start_col);
    return make_tok(TOKEN_IDENTIFIER, buf, lx->line_num, start_col);
}

static Token lex_string(struct Lexer *lx, int start_col) {
    char buf[NOEMA_TOKEN_VALUE_MAX];
    int n = 0;

    next_ch(lx); // opening "

    while (1) {
        int c = peek_ch(lx);
        if (c == EOF || c == '\n') {
            set_error(lx, lx->line_num, start_col, "unterminated string literal");
            return make_tok(TOKEN_STRING, "", lx->line_num, start_col);
        }
        if (c == '"') {
            next_ch(lx); // closing "
            break;
        }
        if (n < (int)sizeof(buf) - 1) buf[n++] = (char)c;
        next_ch(lx);
    }

    buf[n] = '\0';
    return make_tok(TOKEN_STRING, buf, lx->line_num, start_col);
}

static Token lex_operator_or_punct(struct Lexer *lx, int start_col) {
    int c = next_ch(lx);

    if (c == '=') {
        if (peek_ch(lx) == '=') { next_ch(lx); return make_tok(TOKEN_COMPARATOR, "==", lx->line_num, start_col); }
        return make_tok(TOKEN_ASSIGN, "=", lx->line_num, start_col);
    }
    if (c == '!') {
        if (peek_ch(lx) == '=') { next_ch(lx); return make_tok(TOKEN_COMPARATOR, "!=", lx->line_num, start_col); }
        set_error(lx, lx->line_num, start_col, "unexpected '!'");
        return make_tok(TOKEN_OPERATOR, "!", lx->line_num, start_col);
    }
    if (c == '<') {
        if (peek_ch(lx) == '=') { next_ch(lx); return make_tok(TOKEN_COMPARATOR, "<=", lx->line_num, start_col); }
        return make_tok(TOKEN_COMPARATOR, "<", lx->line_num, start_col);
    }
    if (c == '>') {
        if (peek_ch(lx) == '=') { next_ch(lx); return make_tok(TOKEN_COMPARATOR, ">=", lx->line_num, start_col); }
        return make_tok(TOKEN_COMPARATOR, ">", lx->line_num, start_col);
    }

    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
        char v[2] = { (char)c, '\0' };
        return make_tok(TOKEN_OPERATOR, v, lx->line_num, start_col);
    }

    if (c == '(' || c == ')') {
        char v[2] = { (char)c, '\0' };
        if (c == '(') lx->paren_depth++;
        else if (lx->paren_depth > 0) lx->paren_depth--;
        return make_tok(TOKEN_PAREN, v, lx->line_num, start_col);
    }
    if (c == '[' || c == ']') {
        char v[2] = { (char)c, '\0' };
        return make_tok(TOKEN_BRACKET, v, lx->line_num, start_col);
    }
    if (c == ':') return make_tok(TOKEN_COLON, ":", lx->line_num, start_col);
    if (c == ',') return make_tok(TOKEN_COMMA, ",", lx->line_num, start_col);

    {
        char msg[64];
        snprintf(msg, sizeof(msg), "unexpected character '%c'", (char)c);
        set_error(lx, lx->line_num, start_col, msg);
    }
    return make_tok(TOKEN_OPERATOR, "?", lx->line_num, start_col);
}

static Token next_token_internal(struct Lexer *lx) {
    if (lx->error) return make_tok(TOKEN_EOF, "", lx->line_num, lx->pos + 1);

    if (lx->pending_indents > 0) {
        lx->pending_indents--;
        return make_tok(TOKEN_INDENT, "INDENT", lx->line_num, 1);
    }
    if (lx->pending_dedents > 0) {
        lx->pending_dedents--;
        return make_tok(TOKEN_DEDENT, "DEDENT", lx->line_num, 1);
    }

    while (lx->line_len == 0 || lx->pos >= lx->line_len) {
        if (!read_next_line(lx)) {
            if (lx->indent_top > 0) {
                lx->pending_dedents = lx->indent_top;
                lx->indent_top = 0;
                return make_tok(TOKEN_DEDENT, "DEDENT", lx->line_num, 1);
            }
            return make_tok(TOKEN_EOF, "", lx->line_num, 1);
        }

        if (is_line_blank_or_comment(lx->linebuf)) {
            lx->pos = lx->line_len;
            continue;
        }

        if (lx->paren_depth == 0) {
            int old_indent = lx->indent_stack[lx->indent_top];

            int spaces = count_indent_spaces(lx);
            if (lx->error) return make_tok(TOKEN_EOF, "", lx->line_num, 1);

            if (spaces % INDENT_SPACES != 0) {
                set_error(lx, lx->line_num, 1, "indentation must be multiple of 4 spaces");
                return make_tok(TOKEN_EOF, "", lx->line_num, 1);
            }

            int new_indent = spaces / INDENT_SPACES;

            if (new_indent > old_indent) {
                if (lx->indent_top + 1 >= INDENT_STACK_MAX) {
                    set_error(lx, lx->line_num, 1, "indent stack overflow");
                    return make_tok(TOKEN_EOF, "", lx->line_num, 1);
                }
                lx->indent_top++;
                lx->indent_stack[lx->indent_top] = new_indent;
                lx->pending_indents = (new_indent - old_indent) - 1;
                return make_tok(TOKEN_INDENT, "INDENT", lx->line_num, 1);
            }

            if (new_indent < old_indent) {
                int pops = 0;
                while (lx->indent_top > 0 && lx->indent_stack[lx->indent_top] > new_indent) {
                    lx->indent_top--;
                    pops++;
                }
                if (lx->indent_stack[lx->indent_top] != new_indent) {
                    set_error(lx, lx->line_num, 1, "inconsistent dedent");
                    return make_tok(TOKEN_EOF, "", lx->line_num, 1);
                }
                lx->pending_dedents = pops - 1;
                return make_tok(TOKEN_DEDENT, "DEDENT", lx->line_num, 1);
            }
        } else {
            skip_inline_ws(lx);
        }

        break;
    }

    skip_inline_ws(lx);
    if (lx->error) return make_tok(TOKEN_EOF, "", lx->line_num, lx->pos + 1);

    int c = peek_ch(lx);
    int col = lx->pos + 1;

    if (c == '#') {
        lx->pos = lx->line_len;
        if (lx->paren_depth == 0) return make_tok(TOKEN_NEWLINE, "NEWLINE", lx->line_num, col);
        return next_token_internal(lx);
    }

    if (c == '\n' || c == EOF) {
        if (c == '\n') next_ch(lx);
        if (lx->paren_depth == 0) return make_tok(TOKEN_NEWLINE, "NEWLINE", lx->line_num, col);
        return next_token_internal(lx);
    }

    if (c == '"') return lex_string(lx, col);
    if (isdigit(c)) return lex_number(lx, col);
    if (isalpha(c) || c == '_') return lex_identifier_or_keyword(lx, col);

    return lex_operator_or_punct(lx, col);
}

Lexer* lexer_create(FILE *f, const char *path) {
    if (!f) return NULL;

    struct Lexer *lx = (struct Lexer*)calloc(1, sizeof(struct Lexer));
    if (!lx) return NULL;

    lx->f = f;
    lx->path = path;

    lx->linebuf[0] = '\0';
    lx->line_len = 0;
    lx->pos = 0;
    lx->line_num = 0;

    lx->indent_top = 0;
    lx->indent_stack[0] = 0;

    lx->pending_indents = 0;
    lx->pending_dedents = 0;

    lx->paren_depth = 0;

    lx->has_peek = 0;
    lx->error = 0;
    lx->err[0] = '\0';

    return (Lexer*)lx;
}

void lexer_destroy(Lexer *lx_) {
    struct Lexer *lx = (struct Lexer*)lx_;
    if (!lx) return;
    free(lx);
}

Token lexer_next(Lexer *lx_) {
    struct Lexer *lx = (struct Lexer*)lx_;
    if (!lx) return make_tok(TOKEN_EOF, "", 0, 0);

    if (lx->has_peek) {
        lx->has_peek = 0;
        return lx->peek_tok;
    }
    return next_token_internal(lx);
}

Token lexer_peek(Lexer *lx_) {
    struct Lexer *lx = (struct Lexer*)lx_;
    if (!lx) return make_tok(TOKEN_EOF, "", 0, 0);

    if (!lx->has_peek) {
        lx->peek_tok = next_token_internal(lx);
        lx->has_peek = 1;
    }
    return lx->peek_tok;
}

int lexer_has_error(const Lexer *lx_) {
    const struct Lexer *lx = (const struct Lexer*)lx_;
    return lx ? lx->error : 1;
}

const char* lexer_error_message(const Lexer *lx_) {
    const struct Lexer *lx = (const struct Lexer*)lx_;
    if (!lx) return "lexer not initialized";
    return lx->err;
}

const char* token_type_name(TokenType t) {
    switch (t) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_KEYWORD: return "KEYWORD";
        case TOKEN_OPERATOR: return "OPERATOR";
        case TOKEN_COMPARATOR: return "COMPARATOR";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_PAREN: return "PAREN";
        case TOKEN_BRACKET: return "BRACKET";
        case TOKEN_COLON: return "COLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        default: return "UNKNOWN";
    }
}

