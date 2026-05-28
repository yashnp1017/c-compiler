/*
 * parser.c — Recursive descent parser
 *
 * Implements a Pratt-style precedence-climbing expression parser
 * and recursive descent for statements and declarations.
 *
 * Grammar (simplified):
 *   program     = func_decl*
 *   func_decl   = "int" IDENT "(" params ")" block
 *   params      = ("int" IDENT ("," "int" IDENT)*)?
 *   block       = "{" stmt* "}"
 *   stmt        = return_stmt | if_stmt | while_stmt | for_stmt
 *               | var_decl | expr_stmt | block
 *   expr        = assignment (lowest precedence)
 *   assignment  = IDENT ("=" | "+=" | ...) expr | logical_or
 *   logical_or  = logical_and ("||" logical_and)*
 *   logical_and = equality   ("&&" equality)*
 *   equality    = relational (("==" | "!=") relational)*
 *   relational  = additive   (("<" | "<=" | ">" | ">=") additive)*
 *   additive    = multiplicative (("+" | "-") multiplicative)*
 *   multiplicative = unary   (("*" | "/" | "%") unary)*
 *   unary       = ("!" | "-") unary | postfix
 *   postfix     = primary ("(" args ")")?   -- function call
 *   primary     = INT_LIT | IDENT | "(" expr ")"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

/* ── AST allocation helpers ──────────────────────────────────────────────── */

static ASTNode *new_node(NodeKind kind, int line) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->kind = kind;
    n->line = line;
    return n;
}

static void parse_error(const char *msg, Token *t) {
    fprintf(stderr, "parse error (line %d, col %d): %s (got '%s')\n",
            t->line, t->col, msg, t->text);
    exit(1);
}

/* ── Forward declarations ────────────────────────────────────────────────── */

static ASTNode *parse_expr(TokenList *tl);
static ASTNode *parse_stmt(TokenList *tl);
static ASTNode *parse_block(TokenList *tl);

/* ── Expression parser (Pratt-style precedence climbing) ─────────────────── */

static ASTNode *parse_primary(TokenList *tl) {
    Token *t = peek(tl);

    if (t->type == TOK_INT_LIT) {
        advance(tl);
        ASTNode *n = new_node(NODE_INT_LIT, t->line);
        n->int_lit.value = atol(t->text);
        return n;
    }

    if (t->type == TOK_IDENT) {
        advance(tl);
        ASTNode *n = new_node(NODE_IDENT, t->line);
        n->ident.name = strdup(t->text);
        return n;
    }

    if (t->type == TOK_LPAREN) {
        advance(tl); /* consume '(' */
        ASTNode *inner = parse_expr(tl);
        expect(tl, TOK_RPAREN);
        return inner;
    }

    parse_error("expected primary expression", t);
    return NULL;
}

static ASTNode *parse_postfix(TokenList *tl) {
    ASTNode *base = parse_primary(tl);

    /* Function call: IDENT "(" args ")" */
    if (base->kind == NODE_IDENT && peek(tl)->type == TOK_LPAREN) {
        Token *name_tok = peek(tl);  /* already consumed, use base */
        int line = base->line;
        char *name = strdup(base->ident.name);
        free(base->ident.name);
        free(base);

        advance(tl); /* consume '(' */

        /* Parse arguments */
        ASTNode **args = NULL;
        int nargs = 0, cap = 0;

        if (peek(tl)->type != TOK_RPAREN) {
            do {
                if (peek(tl)->type == TOK_COMMA) advance(tl);
                if (nargs >= cap) {
                    cap = cap ? cap * 2 : 4;
                    args = realloc(args, cap * sizeof(ASTNode *));
                }
                args[nargs++] = parse_expr(tl);
            } while (peek(tl)->type == TOK_COMMA);
        }
        expect(tl, TOK_RPAREN);

        ASTNode *call = new_node(NODE_CALL, line);
        call->call.name  = name;
        call->call.args  = args;
        call->call.nargs = nargs;
        return call;
    }

    return base;
}

static ASTNode *parse_unary(TokenList *tl) {
    Token *t = peek(tl);

    if (t->type == TOK_BANG || t->type == TOK_MINUS) {
        advance(tl);
        ASTNode *n = new_node(NODE_UNOP, t->line);
        n->unop.op      = t->type;
        n->unop.operand = parse_unary(tl);
        return n;
    }

    return parse_postfix(tl);
}

static int is_multiplicative(TokenType t) {
    return t == TOK_STAR || t == TOK_SLASH || t == TOK_PERCENT;
}
static int is_additive(TokenType t) {
    return t == TOK_PLUS || t == TOK_MINUS;
}
static int is_relational(TokenType t) {
    return t == TOK_LT || t == TOK_LE || t == TOK_GT || t == TOK_GE;
}
static int is_equality(TokenType t) {
    return t == TOK_EQ || t == TOK_NEQ;
}

#define PARSE_BINOP(name, next_fn, pred)                        \
static ASTNode *name(TokenList *tl) {                           \
    ASTNode *left = next_fn(tl);                                \
    while (pred(peek(tl)->type)) {                              \
        Token *op = advance(tl);                                \
        ASTNode *n = new_node(NODE_BINOP, op->line);            \
        n->binop.op    = op->type;                              \
        n->binop.left  = left;                                  \
        n->binop.right = next_fn(tl);                          \
        left = n;                                               \
    }                                                           \
    return left;                                                \
}

PARSE_BINOP(parse_multiplicative, parse_unary,          is_multiplicative)
PARSE_BINOP(parse_additive,       parse_multiplicative,  is_additive)
PARSE_BINOP(parse_relational,     parse_additive,        is_relational)
PARSE_BINOP(parse_equality,       parse_relational,      is_equality)

static ASTNode *parse_logical_and(TokenList *tl) {
    ASTNode *left = parse_equality(tl);
    while (peek(tl)->type == TOK_AND) {
        Token *op = advance(tl);
        ASTNode *n = new_node(NODE_BINOP, op->line);
        n->binop.op    = TOK_AND;
        n->binop.left  = left;
        n->binop.right = parse_equality(tl);
        left = n;
    }
    return left;
}

static ASTNode *parse_logical_or(TokenList *tl) {
    ASTNode *left = parse_logical_and(tl);
    while (peek(tl)->type == TOK_OR) {
        Token *op = advance(tl);
        ASTNode *n = new_node(NODE_BINOP, op->line);
        n->binop.op    = TOK_OR;
        n->binop.left  = left;
        n->binop.right = parse_logical_and(tl);
        left = n;
    }
    return left;
}

static int is_assign_op(TokenType t) {
    return t == TOK_ASSIGN || t == TOK_PLUS_ASSIGN ||
           t == TOK_MINUS_ASSIGN || t == TOK_STAR_ASSIGN || t == TOK_SLASH_ASSIGN;
}

static ASTNode *parse_assign(TokenList *tl) {
    /*
     * Two-token lookahead: detect IDENT immediately followed by assignment op.
     * We index directly into tl->tokens to avoid consuming any token.
     * This avoids the backtrack bug where consuming IDENT then peeking
     * a multi-char op (like <=) that has already been lexed as one token
     * would leave an orphaned partial consumption.
     */
    int cur  = tl->pos;
    int next = cur + 1;

    if (cur  < tl->count && tl->tokens[cur].type  == TOK_IDENT &&
        next < tl->count && is_assign_op(tl->tokens[next].type)) {

        Token *t  = advance(tl); /* consume IDENT */
        Token *op = advance(tl); /* consume assignment op */
        char  *name = strdup(t->text);
        ASTNode *val = parse_expr(tl);
        ASTNode *n = new_node(NODE_ASSIGN, t->line);
        n->assign.name  = name;
        n->assign.op    = op->type;
        n->assign.value = val;
        return n;
    }

    return parse_logical_or(tl);
}

static ASTNode *parse_expr(TokenList *tl) {
    return parse_assign(tl);
}

/* ── Statement parser ────────────────────────────────────────────────────── */

static ASTNode *parse_return(TokenList *tl) {
    Token *t = expect(tl, TOK_RETURN);
    ASTNode *n = new_node(NODE_RETURN, t->line);
    n->ret.expr = parse_expr(tl);
    expect(tl, TOK_SEMICOLON);
    return n;
}

static ASTNode *parse_if(TokenList *tl) {
    Token *t = expect(tl, TOK_IF);
    ASTNode *n = new_node(NODE_IF, t->line);
    expect(tl, TOK_LPAREN);
    n->if_stmt.cond = parse_expr(tl);
    expect(tl, TOK_RPAREN);
    n->if_stmt.then = parse_stmt(tl);
    n->if_stmt.alt  = NULL;
    if (peek(tl)->type == TOK_ELSE) {
        advance(tl);
        n->if_stmt.alt = parse_stmt(tl);
    }
    return n;
}

static ASTNode *parse_while(TokenList *tl) {
    Token *t = expect(tl, TOK_WHILE);
    ASTNode *n = new_node(NODE_WHILE, t->line);
    expect(tl, TOK_LPAREN);
    n->while_stmt.cond = parse_expr(tl);
    expect(tl, TOK_RPAREN);
    n->while_stmt.body = parse_stmt(tl);
    return n;
}

static ASTNode *parse_for(TokenList *tl) {
    Token *t = expect(tl, TOK_FOR);
    ASTNode *n = new_node(NODE_FOR, t->line);
    expect(tl, TOK_LPAREN);

    /* init: var decl or expr stmt or empty */
    if (peek(tl)->type == TOK_INT) {
        advance(tl);
        Token *name = expect(tl, TOK_IDENT);
        ASTNode *init = new_node(NODE_VAR_DECL, name->line);
        init->var_decl.name = strdup(name->text);
        init->var_decl.init = NULL;
        if (peek(tl)->type == TOK_ASSIGN) {
            advance(tl);
            init->var_decl.init = parse_expr(tl);
        }
        expect(tl, TOK_SEMICOLON);
        n->for_stmt.init = init;
    } else if (peek(tl)->type != TOK_SEMICOLON) {
        ASTNode *init = new_node(NODE_EXPR_STMT, peek(tl)->line);
        init->expr_stmt.expr = parse_expr(tl);
        expect(tl, TOK_SEMICOLON);
        n->for_stmt.init = init;
    } else {
        advance(tl); /* empty init */
        n->for_stmt.init = NULL;
    }

    n->for_stmt.cond = (peek(tl)->type != TOK_SEMICOLON) ? parse_expr(tl) : NULL;
    expect(tl, TOK_SEMICOLON);
    n->for_stmt.post = (peek(tl)->type != TOK_RPAREN) ? parse_expr(tl) : NULL;
    expect(tl, TOK_RPAREN);
    n->for_stmt.body = parse_stmt(tl);
    return n;
}

static ASTNode *parse_var_decl(TokenList *tl) {
    expect(tl, TOK_INT);
    Token *name = expect(tl, TOK_IDENT);
    ASTNode *n = new_node(NODE_VAR_DECL, name->line);
    n->var_decl.name = strdup(name->text);
    n->var_decl.init = NULL;
    if (peek(tl)->type == TOK_ASSIGN) {
        advance(tl);
        n->var_decl.init = parse_expr(tl);
    }
    expect(tl, TOK_SEMICOLON);
    return n;
}

static ASTNode *parse_stmt(TokenList *tl) {
    Token *t = peek(tl);
    switch (t->type) {
        case TOK_RETURN: return parse_return(tl);
        case TOK_IF:     return parse_if(tl);
        case TOK_WHILE:  return parse_while(tl);
        case TOK_FOR:    return parse_for(tl);
        case TOK_INT:    return parse_var_decl(tl);
        case TOK_LBRACE: return parse_block(tl);
        default: {
            ASTNode *n = new_node(NODE_EXPR_STMT, t->line);
            n->expr_stmt.expr = parse_expr(tl);
            expect(tl, TOK_SEMICOLON);
            return n;
        }
    }
}

static ASTNode *parse_block(TokenList *tl) {
    Token *t = expect(tl, TOK_LBRACE);
    ASTNode *n = new_node(NODE_BLOCK, t->line);

    ASTNode **stmts = NULL;
    int nstmts = 0, cap = 0;

    while (peek(tl)->type != TOK_RBRACE && peek(tl)->type != TOK_EOF) {
        if (nstmts >= cap) {
            cap = cap ? cap * 2 : 8;
            stmts = realloc(stmts, cap * sizeof(ASTNode *));
        }
        stmts[nstmts++] = parse_stmt(tl);
    }
    expect(tl, TOK_RBRACE);

    n->block.stmts  = stmts;
    n->block.nstmts = nstmts;
    return n;
}

/* ── Function declaration parser ─────────────────────────────────────────── */

static ASTNode *parse_func_decl(TokenList *tl) {
    Token *t = expect(tl, TOK_INT);
    Token *name = expect(tl, TOK_IDENT);
    ASTNode *n = new_node(NODE_FUNC_DECL, t->line);
    n->func.name = strdup(name->text);

    expect(tl, TOK_LPAREN);

    char **params = NULL;
    int nparams = 0, cap = 0;

    if (peek(tl)->type != TOK_RPAREN) {
        do {
            if (nparams > 0) expect(tl, TOK_COMMA);
            expect(tl, TOK_INT);
            Token *pname = expect(tl, TOK_IDENT);
            if (nparams >= cap) {
                cap = cap ? cap * 2 : 4;
                params = realloc(params, cap * sizeof(char *));
            }
            params[nparams++] = strdup(pname->text);
        } while (peek(tl)->type == TOK_COMMA);
    }
    expect(tl, TOK_RPAREN);

    n->func.params  = params;
    n->func.nparams = nparams;
    n->func.body    = parse_block(tl);
    return n;
}

/* ── Top-level parse ─────────────────────────────────────────────────────── */

ASTNode *parse(TokenList *tl) {
    ASTNode *program = new_node(NODE_PROGRAM, 1);
    ASTNode **funcs = NULL;
    int nfuncs = 0, cap = 0;

    while (peek(tl)->type != TOK_EOF) {
        if (nfuncs >= cap) {
            cap = cap ? cap * 2 : 4;
            funcs = realloc(funcs, cap * sizeof(ASTNode *));
        }
        funcs[nfuncs++] = parse_func_decl(tl);
    }

    program->program.funcs  = funcs;
    program->program.nfuncs = nfuncs;
    return program;
}

/* ── AST printer (debug) ─────────────────────────────────────────────────── */

void print_ast(ASTNode *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");

    switch (n->kind) {
        case NODE_PROGRAM:
            printf("PROGRAM (%d functions)\n", n->program.nfuncs);
            for (int i = 0; i < n->program.nfuncs; i++)
                print_ast(n->program.funcs[i], indent + 1);
            break;
        case NODE_FUNC_DECL:
            printf("FUNC %s(%d params)\n", n->func.name, n->func.nparams);
            print_ast(n->func.body, indent + 1);
            break;
        case NODE_BLOCK:
            printf("BLOCK\n");
            for (int i = 0; i < n->block.nstmts; i++)
                print_ast(n->block.stmts[i], indent + 1);
            break;
        case NODE_RETURN:
            printf("RETURN\n"); print_ast(n->ret.expr, indent + 1); break;
        case NODE_IF:
            printf("IF\n");
            print_ast(n->if_stmt.cond, indent + 1);
            print_ast(n->if_stmt.then, indent + 1);
            if (n->if_stmt.alt) print_ast(n->if_stmt.alt, indent + 1);
            break;
        case NODE_WHILE:
            printf("WHILE\n");
            print_ast(n->while_stmt.cond, indent + 1);
            print_ast(n->while_stmt.body, indent + 1);
            break;
        case NODE_FOR:
            printf("FOR\n");
            print_ast(n->for_stmt.init, indent + 1);
            print_ast(n->for_stmt.cond, indent + 1);
            print_ast(n->for_stmt.post, indent + 1);
            print_ast(n->for_stmt.body, indent + 1);
            break;
        case NODE_VAR_DECL:
            printf("VAR_DECL %s\n", n->var_decl.name);
            if (n->var_decl.init) print_ast(n->var_decl.init, indent + 1);
            break;
        case NODE_EXPR_STMT:
            printf("EXPR_STMT\n"); print_ast(n->expr_stmt.expr, indent + 1); break;
        case NODE_INT_LIT:
            printf("INT_LIT %ld\n", n->int_lit.value); break;
        case NODE_IDENT:
            printf("IDENT %s\n", n->ident.name); break;
        case NODE_ASSIGN:
            printf("ASSIGN %s\n", n->assign.name);
            print_ast(n->assign.value, indent + 1);
            break;
        case NODE_BINOP:
            printf("BINOP %s\n", token_type_name(n->binop.op));
            print_ast(n->binop.left,  indent + 1);
            print_ast(n->binop.right, indent + 1);
            break;
        case NODE_UNOP:
            printf("UNOP %s\n", token_type_name(n->unop.op));
            print_ast(n->unop.operand, indent + 1);
            break;
        case NODE_CALL:
            printf("CALL %s(%d args)\n", n->call.name, n->call.nargs);
            for (int i = 0; i < n->call.nargs; i++)
                print_ast(n->call.args[i], indent + 1);
            break;
    }
}

/* ── AST cleanup ─────────────────────────────────────────────────────────── */

void free_ast(ASTNode *n) {
    if (!n) return;
    switch (n->kind) {
        case NODE_PROGRAM:
            for (int i = 0; i < n->program.nfuncs; i++) free_ast(n->program.funcs[i]);
            free(n->program.funcs); break;
        case NODE_FUNC_DECL:
            free(n->func.name);
            for (int i = 0; i < n->func.nparams; i++) free(n->func.params[i]);
            free(n->func.params);
            free_ast(n->func.body); break;
        case NODE_BLOCK:
            for (int i = 0; i < n->block.nstmts; i++) free_ast(n->block.stmts[i]);
            free(n->block.stmts); break;
        case NODE_RETURN:   free_ast(n->ret.expr); break;
        case NODE_IF:       free_ast(n->if_stmt.cond); free_ast(n->if_stmt.then); free_ast(n->if_stmt.alt); break;
        case NODE_WHILE:    free_ast(n->while_stmt.cond); free_ast(n->while_stmt.body); break;
        case NODE_FOR:      free_ast(n->for_stmt.init); free_ast(n->for_stmt.cond); free_ast(n->for_stmt.post); free_ast(n->for_stmt.body); break;
        case NODE_VAR_DECL: free(n->var_decl.name); free_ast(n->var_decl.init); break;
        case NODE_EXPR_STMT:free_ast(n->expr_stmt.expr); break;
        case NODE_IDENT:    free(n->ident.name); break;
        case NODE_ASSIGN:   free(n->assign.name); free_ast(n->assign.value); break;
        case NODE_BINOP:    free_ast(n->binop.left); free_ast(n->binop.right); break;
        case NODE_UNOP:     free_ast(n->unop.operand); break;
        case NODE_CALL:
            free(n->call.name);
            for (int i = 0; i < n->call.nargs; i++) free_ast(n->call.args[i]);
            free(n->call.args); break;
        default: break;
    }
    free(n);
}
