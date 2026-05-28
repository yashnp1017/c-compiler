/*
 * codegen.h — x86-64 AT&T syntax assembly code generator
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include "parser.h"

void codegen(ASTNode *ast, FILE *out);

#endif /* CODEGEN_H */
