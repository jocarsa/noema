#ifndef NOEMA_LEXER_H
#define NOEMA_LEXER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NOEMA_TOKEN_VALUE_MAX 256

typedef enum {
    TOKEN_EOF = 0,

    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_KEYWORD,

    TOKEN_OPERATOR,
    TOKEN_COMPARATOR,
    TOKEN_ASSIGN,

    TOKEN_PAREN,
    TOKEN_BRACKET,
    TOKEN_COLON,
    TOKEN_COMMA,

    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT
} TokenType;

typedef struct {
    TokenType type;
    char value[NOEMA_TOKEN_VALUE_MAX];
    int line;
    int column;
} Token;

typedef struct Lexer Lexer;

Lexer* lexer_create(FILE *f, const char *path);
void   lexer_destroy(Lexer *lx);

Token  lexer_next(Lexer *lx);
Token  lexer_peek(Lexer *lx);

int         lexer_has_error(const Lexer *lx);
const char* lexer_error_message(const Lexer *lx);

const char* token_type_name(TokenType t);

#ifdef __cplusplus
}
#endif

#endif

