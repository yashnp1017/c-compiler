/*
 * codegen.c — x86-64 AT&T syntax assembly code generator
 *
 * Strategy:
 *   - Stack-based local variable allocation (rbp-relative offsets)
 *   - System V AMD64 ABI calling convention (rdi, rsi, rdx, rcx, r8, r9)
 *   - Expressions evaluated into %rax; intermediate values pushed/popped
 *   - Unique labels via global counter (for if/while/for branches)
 *   - Short-circuit evaluation for && and ||
 *
 * Register usage:
 *   %rax — return value and primary expression result
 *   %rbx — secondary operand (callee-saved, restored after use)
 *   %rdi/%rsi/%rdx/... — function call arguments (System V ABI)
 *   %rsp — stack pointer (16-byte aligned before calls)
 *   %rbp — frame pointer (for rbp-relative local variable access)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"

/* ── Code generation context ─────────────────────────────────────────────── */

#define MAX_LOCALS 128
#define MAX_PARAMS 6

/* SysV AMD64 argument registers */
static const char *arg_regs[MAX_PARAMS] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

typedef struct {
    char *name;
    int   offset;   /* negative offset from %rbp, e.g. -8, -16, ... */
} Local;

typedef struct {
    FILE   *out;
    Local   locals[MAX_LOCALS];
    int     nlocals;
    int     stack_size;     /* current frame size (bytes, grows down) */
    int     label_counter;
} Ctx;

static int new_label(Ctx *ctx) { return ctx->label_counter++; }

static int find_local(Ctx *ctx, const char *name) {
    for (int i = 0; i < ctx->nlocals; i++)
        if (strcmp(ctx->locals[i].name, name) == 0)
            return ctx->locals[i].offset;
    fprintf(stderr, "codegen error: undefined variable '%s'\n", name);
    exit(1);
}

static int alloc_local(Ctx *ctx, const char *name) {
    if (ctx->nlocals >= MAX_LOCALS) {
        fprintf(stderr, "codegen error: too many locals\n");
        exit(1);
    }
    ctx->stack_size += 8;
    int offset = -ctx->stack_size;
    ctx->locals[ctx->nlocals].name   = strdup(name);
    ctx->locals[ctx->nlocals].offset = offset;
    ctx->nlocals++;
    return offset;
}

/* ── Emit helpers ────────────────────────────────────────────────────────── */

#define EMIT(ctx, fmt, ...) fprintf((ctx)->out, "\t" fmt "\n", ##__VA_ARGS__)
#define EMITL(ctx, lbl)     fprintf((ctx)->out, ".L%d:\n", lbl)

/* ── Expression code generation ──────────────────────────────────────────── */

static void gen_expr(Ctx *ctx, ASTNode *n);

static void gen_binop(Ctx *ctx, ASTNode *n) {
    TokenType op = n->binop.op;

    /* Short-circuit && */
    if (op == TOK_AND) {
        int false_lbl = new_label(ctx);
        int end_lbl   = new_label(ctx);
        gen_expr(ctx, n->binop.left);
        EMIT(ctx, "testq %%rax, %%rax");
        EMIT(ctx, "je .L%d", false_lbl);
        gen_expr(ctx, n->binop.right);
        EMIT(ctx, "testq %%rax, %%rax");
        EMIT(ctx, "je .L%d", false_lbl);
        EMIT(ctx, "movq $1, %%rax");
        EMIT(ctx, "jmp .L%d", end_lbl);
        EMITL(ctx, false_lbl);
        EMIT(ctx, "movq $0, %%rax");
        EMITL(ctx, end_lbl);
        return;
    }

    /* Short-circuit || */
    if (op == TOK_OR) {
        int true_lbl = new_label(ctx);
        int end_lbl  = new_label(ctx);
        gen_expr(ctx, n->binop.left);
        EMIT(ctx, "testq %%rax, %%rax");
        EMIT(ctx, "jne .L%d", true_lbl);
        gen_expr(ctx, n->binop.right);
        EMIT(ctx, "testq %%rax, %%rax");
        EMIT(ctx, "jne .L%d", true_lbl);
        EMIT(ctx, "movq $0, %%rax");
        EMIT(ctx, "jmp .L%d", end_lbl);
        EMITL(ctx, true_lbl);
        EMIT(ctx, "movq $1, %%rax");
        EMITL(ctx, end_lbl);
        return;
    }

    /* General: evaluate both sides, left in %rbx, right in %rax */
    gen_expr(ctx, n->binop.left);
    EMIT(ctx, "pushq %%rax");
    gen_expr(ctx, n->binop.right);
    EMIT(ctx, "movq %%rax, %%rbx");
    EMIT(ctx, "popq %%rax");

    switch (op) {
        case TOK_PLUS:    EMIT(ctx, "addq %%rbx, %%rax");  break;
        case TOK_MINUS:   EMIT(ctx, "subq %%rbx, %%rax");  break;
        case TOK_STAR:    EMIT(ctx, "imulq %%rbx, %%rax"); break;
        case TOK_SLASH:
            EMIT(ctx, "cqto");
            EMIT(ctx, "idivq %%rbx");
            break;
        case TOK_PERCENT:
            EMIT(ctx, "cqto");
            EMIT(ctx, "idivq %%rbx");
            EMIT(ctx, "movq %%rdx, %%rax"); /* remainder in %rdx */
            break;
        case TOK_EQ:
            EMIT(ctx, "cmpq %%rbx, %%rax");
            EMIT(ctx, "sete %%al");
            EMIT(ctx, "movzbq %%al, %%rax");
            break;
        case TOK_NEQ:
            EMIT(ctx, "cmpq %%rbx, %%rax");
            EMIT(ctx, "setne %%al");
            EMIT(ctx, "movzbq %%al, %%rax");
            break;
        case TOK_LT:
            EMIT(ctx, "cmpq %%rbx, %%rax");
            EMIT(ctx, "setl %%al");
            EMIT(ctx, "movzbq %%al, %%rax");
            break;
        case TOK_LE:
            EMIT(ctx, "cmpq %%rbx, %%rax");
            EMIT(ctx, "setle %%al");
            EMIT(ctx, "movzbq %%al, %%rax");
            break;
        case TOK_GT:
            EMIT(ctx, "cmpq %%rbx, %%rax");
            EMIT(ctx, "setg %%al");
            EMIT(ctx, "movzbq %%al, %%rax");
            break;
        case TOK_GE:
            EMIT(ctx, "cmpq %%rbx, %%rax");
            EMIT(ctx, "setge %%al");
            EMIT(ctx, "movzbq %%al, %%rax");
            break;
        default:
            fprintf(stderr, "codegen: unhandled binary op %d\n", op);
            exit(1);
    }
}

static void gen_expr(Ctx *ctx, ASTNode *n) {
    if (!n) return;

    switch (n->kind) {
        case NODE_INT_LIT:
            EMIT(ctx, "movq $%ld, %%rax", n->int_lit.value);
            break;

        case NODE_IDENT: {
            int off = find_local(ctx, n->ident.name);
            EMIT(ctx, "movq %d(%%rbp), %%rax", off);
            break;
        }

        case NODE_ASSIGN: {
            /* Evaluate compound assignment ops */
            if (n->assign.op != TOK_ASSIGN) {
                /* Load current value */
                int off = find_local(ctx, n->assign.name);
                EMIT(ctx, "movq %d(%%rbp), %%rbx", off);
                /* Evaluate RHS into %rax */
                gen_expr(ctx, n->assign.value);
                /* Apply operator */
                switch (n->assign.op) {
                    case TOK_PLUS_ASSIGN:  EMIT(ctx, "addq %%rbx, %%rax");  break;
                    case TOK_MINUS_ASSIGN: EMIT(ctx, "subq %%rax, %%rbx"); EMIT(ctx, "movq %%rbx, %%rax"); break;
                    case TOK_STAR_ASSIGN:  EMIT(ctx, "imulq %%rbx, %%rax"); break;
                    case TOK_SLASH_ASSIGN:
                        EMIT(ctx, "pushq %%rax");
                        EMIT(ctx, "movq %%rbx, %%rax");
                        EMIT(ctx, "cqto");
                        EMIT(ctx, "popq %%rbx");
                        EMIT(ctx, "idivq %%rbx");
                        break;
                    default: break;
                }
            } else {
                gen_expr(ctx, n->assign.value);
            }
            int off = find_local(ctx, n->assign.name);
            EMIT(ctx, "movq %%rax, %d(%%rbp)", off);
            break;
        }

        case NODE_BINOP:
            gen_binop(ctx, n);
            break;

        case NODE_UNOP:
            gen_expr(ctx, n->unop.operand);
            if (n->unop.op == TOK_MINUS) {
                EMIT(ctx, "negq %%rax");
            } else if (n->unop.op == TOK_BANG) {
                EMIT(ctx, "testq %%rax, %%rax");
                EMIT(ctx, "sete %%al");
                EMIT(ctx, "movzbq %%al, %%rax");
            }
            break;

        case NODE_CALL: {
            /* Push args in reverse, pass first 6 via registers (SysV ABI) */
            int nargs = n->call.nargs;
            if (nargs > MAX_PARAMS) {
                fprintf(stderr, "codegen: max %d args supported\n", MAX_PARAMS);
                exit(1);
            }
            /* Evaluate each argument and push onto stack temporarily */
            for (int i = 0; i < nargs; i++) {
                gen_expr(ctx, n->call.args[i]);
                EMIT(ctx, "pushq %%rax");
            }
            /* Load into arg registers in reverse order (last pushed = deepest) */
            for (int i = nargs - 1; i >= 0; i--) {
                EMIT(ctx, "popq %s", arg_regs[i]);
            }
            /* Save rbx across call (callee-saved; we use it for binary ops) */
            EMIT(ctx, "pushq %%rbx");
            EMIT(ctx, "callq %s", n->call.name);
            EMIT(ctx, "popq %%rbx");
            break;
        }

        default:
            fprintf(stderr, "codegen: unexpected node kind %d in expression\n", n->kind);
            exit(1);
    }
}

/* ── Statement code generation ───────────────────────────────────────────── */

static void gen_stmt(Ctx *ctx, ASTNode *n);

static void gen_block(Ctx *ctx, ASTNode *n) {
    for (int i = 0; i < n->block.nstmts; i++)
        gen_stmt(ctx, n->block.stmts[i]);
}

static void gen_stmt(Ctx *ctx, ASTNode *n) {
    if (!n) return;

    switch (n->kind) {
        case NODE_RETURN:
            gen_expr(ctx, n->ret.expr);
            /* Epilogue */
            EMIT(ctx, "movq %%rbp, %%rsp");
            EMIT(ctx, "popq %%rbp");
            EMIT(ctx, "retq");
            break;

        case NODE_EXPR_STMT:
            gen_expr(ctx, n->expr_stmt.expr);
            break;

        case NODE_VAR_DECL: {
            int off = alloc_local(ctx, n->var_decl.name);
            /* Adjust stack pointer for this new slot */
            EMIT(ctx, "subq $8, %%rsp");
            if (n->var_decl.init) {
                gen_expr(ctx, n->var_decl.init);
                EMIT(ctx, "movq %%rax, %d(%%rbp)", off);
            } else {
                EMIT(ctx, "movq $0, %d(%%rbp)", off);
            }
            break;
        }

        case NODE_IF: {
            int else_lbl = new_label(ctx);
            int end_lbl  = new_label(ctx);
            gen_expr(ctx, n->if_stmt.cond);
            EMIT(ctx, "testq %%rax, %%rax");
            EMIT(ctx, "je .L%d", else_lbl);
            gen_stmt(ctx, n->if_stmt.then);
            EMIT(ctx, "jmp .L%d", end_lbl);
            EMITL(ctx, else_lbl);
            if (n->if_stmt.alt) gen_stmt(ctx, n->if_stmt.alt);
            EMITL(ctx, end_lbl);
            break;
        }

        case NODE_WHILE: {
            int cond_lbl = new_label(ctx);
            int end_lbl  = new_label(ctx);
            EMITL(ctx, cond_lbl);
            gen_expr(ctx, n->while_stmt.cond);
            EMIT(ctx, "testq %%rax, %%rax");
            EMIT(ctx, "je .L%d", end_lbl);
            gen_stmt(ctx, n->while_stmt.body);
            EMIT(ctx, "jmp .L%d", cond_lbl);
            EMITL(ctx, end_lbl);
            break;
        }

        case NODE_FOR: {
            int cond_lbl = new_label(ctx);
            int post_lbl = new_label(ctx);
            int end_lbl  = new_label(ctx);
            if (n->for_stmt.init) gen_stmt(ctx, n->for_stmt.init);
            EMITL(ctx, cond_lbl);
            if (n->for_stmt.cond) {
                gen_expr(ctx, n->for_stmt.cond);
                EMIT(ctx, "testq %%rax, %%rax");
                EMIT(ctx, "je .L%d", end_lbl);
            }
            gen_stmt(ctx, n->for_stmt.body);
            EMITL(ctx, post_lbl);
            if (n->for_stmt.post) gen_expr(ctx, n->for_stmt.post);
            EMIT(ctx, "jmp .L%d", cond_lbl);
            EMITL(ctx, end_lbl);
            break;
        }

        case NODE_BLOCK:
            gen_block(ctx, n);
            break;

        default:
            fprintf(stderr, "codegen: unhandled statement kind %d\n", n->kind);
            exit(1);
    }
}

/* ── Function code generation ────────────────────────────────────────────── */

static void gen_func(Ctx *ctx, ASTNode *n) {
    /* Reset per-function state */
    for (int i = 0; i < ctx->nlocals; i++) free(ctx->locals[i].name);
    ctx->nlocals     = 0;
    ctx->stack_size  = 0;

    /* Function label */
    fprintf(ctx->out, "\t.globl %s\n", n->func.name);
    fprintf(ctx->out, "%s:\n", n->func.name);

    /* Prologue: save rbp, set frame pointer */
    EMIT(ctx, "pushq %%rbp");
    EMIT(ctx, "movq %%rsp, %%rbp");
    /* Save rbx (callee-saved, used for binary ops) */
    EMIT(ctx, "pushq %%rbx");

    /* Allocate space for parameters as locals */
    for (int i = 0; i < n->func.nparams && i < MAX_PARAMS; i++) {
        int off = alloc_local(ctx, n->func.params[i]);
        EMIT(ctx, "subq $8, %%rsp");
        EMIT(ctx, "movq %s, %d(%%rbp)", arg_regs[i], off);
    }

    /* Generate body */
    gen_block(ctx, n->func.body);

    /* Fallthrough epilogue (handles functions without explicit return) */
    EMIT(ctx, "movq $0, %%rax");
    EMIT(ctx, "popq %%rbx");
    EMIT(ctx, "movq %%rbp, %%rsp");
    EMIT(ctx, "popq %%rbp");
    EMIT(ctx, "retq");
    fprintf(ctx->out, "\n");
}

/* ── Top-level code generation ───────────────────────────────────────────── */

void codegen(ASTNode *ast, FILE *out) {
    Ctx ctx = { .out = out, .nlocals = 0, .stack_size = 0, .label_counter = 0 };

    fprintf(out, "\t.text\n");
    for (int i = 0; i < ast->program.nfuncs; i++)
        gen_func(&ctx, ast->program.funcs[i]);
}
