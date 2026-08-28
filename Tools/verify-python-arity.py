"""Argument-count check for Python calls into the published Cy* surface.

WHY THIS EXISTS: when a read or write is re-homed onto the object it describes, its
signature often changes with it -- the address arguments go away, and a legacy parameter
may go with them. `CyUnit::isInvisible` is the worked case: the legacy binding took
(team, bDebug), the re-homed read takes (team) alone, and the Python call site still passed
two. Nothing catches that:

  * the C++ compiles -- the call site is in Python;
  * `verify-python-bindings` only asks whether the NAME is registered, never with how many
    arguments it is called;
  * `verify-python-syntax` sees a well formed call.

So it survives every check and fails at runtime, inside one screen, as
`ArgumentError: Python argument types in CyUnit.isInvisible(CyUnit, int, bool) did not
match C++ signature`. That is a broken advisor for the player and a silent one for us.

HOW IT DECIDES. A name published on exactly ONE Cy class is unambiguous: any Python call of
that name must supply between (required) and (total) arguments, where `required` discounts
C++ default arguments. A name published on SEVERAL classes is skipped -- the receiver's type
is not knowable from the text, and guessing produces noise rather than findings.

⚖ ADVISORY, and it must stay that way until receiver types can be inferred. Python is untyped and
this repo names ordinary locals `CyPlayer`, `tab`, `scores`, `city` -- so a call like
`tab.setStatus(x)` (a BUG screen tab) is indistinguishable from `CyUnit.setStatus`. The report is a
TRIAGE LIST, not a defect count: judge each entry by what the receiver actually is.

⛔ When an entry IS real, fix the CALL SITE to the published signature. Never widen the C++ signature
to accept the legacy shape merely to silence it -- that re-creates the per-field contract the re-home
removed. ⚖ Widening is right only when a consumer genuinely NEEDS the parameter: `CyCity::pushOrder`
carries bAppend because the scenario copier replays a whole build queue, and a fixed replace would
keep only the last order.

Run from the repo root:  python Tools/verify-python-arity.py
"""

import os
import re
import sys
import collections

SOURCE_DIRS = ["Sources/Python", "Sources/Infrastructure"]
PYTHON_ROOTS = ["Assets/Python"]

DEF_LINE = re.compile(r'\.def\(\s*"([^"]+)"')
CLASS_LINE = re.compile(r"class_<\s*(Cy\w+)")
# a declaration inside a Cy header: <return> name(args) [const];
DECL = re.compile(r"^\s+[A-Za-z_][\w:<>,\s\*&]*?\b([A-Za-z_]\w*)\s*\(([^;{]*)\)\s*(?:const)?\s*;", re.M)


def published_names():
    """name -> set of classes that publish it."""
    owners = collections.defaultdict(set)
    for directory in SOURCE_DIRS:
        for name in sorted(os.listdir(directory)):
            if not name.endswith(".cpp"):
                continue
            current = None
            path = os.path.join(directory, name)
            with open(path, encoding="utf-8", errors="replace") as handle:
                for line in handle:
                    found = CLASS_LINE.search(line)
                    if found:
                        current = found.group(1)
                    defined = DEF_LINE.search(line)
                    if defined and current:
                        owners[defined.group(1)].add(current)
    return owners


def declared_arity(class_name):
    """name -> (required, total) from the class header, or None when it cannot be read."""
    path = os.path.join("Sources/Python", class_name + ".h")
    if not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    arity = {}
    for match in DECL.finditer(text):
        name = match.group(1)
        raw = match.group(2).strip()
        if name in ("if", "for", "while", "switch", "return", class_name):
            continue
        if not raw:
            arity[name] = (0, 0)
            continue
        depth = 0
        parts = []
        current = ""
        for char in raw:
            if char in "<([":
                depth += 1
            elif char in ">)]":
                depth -= 1
            if char == "," and depth == 0:
                parts.append(current)
                current = ""
            else:
                current += char
        parts.append(current)
        total = len(parts)
        required = len([p for p in parts if "=" not in p])
        arity[name] = (required, total)
    return arity


def count_call_args(text, open_index):
    depth = 0
    index = open_index
    args = 0
    seen = False
    while index < len(text):
        char = text[index]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if depth == 0:
                return (args + 1 if seen else 0), index
        elif char == "," and depth == 1:
            args += 1
        elif not char.isspace() and depth == 1:
            seen = True
        index += 1
    return None, None


def main():
    owners = published_names()
    # A name DECLARED in more than one Cy header is ambiguous: the receiver's type is not
    # knowable from the text, so judging its arity would be guesswork. Skip those outright --
    # this is what keeps `getButton` / `getDescription` (on every info header) out of the report.
    declared_in = collections.defaultdict(set)
    for header in sorted(os.listdir("Sources/Python")):
        if not header.endswith(".h"):
            continue
        class_name = header[:-2]
        for method in declared_arity(class_name):
            declared_in[method].add(class_name)
    # CyInfo is the PREFIX-ADDRESSED plane: its getDescription(prefix, id) shares a name with the
    # zero-argument getDescription() on every info HANDLE (GC.getPromotionInfo(i) and its kin), and the
    # receiver cannot be told apart from the text. Judging it produces only noise.
    AMBIGUOUS_PLANES = {"CyInfo"}
    unique = {}
    for name, classes in owners.items():
        if list(classes)[0] in AMBIGUOUS_PLANES:
            continue
        if len(classes) != 1:
            continue
        if len(declared_in.get(name, set())) > 1:
            continue
        unique[name] = list(classes)[0]
    arities = {}
    for class_name in set(unique.values()):
        arities[class_name] = declared_arity(class_name)

    failures = 0
    scanned = 0
    for root in PYTHON_ROOTS:
        for directory, _, names in os.walk(root):
            if "__pycache__" in directory:
                continue
            for name in sorted(names):
                if not name.endswith(".py"):
                    continue
                path = os.path.join(directory, name)
                scanned += 1
                with open(path, encoding="utf-8", errors="replace") as handle:
                    lines = handle.read().split("\n")
                for number, line in enumerate(lines, 1):
                    if line.lstrip().startswith("#"):
                        continue
                    for call in re.finditer(r"\.([A-Za-z_]\w*)\s*\(", line):
                        method = call.group(1)
                        if method not in unique:
                            continue
                        class_name = unique[method]
                        bounds = arities.get(class_name, {}).get(method)
                        if bounds is None:
                            continue
                        required, total = bounds
                        given, _ = count_call_args(line, call.end() - 1)
                        if given is None:      # call spans lines -- not judged
                            continue
                        if given < required or given > total:
                            failures += 1
                            expect = str(total) if required == total else "%d-%d" % (required, total)
                            print("%s:%d  %s.%s takes %s argument(s), called with %d"
                                  % (path.replace("\\", "/"), number, class_name, method, expect, given))
                            print("    %s" % line.strip()[:150])

    print("scanned %d Python files against the published Cy* surface" % scanned)
    if failures:
        print("%d call(s) disagree with the published signature -- ADVISORY, triage each by what the"
              " receiver actually is (an untyped local may merely share a method name)." % failures)
        return 0
    print("arity clean -- every unambiguous Cy* call matches its published signature.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
