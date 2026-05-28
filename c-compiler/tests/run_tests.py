#!/usr/bin/env python3
"""
Test runner for the C compiler.

Each test case is a small C program and an expected exit code.
The compiler produces assembly, which is assembled and linked by gcc,
then run and its exit code compared to the expected value.

Usage:
    python3 tests/run_tests.py
"""

import subprocess
import sys
import os
import tempfile
from dataclasses import dataclass

COMPILER = "./cc"
TESTS_DIR = os.path.dirname(__file__)


@dataclass
class TestCase:
    name: str
    source: str
    expected: int  # expected process exit code


TEST_CASES = [
    # ── Literals and returns ──────────────────────────────────────────────
    TestCase("return_zero",      "int main() { return 0; }",                 0),
    TestCase("return_constant",  "int main() { return 42; }",                42),
    TestCase("return_negative",
             "int main() { return -3; }",                                    253),  # 256-3

    # ── Arithmetic ────────────────────────────────────────────────────────
    TestCase("addition",         "int main() { return 3 + 4; }",             7),
    TestCase("subtraction",      "int main() { return 10 - 3; }",            7),
    TestCase("multiplication",   "int main() { return 3 * 4; }",             12),
    TestCase("division",         "int main() { return 20 / 4; }",            5),
    TestCase("modulo",           "int main() { return 17 % 5; }",            2),
    TestCase("precedence",       "int main() { return 2 + 3 * 4; }",         14),
    TestCase("parens",           "int main() { return (2 + 3) * 4; }",       20),

    # ── Unary operators ───────────────────────────────────────────────────
    TestCase("negate",           "int main() { return -7 + 10; }",           3),
    TestCase("not_zero",         "int main() { return !0; }",                1),
    TestCase("not_nonzero",      "int main() { return !5; }",                0),

    # ── Comparison operators ──────────────────────────────────────────────
    TestCase("lt_true",          "int main() { return 3 < 5; }",             1),
    TestCase("lt_false",         "int main() { return 5 < 3; }",             0),
    TestCase("le_equal",         "int main() { return 4 <= 4; }",            1),
    TestCase("gt_true",          "int main() { return 7 > 2; }",             1),
    TestCase("ge_true",          "int main() { return 5 >= 5; }",            1),
    TestCase("eq_true",          "int main() { return 7 == 7; }",            1),
    TestCase("eq_false",         "int main() { return 7 == 8; }",            0),
    TestCase("neq_true",         "int main() { return 7 != 8; }",            1),

    # ── Logical operators ─────────────────────────────────────────────────
    TestCase("and_true",         "int main() { return 1 && 1; }",            1),
    TestCase("and_false",        "int main() { return 1 && 0; }",            0),
    TestCase("or_true",          "int main() { return 0 || 1; }",            1),
    TestCase("or_false",         "int main() { return 0 || 0; }",            0),

    # ── Variables ─────────────────────────────────────────────────────────
    TestCase("var_decl",         "int main() { int x = 5; return x; }",      5),
    TestCase("var_assign",       "int main() { int x = 0; x = 7; return x; }",  7),
    TestCase("var_arith",        "int main() { int x = 3; int y = 4; return x + y; }", 7),
    TestCase("compound_add",     "int main() { int x = 10; x += 5; return x; }", 15),
    TestCase("compound_sub",     "int main() { int x = 10; x -= 3; return x; }", 7),
    TestCase("compound_mul",     "int main() { int x = 4; x *= 3; return x; }", 12),

    # ── If / else ─────────────────────────────────────────────────────────
    TestCase("if_true",
             "int main() { int x = 1; if (x) { return 1; } return 0; }",    1),
    TestCase("if_false",
             "int main() { int x = 0; if (x) { return 1; } return 0; }",    0),
    TestCase("if_else_true",
             "int main() { if (1) { return 2; } else { return 3; } }",      2),
    TestCase("if_else_false",
             "int main() { if (0) { return 2; } else { return 3; } }",      3),
    TestCase("nested_if",
             "int main() { int x = 5; if (x > 3) { if (x > 4) { return 1; } return 2; } return 0; }", 1),

    # ── While loops ───────────────────────────────────────────────────────
    TestCase("while_basic",
             "int main() { int i = 0; int s = 0; while (i < 5) { s += i; i += 1; } return s; }", 10),
    TestCase("while_zero_iters",
             "int main() { while (0) { return 99; } return 0; }",           0),

    # ── For loops ─────────────────────────────────────────────────────────
    TestCase("for_basic",
             "int main() { int s = 0; for (int i = 0; i < 5; i = i + 1) { s = s + i; } return s; }", 10),
    TestCase("for_count",
             "int main() { int n = 0; for (int i = 1; i <= 10; i = i + 1) { n = n + 1; } return n; }", 10),

    # ── Functions ─────────────────────────────────────────────────────────
    TestCase("func_call",
             "int add(int a, int b) { return a + b; } int main() { return add(3, 4); }", 7),
    TestCase("func_recursive",
             "int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); } int main() { return fact(5); }", 120),
    TestCase("func_multi_param",
             "int mul3(int a, int b, int c) { return a * b * c; } int main() { return mul3(2, 3, 4); }", 24),

    # ── Complex programs ──────────────────────────────────────────────────
    TestCase("fibonacci",
             """
int fib(int n) {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}
int main() { return fib(10); }
""",          55),

    TestCase("gcd",
             """
int gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a - (a / b) * b;
        a = t;
    }
    return a;
}
int main() { return gcd(48, 18); }
""",          6),
]


def run_test(tc: TestCase) -> tuple[bool, str]:
    with tempfile.TemporaryDirectory() as tmp:
        src_file = os.path.join(tmp, "test.c")
        asm_file = os.path.join(tmp, "test.s")
        bin_file = os.path.join(tmp, "test")

        with open(src_file, "w") as f:
            f.write(tc.source)

        # Compile: C → assembly
        r = subprocess.run([COMPILER, src_file, "-o", asm_file],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return False, f"compiler error: {r.stderr.strip()}"

        # Assemble + link: assembly → binary
        r = subprocess.run(["gcc", asm_file, "-o", bin_file],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return False, f"assembler error: {r.stderr.strip()}"

        # Run and capture exit code
        r = subprocess.run([bin_file], capture_output=True)
        actual = r.returncode

        if actual == tc.expected:
            return True, ""
        else:
            return False, f"expected exit code {tc.expected}, got {actual}"


def main():
    passed = 0
    failed = 0
    errors = []

    print(f"Running {len(TEST_CASES)} tests...\n")

    for tc in TEST_CASES:
        ok, msg = run_test(tc)
        if ok:
            print(f"  ✓  {tc.name}")
            passed += 1
        else:
            print(f"  ✗  {tc.name}: {msg}")
            errors.append(tc.name)
            failed += 1

    print(f"\n{'─' * 40}")
    print(f"Results: {passed} passed, {failed} failed out of {len(TEST_CASES)}")

    if failed:
        print("\nFailed tests:")
        for name in errors:
            print(f"  - {name}")
        sys.exit(1)
    else:
        print("\nAll tests passed! ✓")
        sys.exit(0)


if __name__ == "__main__":
    main()
