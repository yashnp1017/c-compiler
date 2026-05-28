# Simple C Compiler

A complete C compiler built from scratch in C — no libraries, no frameworks. Translates a subset of C source code into working x86-64 assembly through a classic four-stage pipeline: lexer → parser → AST → code generator.

**All 45 tests pass**, including recursion (factorial, Fibonacci), loops, conditionals, and multi-function programs.

---

## Compiler Pipeline

```
Source Code (.c)
      │
      ▼
┌─────────────────────────────────────────────────────┐
│  Stage 1: Lexer (src/lexer.c)                       │
│  Single-pass scanner → token stream                 │
│  Handles: keywords, identifiers, integer literals,  │
│  all operators (including multi-char: <=, ==, +=),  │
│  comments (//, /* */), line/col tracking            │
└─────────────────────┬───────────────────────────────┘
                      │ TokenList
                      ▼
┌─────────────────────────────────────────────────────┐
│  Stage 2: Parser (src/parser.c)                     │
│  Recursive descent + Pratt precedence climbing      │
│  → Abstract Syntax Tree (AST)                       │
│  Grammar: functions, blocks, if/else, while, for,   │
│  variable declarations, all expression forms        │
└─────────────────────┬───────────────────────────────┘
                      │ ASTNode*
                      ▼
┌─────────────────────────────────────────────────────┐
│  Stage 3: Code Generator (src/codegen.c)            │
│  Tree-walk emitter → x86-64 AT&T assembly           │
│  Stack-based locals, rbp-relative addressing,       │
│  System V AMD64 ABI calling convention,             │
│  short-circuit && and ||, label counter for branches│
└─────────────────────┬───────────────────────────────┘
                      │ .s file
                      ▼
              x86-64 Assembly
           (assembled by gcc -c)
                      │
                      ▼
               Executable Binary
```

---

## Language Features Supported

| Feature | Example |
|---|---|
| Integer literals | `return 42;` |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| Comparison | `<`, `<=`, `>`, `>=`, `==`, `!=` |
| Logical | `&&`, `\|\|`, `!` (with short-circuit) |
| Assignment | `=`, `+=`, `-=`, `*=`, `/=` |
| Variables | `int x = 5;` |
| If / else | `if (x > 0) { ... } else { ... }` |
| While loops | `while (i < n) { ... }` |
| For loops | `for (int i = 0; i < n; i = i + 1) { ... }` |
| Functions | `int add(int a, int b) { return a + b; }` |
| Recursion | `int fact(int n) { return n * fact(n-1); }` |
| Comments | `// line comment`, `/* block */` |

---

## Design Decisions

### Lexer
- **Single-pass scanning** — O(n) in source length, no backtracking
- **Multi-char operator handling** — `PEEK1` macro checks the character after `i++` (critical: defined *after* the `i++` increment so it correctly references the next character)
- **Keyword table** — linear scan over a small static array; negligible cost for the keyword count

### Parser
- **Pratt-style precedence climbing** using a macro `PARSE_BINOP` that generates left-associative binary operator parsers at each precedence level — avoids recursive descent for each level while keeping code readable
- **Two-token lookahead for assignment** — indexes `tl->tokens[pos]` and `tl->tokens[pos+1]` directly rather than consuming and backtracking, which prevents multi-char operators (e.g. `<=`) from being misidentified as `<` followed by `=`
- **AST allocated on heap** — each node is `calloc`'d; entire tree is freed after code generation

### Code Generator
- **Stack-based frame layout** — each local variable gets an 8-byte slot at `rbp - (n * 8)`, allocated incrementally as declarations are encountered
- **`%rbx` as scratch register** — callee-saved per SysV ABI, saved in function prologue and restored before every `callq` via push/pop, enabling binary operators to use it as the second operand without corrupting recursive call chains
- **Short-circuit evaluation** — `&&` and `||` use conditional jumps at the IR level rather than evaluating both sides, matching C semantics
- **No `andq $-16, %rsp`** — stack alignment for SSE is not needed for integer-only calls, and the instruction is non-reversible when there are pending values on the stack

---

## Quickstart

### Prerequisites
- GCC (for compiling the compiler and assembling the output)
- Python 3 (for running the test suite)
- Linux x86-64 (the emitted assembly targets the SysV AMD64 ABI)

### Build
```bash
git clone https://github.com/yashnp1017/c-compiler
cd c-compiler
make
```

### Usage
```bash
# Compile a C file to assembly
./cc examples/factorial.c -o factorial.s

# Assemble and link with gcc
gcc factorial.s -o factorial

# Run
./factorial; echo $?   # prints exit code = program return value
```

### Run tests
```bash
make test
```

Expected output:
```
Running 45 tests...
  ✓  return_zero
  ✓  return_constant
  ... (all 45)
Results: 45 passed, 0 failed out of 45
All tests passed! ✓
```

---

## Project Structure

```
c-compiler/
├── src/
│   ├── main.c       # Entry point: read file, lex → parse → codegen
│   ├── lexer.c      # Single-pass scanner, token stream
│   ├── parser.c     # Recursive descent parser, AST construction
│   └── codegen.c    # x86-64 AT&T assembly emitter
├── include/
│   ├── lexer.h      # Token types, TokenList API
│   ├── parser.h     # AST node kinds, ASTNode struct
│   └── codegen.h    # codegen() declaration
├── tests/
│   └── run_tests.py # 45-test Python test runner
├── Makefile
└── README.md
```

---

## Test Coverage

45 tests covering:
- Return statements and integer literals
- All arithmetic operators and operator precedence
- Unary operators (`-`, `!`)
- All comparison and logical operators
- Variable declaration, assignment, and compound assignment
- If/else (including nested)
- While loops (including zero iterations)
- For loops
- Function calls (single and multiple parameters)
- **Recursive functions** (factorial, Fibonacci)
- **Complex programs** (GCD via Euclidean algorithm)

---

## Known Limitations

- Integers only (`int`) — no `float`, `char`, pointers, or arrays
- No `printf` / standard library calls (output via exit code)
- No string literals
- Max 6 function parameters (SysV ABI register limit)
- Max 128 local variables per function
- No `break` / `continue` in loops

---

## Author

**Yash Patel** — Purdue University, B.S. Computer Science & AI (May 2027)

[LinkedIn](https://linkedin.com/in/yash-patel-018291278) · [GitHub](https://github.com/yashnp1017)
