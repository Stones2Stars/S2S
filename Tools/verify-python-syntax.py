"""Structural syntax check for the Python 2.4 game scripts.

WHY THIS EXISTS, and why verify-python24.py does not cover it: that tool scans for
post-2.4 SYNTAX (conditional expressions, `with`, `except X as e`). It is a regex pass, so
a file can be structurally BROKEN and still report clean -- which is exactly what happened
when a whole-file substitution stripped every `()` from the tree: 185 `def` statements lost
their parameter list, `verify-python24` said "clean", and the game failed to initialize
Python at startup with a SyntaxError far from the edit.

The embedded interpreter is Python 2.4, so this cannot use `compile()` from a Python 3
host (print statements alone would fail). It checks STRUCTURE instead, which is what a bulk
text edit actually breaks:

  1. `def NAME` must be followed by a parameter list.
  2. `class NAME` must be followed by a base list or `:`.
  3. Brackets must balance over each logical line, and over the file.

Run from the repo root:  python Tools/verify-python-syntax.py
Exit status is non-zero when anything fails, so it is usable as a gate.
"""

import os
import re
import sys

ROOTS = ["Assets/Python", "Assets/PrivateMaps", "Assets/PublicMaps"]

DEF_HEAD = re.compile(r"^\s*def\s+([A-Za-z_]\w*)(.*)$")
CLASS_HEAD = re.compile(r"^\s*class\s+([A-Za-z_]\w*)(.*)$")
OPEN = "([{"
CLOSE = ")]}"
PAIR = {")": "(", "]": "[", "}": "{"}


def strip_strings_and_comments(line, in_triple, triple_kind):
    """Blank out string literals and comments so bracket counting sees only code."""
    out = []
    index = 0
    length = len(line)
    while index < length:
        char = line[index]
        escaped = index > 0 and line[index - 1] == "\\"
        if in_triple:
            if line.startswith(triple_kind, index) and not escaped:
                in_triple = False
                index += 3
                continue
            index += 1
            continue
        if (line.startswith('"""', index) or line.startswith("'''", index)) and not escaped:
            triple_kind = line[index:index + 3]
            in_triple = True
            index += 3
            continue
        if char == "#":
            break
        if char in "\"'":
            quote = char
            index += 1
            while index < length:
                if line[index] == "\\":
                    index += 2
                    continue
                if line[index] == quote:
                    index += 1
                    break
                index += 1
            continue
        out.append(char)
        index += 1
    return "".join(out), in_triple, triple_kind


def check_file(path):
    problems = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        raw_lines = handle.read().split("\n")

    in_triple = False
    triple_kind = "'''"
    depth = 0
    stack = []
    logical_start = 0

    for number, raw in enumerate(raw_lines, 1):
        code, in_triple_next, triple_kind = strip_strings_and_comments(raw, in_triple, triple_kind)
        was_in_triple = in_triple
        in_triple = in_triple_next

        if not was_in_triple and depth == 0:
            head = DEF_HEAD.match(raw)
            if head and not head.group(2).lstrip().startswith("("):
                problems.append((number, "def without a parameter list", raw.strip()))
            head = CLASS_HEAD.match(raw)
            if head:
                rest = head.group(2).lstrip()
                if not (rest.startswith("(") or rest.startswith(":")):
                    problems.append((number, "class without a base list or ':'", raw.strip()))

        if depth == 0:
            logical_start = number
        for char in code:
            if char in OPEN:
                stack.append((char, number))
                depth += 1
            elif char in CLOSE:
                if not stack:
                    problems.append((number, "closing '%s' with nothing open" % char, raw.strip()))
                    continue
                opener, _ = stack.pop()
                depth -= 1
                if opener != PAIR[char]:
                    problems.append((number, "'%s' closed by '%s'" % (opener, char), raw.strip()))

    if stack:
        opener, line_number = stack[0]
        problems.append((line_number, "'%s' never closed (opened here)" % opener, raw_lines[line_number - 1].strip()))
    if in_triple:
        problems.append((logical_start, "unterminated triple-quoted string", ""))
    return problems


def main():
    scanned = 0
    failures = 0
    for root in ROOTS:
        if not os.path.isdir(root):
            continue
        for directory, _, names in os.walk(root):
            if "__pycache__" in directory:
                continue
            for name in sorted(names):
                if not name.endswith(".py"):
                    continue
                path = os.path.join(directory, name)
                scanned += 1
                for number, why, text in check_file(path):
                    failures += 1
                    print("%s:%d  %s" % (path.replace("\\", "/"), number, why))
                    if text:
                        print("    %s" % text[:150])

    print("scanned %d Python files under %s" % (scanned, ", ".join(ROOTS)))
    if failures:
        print("FAILED -- %d structural problem(s)" % failures)
        return 1
    print("structure clean -- every def/class is well formed and every bracket balances.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
