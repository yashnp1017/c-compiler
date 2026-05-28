/*
 * cc — A Simple C Compiler
 *
 * Entry point. Orchestrates the full compilation pipeline:
 *   Source → Lexer → Parser → AST → Code Generator → x86-64 Assembly
 *
 * Usage:
 *   ./cc input.c -o output.s
 *   gcc output.s -o output && ./output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

/* Read entire file into a heap-allocated string. Caller must free. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open file '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    if (!buf) { fprintf(stderr, "error: out of memory\n"); exit(1); }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    /* Parse arguments: cc <input.c> [-o <output.s>] */
    const char *input_path  = NULL;
    const char *output_path = "out.s";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else {
            input_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "usage: cc <input.c> [-o <output.s>]\n");
        return 1;
    }

    /* 1. Read source */
    char *source = read_file(input_path);

    /* 2. Lex → token stream */
    TokenList *tokens = lex(source);

    /* 3. Parse → AST */
    ASTNode *ast = parse(tokens);

    /* 4. Generate x86-64 assembly */
    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "error: cannot open output '%s'\n", output_path);
        return 1;
    }
    codegen(ast, out);
    fclose(out);

    fprintf(stderr, "compiled '%s' → '%s'\n", input_path, output_path);

    /* Cleanup */
    free_tokens(tokens);
    free_ast(ast);
    free(source);
    return 0;
}
