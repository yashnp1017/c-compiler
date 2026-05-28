/*
 * lexer.h — Lexical analysis: source text → token stream
 *
 * Supported tokens:
 *   Keywords:    int, return, if, else, while, for
 *   Literals:    integer constants
 *   Identifiers: variable and function names
 *   Operators:   + - * / % == != < <= > >= && || ! = += -= *= /=
 *   Punctuation: ( ) { } ; ,
 */

#ifndef LEXER_H
#define LEXER_H

/* ── Token types ─────────────────────────────────────────────────────────── */

typedef enum {
    /* Literals */
    TOK_INT_LIT,        /* 42 */
    TOK_IDENT,          /* foo, bar */

    /* Keywords */
    TOK_INT,            /* int */
    TOK_RETURN,         /* return */
    TOK_IF,             /* if */
    TOK_ELSE,           /* else */
    TOK_WHILE,          /* while */
    TOK_FOR,            /* for */

    /* Arithmetic */
    TOK_PLUS,           /* + */
    TOK_MINUS,          /* - */
    TOK_STAR,           /* * */
    TOK_SLASH,          /* / */
    TOK_PERCENT,        /* % */

    /* Comparison */
    TOK_EQ,             /* == */
    TOK_NEQ,            /* != */
    TOK_LT,             /* < */
    TOK_LE,             /* <= */
    TOK_GT,             /* > */
    TOK_GE,             /* >= */

    /* Logical */
    TOK_AND,            /* && */
    TOK_OR,             /* || */
    TOK_BANG,           /* ! */

    /* Assignment */
    TOK_ASSIGN,         /* = */
    TOK_PLUS_ASSIGN,    /* += */
    TOK_MINUS_ASSIGN,   /* -= */
    TOK_STAR_ASSIGN,    /* *= */
    TOK_SLASH_ASSIGN,   /* /= */

    /* Punctuation */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_LBRACE,         /* { */
    TOK_RBRACE,         /* } */
    TOK_SEMICOLON,      /* ; */
    TOK_COMMA,          /* , */

    TOK_EOF,
} TokenType;

/* ── Token ───────────────────────────────────────────────────────────────── */

typedef struct {
    TokenType   type;
    char       *text;   /* heap-allocated source text of this token */
    int         line;
    int         col;
} Token;

/* ── Token list ──────────────────────────────────────────────────────────── */

typedef struct {
    Token  *tokens;
    int     count;
    int     capacity;
    int     pos;        /* current read position (used by parser) */
} TokenList;

/* ── Public API ──────────────────────────────────────────────────────────── */

TokenList *lex(const char *source);
void       free_tokens(TokenList *tl);
Token     *peek(TokenList *tl);
Token     *advance(TokenList *tl);
Token     *expect(TokenList *tl, TokenType type);
const char *token_type_name(TokenType t);

#endif /* LEXER_H */
