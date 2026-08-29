#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify-python-bindings -- the PYTHON-SIDE MIGRATION BURNDOWN, counted.

⛔ THIS IS NOT A BUG COUNT, AND IT IS NOT A LIST OF THINGS TO RE-REGISTER. The `Cy*`
bindings ARE the API surface for Python -- a type publishes the GET / PUT / POST it is
required to (patterns.md, THE IDENTITY SET IS THE FLOOR, NOT THE CEILING). The legacy
per-field contract is what died, not the surface.

The tree shows the re-home well under way: CyPlayer publishes 332 endpoints, CyCity 157
(the coherent group reads -- getYields, getCommerces, getWellbeing, getScalars,
getDefenseKinds ... plus its mutators), CyTeam 116, CyPlot 106. A THIN wrapper is
therefore an UN-RE-HOMED TYPE, never a finished one.

What never existed is a way to COUNT WHAT IS LEFT. That is this tool. It reports the
methods still DECLARED on a `Cy*` wrapper, registered nowhere, and still CALLED from
Python -- i.e. Python consumers not yet re-pointed onto the coherent reads.

⚑ THE COUNT IS CONCENTRATED, WHICH IS THE USEFUL PART. It splits into two different jobs:
a type that HAS its group reads and still carries the legacy names beside them (CyCity:
157 published, 110 legacy -- "the collision is the work"; e.g. getWellbeing is published
while happyLevel / unhappyLevel / angryPopulation are still called from 13 sites), and a
type not re-homed at all, whose GET surface has to be built first (CyUnit: 8 published,
58 legacy).

⚖ ADVISORY, AND A RATCHET -- exactly like verify-registry-scans. The count may only FALL;
a rise means a legacy declaration was revived or a legacy call was added. It does not fail
the run, because a permanently-red gate on a known in-progress migration is a gate nobody
can act on.

That is worth a check rather than a rule for the reasons this repo keeps choosing one:

  - the COMPILER CANNOT SEE IT. The C++ side is well-formed with or without the `.def`;
    the debt lives between a registration list and a `.py` file the compiler never reads.
  - it is SILENT until that screen or callback runs, and it surfaces as a Python traceback
    naming the CALLER, so it reads as "the advisor is broken", never as "this consumer was
    never migrated".
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

Two findings, deliberately separated:

  UNMIGRATED CONSUMER -- the method IS declared on a `Cy*` class in C++, is registered
  NOWHERE, and Python calls that name. The call raises today; the DECLARATION is legacy
  contract still written down; the CALL is a consumer not yet re-pointed.

  UNRESOLVED CALL -- Python calls a name registered nowhere, declared on no `Cy*` class,
  and not a Python `def` anywhere in `Assets/Python`. Usually a mechanic removed outright,
  or one that never existed at all. The receiver's type cannot be known from Python
  source, so a human decides.

⛔ HOW TO CLOSE ONE, AND THE ONE MOVE THAT IS BANNED. The declaration is KILL-ON-SIGHT
(patterns.md): an unpublished legacy method on a wrapper is not harmless dead weight -- it
is the per-field contract still written down, so the next agent reads it as the surface
and "JUST PUBLISH WHAT IS ALREADY DECLARED" looks like the cheap fix at exactly the moment
the new surface arrives. ⛔ So RE-REGISTERING IT IS THE ANTI-PATTERN, never the fix. The
declaration and its body go; the Python re-points onto the new surface, homed on the type
it addresses -- `CyCity::getPopulation()`, never `getCityPopulation(owner, id)`, because
"the moment you have getAnotherObjectSomething, we have failed."

⚠ NOT killed alongside them: the `class_<>` REGISTRATION (a zero-`.def` registration is
the TYPE IDENTITY the kept engine->Python direction needs -- the marshaller throws without
a registered converter), the identity set, and anything the ENGINE calls on the wrapper.

⚖ And the counterweight, without which killing under-serves: the surface is designed from
DEMAND and freely given where a consumer genuinely needs a read -- never derived from the
legacy list. Killing without serving pushes the next consumer back onto legacy; serving by
preserving the legacy set re-creates the per-field contract.

Worked case: `CvMilitaryAdvisor.py:329/331` calls `baseCombatStr()` / `airBaseCombatStr()`
on the normal unit-row render path -- declared in `CyUnit.cpp`, registered nowhere, so the
advisor raises. Separately `getCommerceRateTimes100` is called by two `canTrigger`
predicates and exists nowhere in the tree at all, so those two random events can never
fire -- the silent class nobody reports, since you do not miss an event you have never
seen.

⛔ Never widen the tool to make a count fall.

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

# The class a registration block is FOR: `class_<CyFoo>(...)` inline, or a loader function
# taking `class_<CyFoo>& inst`. Both spell the type inside `class_< >`, so one pattern serves.
#
# ⛔ REGISTRATION MUST BE ATTRIBUTED PER CLASS, and testing the NAME alone is what let the
# worst instance of this defect hide for years. `plot` is declared on CyUnit, CyCity, CyMap
# AND CySelectionGroup but registered on only CyCity and CyMap -- so a name-level "is it
# registered anywhere" test passes `unit.plot()`, which raises AttributeError every time it
# runs. That one call broke the military advisor's construction, and because a screen that
# throws while building never becomes an active screen, it surfaced as "ESC does not close
# the military advisor" -- a keyboard bug that was never a keyboard bug.
RE_CLASS_BLOCK = re.compile(r'class_<\s*(Cy[A-Za-z0-9_]+)\s*>')

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
    cy_registered = {}       # method name -> set of Cy classes REGISTERING it
    cy_defined = {}          # method name -> set of Cy classes defining it
    for path in walk(SOURCES, ".cpp") + walk(SOURCES, ".h"):
        text = read(path)
        registered.update(RE_REGISTERED.findall(text))
        for cls, method in RE_CY_DEFINITION.findall(text):
            cy_defined.setdefault(method, set()).add(cls)
        #	Attribute each `.def` to the class whose registration block encloses it: walk the
        #	file in order and carry the most recent `class_<CyFoo>` forward.
        current_class = None
        for match in re.finditer(r'class_<\s*(Cy[A-Za-z0-9_]+)\s*>|\.def\(\s*"([A-Za-z_][A-Za-z0-9_]*)"', text):
            if match.group(1):
                current_class = match.group(1)
            elif current_class:
                cy_registered.setdefault(match.group(2), set()).add(current_class)

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
        if method not in cy_defined:
            continue
        #	PER CLASS, never per name: the classes that DECLARE it and do not REGISTER it.
        #	A name registered on some other wrapper says nothing about this one.
        orphan_classes = cy_defined[method] - cy_registered.get(method, set())
        if not orphan_classes:
            continue
        dead.append((method, sorted(orphan_classes), py_calls[method]))

    unresolved = []
    for method in sorted(py_calls):
        if method in registered or method in cy_defined or method in py_defs:
            continue
        unresolved.append((method, py_calls[method]))

    print("registered Cy methods: %d   |   Cy methods defined in C++: %d   |   distinct Python calls: %d"
          % (len(registered), len(cy_defined), len(py_calls)))
    print("")

    if dead:
        print("UNMIGRATED CONSUMERS -- declared on a Cy class, registered nowhere, called from Python:")
        print("  NOTE: the site count is for the NAME, not for the orphan class -- the receiver is not typed.")
        print("    `area` is unregistered on CyUnit and CySelectionGroup but REGISTERED on CyPlot and CyCity,")
        print("    so most of its sites are valid plot.area() calls. Check the receiver before counting.")
        for method, classes, sites in dead:
            print("  %s   (%s)  -- %d call site(s)" % (method, ", ".join(classes), len(sites)))
            shown = sites if want_list else sites[:3]
            for site in shown:
                print("      %s" % site)
            if not want_list and len(sites) > 3:
                print("      ... and %d more (--list for all)" % (len(sites) - 3))
        print("")
    else:
        print("UNMIGRATED CONSUMERS: none")
        print("")

    if unresolved:
        print("UNRESOLVED CALLS -- a human decides:")
        print("  Python calls these on a Cy-looking receiver; they are registered nowhere,")
        print("  defined on no Cy class, and are not a Python def. Likely a removed mechanic.")
        for method, sites in unresolved:
            shown = sites if want_list else sites[:2]
            print("  %s  -- %d call site(s): %s" % (method, len(sites), ", ".join(shown)))
        print("")

    total_sites = sum(len(sites) for _, _, sites in dead)
    print("BURNDOWN: %d unmigrated method(s) across %d Python call site(s)."
          % (len(dead), total_sites))
    print("ADVISORY, and a RATCHET: the count may only FALL. A rise means a legacy declaration")
    print("was revived or a legacy call added. Close one by KILLING the declaration and")
    print("re-pointing the Python onto the new surface -- never by re-registering it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
