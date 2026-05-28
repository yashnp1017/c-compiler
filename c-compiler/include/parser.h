/*
 * parser.h — Abstract Syntax Tree node types and parser API
 *
 * The AST represents a full C program. Node types cover:
 *   - Programs (list of function declarations)
 *   - Functions (name, params, body)
 *   - Statements: return, if/else, while, for, block, expression, variable decl
 *   - Expressions: binary ops, unary ops, assignment, function calls, literals, identifiers
 */

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

/* ── AST node kinds ──────────────────────────────────────────────────────── */

typedef enum {
    /* Top-level */
    NODE_PROGRAM,       /* list of function declarations */
    NODE_FUNC_DECL,     /* int name(params) { body } */

    /* Statements */
    NODE_BLOCK,         /* { stmt* } */
    NODE_RETURN,        /* return expr; */
    NODE_IF,            /* if (cond) then [else alt] */
    NODE_WHILE,         /* while (cond) body */
    NODE_FOR,           /* for (init; cond; post) body */
    NODE_VAR_DECL,      /* int name [= expr]; */
    NODE_EXPR_STMT,     /* expr; */

    /* Expressions */
    NODE_INT_LIT,       /* 42 */
    NODE_IDENT,         /* x */
    NODE_ASSIGN,        /* x = expr */
    NODE_BINOP,         /* expr op expr */
    NODE_UNOP,          /* !expr, -expr */
    NODE_CALL,          /* name(args) */
} NodeKind;

/* ── AST node ────────────────────────────────────────────────────────────── */

typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeKind kind;
    int line;

    union {
        /* NODE_PROGRAM */
        struct { ASTNode **funcs; int nfuncs; } program;

        /* NODE_FUNC_DECL */
        struct {
            char    *name;
            char   **params;  /* parameter names */
            int      nparams;
            ASTNode *body;    /* NODE_BLOCK */
        } func;

        /* NODE_BLOCK */
        struct { ASTNode **stmts; int nstmts; } block;

        /* NODE_RETURN */
        struct { ASTNode *expr; } ret;

        /* NODE_IF */
        struct { ASTNode *cond; ASTNode *then; ASTNode *alt; } if_stmt;

        /* NODE_WHILE */
        struct { ASTNode *cond; ASTNode *body; } while_stmt;

        /* NODE_FOR */
        struct {
            ASTNode *init;
            ASTNode *cond;
            ASTNode *post;
            ASTNode *body;
        } for_stmt;

        /* NODE_VAR_DECL */
        struct { char *name; ASTNode *init; } var_decl;

        /* NODE_EXPR_STMT */
        struct { ASTNode *expr; } expr_stmt;

        /* NODE_INT_LIT */
        struct { long value; } int_lit;

        /* NODE_IDENT */
        struct { char *name; } ident;

        /* NODE_ASSIGN */
        struct { char *name; ASTNode *value; TokenType op; } assign;

        /* NODE_BINOP */
        struct { TokenType op; ASTNode *left; ASTNode *right; } binop;

        /* NODE_UNOP */
        struct { TokenType op; ASTNode *operand; } unop;

        /* NODE_CALL */
        struct { char *name; ASTNode **args; int nargs; } call;
    };
};

/* ── Public API ──────────────────────────────────────────────────────────── */

ASTNode *parse(TokenList *tl);
void     free_ast(ASTNode *node);
void     print_ast(ASTNode *node, int indent);  /* for debugging */

#endif /* PARSER_H */
