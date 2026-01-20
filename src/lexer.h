#ifndef NOEMA_LEXER_H
#define NOEMA_LEXER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NOEMA_TOKEN_VALUE_MAX
#define NOEMA_TOKEN_VALUE_MAX 256
#endif

typedef enum {
    TOKEN_INVALID = 0,

    /* Structural tokens (Phase 2) */
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    TOKEN_COLON,

    /* Core tokens */
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_KEYWORD,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_ASSIGN,       /* = */
    TOKEN_OPERATOR,     /* + - * / % */
    TOKEN_COMPARATOR,   /* == != < <= > >= */

    TOKEN_PAREN         /* ( or ) */
} TokenType;

typedef struct {
    TokenType type;
    int line;
    int column;
    char value[NOEMA_TOKEN_VALUE_MAX];
} Token;

typedef struct Lexer Lexer;

/* Create/destroy */
Lexer* lexer_create(FILE *f, const char *path);
void   lexer_destroy(Lexer *lx);

/* Token stream */
Token  lexer_next(Lexer *lx);
Token  lexer_peek(Lexer *lx);

/* Error handling */
int         lexer_has_error(Lexer *lx);
const char* lexer_error_message(Lexer *lx);

/* Debug helper */
const char* token_type_name(TokenType t);

#ifdef __cplusplus
}
#endif

#endif

