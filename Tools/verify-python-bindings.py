#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify-python-bindings -- a Cy method Python CALLS must actually be REGISTERED.

The `Cy*` classes are the engine's API endpoint for Python. A method is reachable from
Python only if some `class_<CyX>` block `.def("name", ...)`s it -- being a public C++
member of CyX is NOT enough. Remove the `.def` and the C++ method still compiles, still
looks alive in the header, and every Python caller starts raising AttributeError.

That is worth a check rather than a rule for the reasons this repo keeps choosing one:

  - the COMPILER CANNOT SEE IT. The C++ side is well-formed with or without the `.def`;
    the break lives between a registration list and a `.py` file the compiler never reads.
  - it is SILENT until that screen or callback runs, and it fails as a Python traceback
    naming the CALLER, so it reads as "the advisor is broken", never as "a binding is
    missing".
  - a binding sweep is exactly the operation that produces it in bulk. One commit here
    cut 494 dangling Cy bindings; nothing checked what still called them.
  - and it is trivially mechanical to detect, which is the whole test for a checker.

⛔ REGISTRATION LIVES IN TWO PLACES AND SCANNING ONLY ONE UNDER-REPORTS BADLY. Some
classes register inline in `Sources/Python/CyX.cpp` as `class_<CyX>("CyX").def(...)`;
others register through loader functions in `Sources/Infrastructure/CvPython*Loader.cpp`
that take a `boost::python::class_<CyX>& inst` and call `inst.def(...)`. A census that
reads only the first style reports most of `CyPlayer` and `CyPlot` as unregistered, and
a census that reads only the second misses `CyUnit` entirely. This tool takes the UNION
of every `.def("name")` under `Sources/`, which is why it does not attribute a name to a
class: attribution is not needed for the verdict, and every attempt at it was the thing
that produced false positives.

Two findings, deliberately separated because only one is provable:

  DEAD BINDING (fails the run) -- the method IS defined on a `Cy*` class in C++, is
  registered NOWHERE, and Python calls that name. There is no reading of that state in
  which the call works.

  UNRESOLVED CALL (reported, never fails) -- Python calls a name that is registered
  nowhere AND defined on no `Cy*` class AND is not a Python `def` anywhere in
  `Assets/Python`. Usually a method removed outright, or one that never existed; but the
  receiver's type cannot be known from Python source, so a human decides. ⚖ Advisory for
  the same reason `verify-registry-scans` is: the mechanical part is certain, the
  CONTEXT is not.

Worked case: `CyUnit` registers exactly 8 methods, while `CvMilitaryAdvisor.py` calls
`baseCombatStr()` and `airBaseCombatStr()` on the normal unit-row render path -- both
defined in `CyUnit.cpp`, neither registered. Separately `getCommerceRateTimes100` is
called by two `canTrigger` predicates and exists nowhere in the tree at all, so those
random events can never fire.

⛔ If it fires, fix the SIDE THAT IS WRONG. A binding Python legitimately needs is
RE-REGISTERED; a call into a mechanic that is gone has its PYTHON deleted. Never widen
the tool, and never register a method just to silence it -- an endpoint is a live
consumer, and re-exposing a legacy read keeps that member alive past the compiler census.

Run from the repo root:  python Tools/verify-python-bindings.py
                         python Tools/verify-python-bindings.py --list
"""

import os
import re
import sys
import glob

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCES = os.path.join(REPO, "Sources")
PYTHON_ROOT = os.path.join(REPO, "Assets", "Python")

# `.def("name"` in any registration style -- inline class_<> chains and loader `inst.def(...)` alike.
RE_REGISTERED = re.compile(r'\.def\(\s*"([A-Za-z_][A-Za-z0-9_]*)"')

# A method DEFINED on a Cy class: `<return type> CyFoo::bar(`. The return type is skipped
# rather than enumerated -- the `CyFoo::` qualifier is what identifies it.
RE_CY_DEFINITION = re.compile(r'\b(Cy[A-Za-z0-9_]+)::([A-Za-z_][A-Za-z0-9_]*)\s*\(')

# A Python attribute call, capturing the RECEIVER token as well as the method.
#
# ⛔ THE RECEIVER IS WHAT MAKES THIS PRECISE, and dropping it is what makes the tool
# useless. A bare `.name(` scan cannot tell `CyUnit.baseCombatStr()` from
# `self.validAttrs.add(name)` -- and `add` happens to be BOTH a `CyArgsList` member and
# Python's own `set.add`, so a receiver-blind census reports 40 phantom call sites for it
# and buries the real findings under ~200 like it. A method name alone is not evidence.
RE_PY_CALL = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(')

# A Python-defined function/method of that name -- excluded, since the call may be its own.
RE_PY_DEF = re.compile(r'^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(', re.M)

# A receiver token that denotes a Cy handle: either spelled `Cy*`, or an identifier ending
# in one of the engine nouns the Python code names these objects after (`pLoopUnit`,
# `pCity`, `m_pPlot`, `eTeam`, ...). Deliberately generous on the PREFIX and strict on the
# NOUN -- the prefix conventions vary per file, the noun does not.
RE_CY_RECEIVER = re.compile(
    r'^(?:Cy[A-Za-z0-9_]*'
    r'|[A-Za-z_][A-Za-z0-9_]*(?:Unit|City|Plot|Player|Team|Group|Area|Deal|Map|Selection))$')


def read(path):
    try:
        f = open(path, "rb")
        try:
            return f.read().decode("utf-8", "ignore")
        finally:
            f.close()
    except IOError:
        return ""


def walk(root, suffix):
    out = []
    for base, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in (".vs", ".vscode", "nbproject", "__pycache__")]
        for name in files:
            if name.endswith(suffix):
                out.append(os.path.join(base, name))
    return out


def main():
    want_list = "--list" in sys.argv

    registered = set()
    cy_defined = {}          # method name -> set of Cy classes defining it
    for path in walk(SOURCES, ".cpp") + walk(SOURCES, ".h"):
        text = read(path)
        registered.update(RE_REGISTERED.findall(text))
        for cls, method in RE_CY_DEFINITION.findall(text):
            cy_defined.setdefault(method, set()).add(cls)

    py_calls = {}            # method name -> [ "file:line", ... ]  (Cy-receiver calls ONLY)
    py_defs = set()
    for path in walk(PYTHON_ROOT, ".py"):
        text = read(path)
        py_defs.update(RE_PY_DEF.findall(text))
        rel = os.path.relpath(path, REPO).replace("\\", "/")
        for lineno, line in enumerate(text.splitlines(), 1):
            if line.lstrip().startswith("#"):
                continue
            for receiver, method in RE_PY_CALL.findall(line):
                if not RE_CY_RECEIVER.match(receiver):
                    continue
                py_calls.setdefault(method, []).append("%s:%d" % (rel, lineno))

    dead = []
    for method in sorted(py_calls):
        if method in registered or method not in cy_defined:
            continue
        dead.append((method, sorted(cy_defined[method]), py_calls[method]))

    unresolved = []
    for method in sorted(py_calls):
        if method in registered or method in cy_defined or method in py_defs:
            continue
        unresolved.append((method, py_calls[method]))

    print("registered Cy methods: %d   |   Cy methods defined in C++: %d   |   distinct Python calls: %d"
          % (len(registered), len(cy_defined), len(py_calls)))
    print("")

    if dead:
        print("DEAD BINDINGS -- defined on a Cy class, registered nowhere, called from Python:")
        for method, classes, sites in dead:
            print("  %s   (%s)  -- %d call site(s)" % (method, ", ".join(classes), len(sites)))
            shown = sites if want_list else sites[:3]
            for site in shown:
                print("      %s" % site)
            if not want_list and len(sites) > 3:
                print("      ... and %d more (--list for all)" % (len(sites) - 3))
        print("")
    else:
        print("DEAD BINDINGS: none")
        print("")

    if unresolved:
        print("UNRESOLVED CALLS (ADVISORY -- a human decides; never fails the run):")
        print("  Python calls these on a Cy-looking receiver; they are registered nowhere,")
        print("  defined on no Cy class, and are not a Python def. Likely a removed mechanic.")
        for method, sites in unresolved:
            shown = sites if want_list else sites[:2]
            print("  %s  -- %d call site(s): %s" % (method, len(sites), ", ".join(shown)))
        print("")

    if dead:
        print("FAILED: %d dead binding(s). Fix the side that is WRONG -- re-register a binding"
              % len(dead))
        print("Python legitimately needs, or delete the Python for a mechanic that is gone.")
        return 1

    print("OK: every Cy method Python calls is registered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
