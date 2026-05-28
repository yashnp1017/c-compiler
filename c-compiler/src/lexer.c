/*
 * lexer.c — Lexical analysis implementation
 *
 * Single-pass scanner. Handles:
 *   - Integer literals
 *   - Identifiers and keyword recognition
 *   - All operators (including multi-char: ==, !=, <=, >=, &&, ||, +=, etc.)
 *   - Line/column tracking for error messages
 *   - Single-line and block comments
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"

/* ── Internal helpers ────────────────────────────────────────────────────── */

static void tl_push(TokenList *tl, Token tok) {
    if (tl->count >= tl->capacity) {
        tl->capacity = tl->capacity ? tl->capacity * 2 : 64;
        tl->tokens = realloc(tl->tokens, tl->capacity * sizeof(Token));
    }
    tl->tokens[tl->count++] = tok;
}

static Token make_token(TokenType type, const char *text, int line, int col) {
    Token t;
    t.type = type;
    t.text = strdup(text);
    t.line = line;
    t.col  = col;
    return t;
}

static void lex_error(const char *msg, int line, int col) {
    fprintf(stderr, "lex error (line %d, col %d): %s\n", line, col, msg);
    exit(1);
}

/* ── Keyword table ───────────────────────────────────────────────────────── */

typedef struct { const char *kw; TokenType type; } KwEntry;
static const KwEntry keywords[] = {
    {"int",    TOK_INT},
    {"return", TOK_RETURN},
    {"if",     TOK_IF},
    {"else",   TOK_ELSE},
    {"while",  TOK_WHILE},
    {"for",    TOK_FOR},
    {NULL, 0},
};

static TokenType lookup_keyword(const char *ident) {
    for (int i = 0; keywords[i].kw; i++) {
        if (strcmp(ident, keywords[i].kw) == 0)
            return keywords[i].type;
    }
    return TOK_IDENT;
}

/* ── Main lexer ──────────────────────────────────────────────────────────── */

TokenList *lex(const char *src) {
    TokenList *tl = calloc(1, sizeof(TokenList));
    int i = 0, line = 1, col = 1;
    int n = strlen(src);

    while (i < n) {
        char c = src[i];

        /* Skip whitespace */
        if (isspace(c)) {
            if (c == '\n') { line++; col = 1; } else { col++; }
            i++; continue;
        }

        /* Skip single-line comments */
        if (c == '/' && i + 1 < n && src[i+1] == '/') {
            while (i < n && src[i] != '\n') i++;
            continue;
        }

        /* Skip block comments */
        if (c == '/' && i + 1 < n && src[i+1] == '*') {
            i += 2; col += 2;
            while (i + 1 < n && !(src[i] == '*' && src[i+1] == '/')) {
                if (src[i] == '\n') { line++; col = 1; } else col++;
                i++;
            }
            if (i + 1 >= n) lex_error("unterminated block comment", line, col);
            i += 2; col += 2;
            continue;
        }

        int tok_col = col;

        /* Integer literal */
        if (isdigit(c)) {
            int start = i;
            while (i < n && isdigit(src[i])) { i++; col++; }
            char buf[64];
            int len = i - start;
            snprintf(buf, sizeof(buf), "%.*s", len, src + start);
            tl_push(tl, make_token(TOK_INT_LIT, buf, line, tok_col));
            continue;
        }

        /* Identifier or keyword */
        if (isalpha(c) || c == '_') {
            int start = i;
            while (i < n && (isalnum(src[i]) || src[i] == '_')) { i++; col++; }
            char buf[256];
            int len = i - start;
            snprintf(buf, sizeof(buf), "%.*s", len, src + start);
            TokenType type = lookup_keyword(buf);
            tl_push(tl, make_token(type, buf, line, tok_col));
            continue;
        }

        /* Multi-char and single-char operators */
i++; col++;
#define PEEK1 (i < n ? src[i] : 0)
        switch (c) {
            case '(': tl_push(tl, make_token(TOK_LPAREN,    "(", line, tok_col)); break;
            case ')': tl_push(tl, make_token(TOK_RPAREN,    ")", line, tok_col)); break;
            case '{': tl_push(tl, make_token(TOK_LBRACE,    "{", line, tok_col)); break;
            case '}': tl_push(tl, make_token(TOK_RBRACE,    "}", line, tok_col)); break;
            case ';': tl_push(tl, make_token(TOK_SEMICOLON, ";", line, tok_col)); break;
            case ',': tl_push(tl, make_token(TOK_COMMA,     ",", line, tok_col)); break;
            case '%': tl_push(tl, make_token(TOK_PERCENT,   "%", line, tok_col)); break;
            case '!':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_NEQ,  "!=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_BANG, "!",  line, tok_col)); }
                break;
            case '=':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_EQ,     "==", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_ASSIGN, "=",  line, tok_col)); }
                break;
            case '<':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_LE, "<=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_LT, "<",  line, tok_col)); }
                break;
            case '>':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_GE, ">=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_GT, ">",  line, tok_col)); }
                break;
            case '+':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_PLUS_ASSIGN,  "+=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_PLUS,         "+",  line, tok_col)); }
                break;
            case '-':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_MINUS_ASSIGN, "-=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_MINUS,        "-",  line, tok_col)); }
                break;
            case '*':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_STAR_ASSIGN,  "*=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_STAR,         "*",  line, tok_col)); }
                break;
            case '/':
                if (PEEK1 == '=') { tl_push(tl, make_token(TOK_SLASH_ASSIGN, "/=", line, tok_col)); i++; col++; }
                else               { tl_push(tl, make_token(TOK_SLASH,        "/",  line, tok_col)); }
                break;
            case '&':
                if (PEEK1 == '&') { tl_push(tl, make_token(TOK_AND, "&&", line, tok_col)); i++; col++; }
                else { lex_error("expected '&&'", line, tok_col); }
                break;
            case '|':
                if (PEEK1 == '|') { tl_push(tl, make_token(TOK_OR, "||", line, tok_col)); i++; col++; }
                else { lex_error("expected '||'", line, tok_col); }
                break;
            default: {
                char msg[64];
                snprintf(msg, sizeof(msg), "unexpected character '%c'", c);
                lex_error(msg, line, tok_col);
            }
        }
#undef PEEK1
    }

    tl_push(tl, make_token(TOK_EOF, "", line, col));
    return tl;
}

/* ── Token stream navigation ─────────────────────────────────────────────── */

Token *peek(TokenList *tl) {
    if (tl->pos >= tl->count) return &tl->tokens[tl->count - 1]; /* EOF */
    return &tl->tokens[tl->pos];
}

Token *advance(TokenList *tl) {
    Token *t = peek(tl);
    if (t->type != TOK_EOF) tl->pos++;
    return t;
}

Token *expect(TokenList *tl, TokenType type) {
    Token *t = peek(tl);
    if (t->type != type) {
        fprintf(stderr, "parse error (line %d, col %d): expected '%s', got '%s'\n",
                t->line, t->col, token_type_name(type), token_type_name(t->type));
        exit(1);
    }
    return advance(tl);
}

void free_tokens(TokenList *tl) {
    for (int i = 0; i < tl->count; i++) free(tl->tokens[i].text);
    free(tl->tokens);
    free(tl);
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_INT_LIT: return "INT_LIT";
        case TOK_IDENT:   return "IDENT";
        case TOK_INT:     return "int";
        case TOK_RETURN:  return "return";
        case TOK_IF:      return "if";
        case TOK_ELSE:    return "else";
        case TOK_WHILE:   return "while";
        case TOK_FOR:     return "for";
        case TOK_PLUS:    return "+";
        case TOK_MINUS:   return "-";
        case TOK_STAR:    return "*";
        case TOK_SLASH:   return "/";
        case TOK_PERCENT: return "%";
        case TOK_EQ:      return "==";
        case TOK_NEQ:     return "!=";
        case TOK_LT:      return "<";
        case TOK_LE:      return "<=";
        case TOK_GT:      return ">";
        case TOK_GE:      return ">=";
        case TOK_AND:     return "&&";
        case TOK_OR:      return "||";
        case TOK_BANG:    return "!";
        case TOK_ASSIGN:  return "=";
        case TOK_PLUS_ASSIGN:  return "+=";
        case TOK_MINUS_ASSIGN: return "-=";
        case TOK_STAR_ASSIGN:  return "*=";
        case TOK_SLASH_ASSIGN: return "/=";
        case TOK_LPAREN:    return "(";
        case TOK_RPAREN:    return ")";
        case TOK_LBRACE:    return "{";
        case TOK_RBRACE:    return "}";
        case TOK_SEMICOLON: return ";";
        case TOK_COMMA:     return ",";
        case TOK_EOF:       return "EOF";
        default:            return "?";
    }
}
