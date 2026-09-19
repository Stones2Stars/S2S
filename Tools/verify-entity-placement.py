#!/usr/bin/env python
"""Every EXE entity call on a unit must hand over a node that has been given a location.

A scene node the engine has never been told a location for believes it stands at the world
origin -- the map centre -- and the engine reconciles ANYTHING it is subsequently told about
that unit against that belief. So a call about a promotion layer, an era, a glow or a siege
tower walks the unit in from mid-map exactly as a move call would; what makes the difference
is not WHAT is said but whether the node was placed first
(docs/reference/unit-rendering/08-the-run-from-origin-reconciliation.md).

The compiler cannot see this. `getUnitEntity()` and `getUnitEntityPlaced()` have the same
type, both compile, and the wrong one is the shorter name -- so the rule closes only by being
checked. It has been broken three times already, each time by an agent reaching for the
nearest accessor.

FAILS on a raw `gDLL->getEntityIFace()->X(... getUnitEntity() ...)` outside the CvDLLEntity
wrapper layer. The wrappers are exempt BY NAME, and that is the point: they are the one place
allowed to pass the bare entity, because a caller reaches them through CvUnit, which places.
"""

import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES_ROOT = os.path.join(REPO_ROOT, "Sources")

#	The wrapper layer itself: these files ARE the guarded boundary, so they pass the bare entity by design.
EXEMPT_FILES = ("CvDLLEntity.cpp", "CvDLLEntity.h", "CvDLLEntityIFaceBase.h")

#	A call INTO the entity interface that hands over the bare accessor. Non-greedy across one call's arguments;
#	the interface call and the accessor must appear on one line, which every site in the tree satisfies.
RAW_CALL = re.compile(r"getEntityIFace\(\)\s*->\s*(\w+)\s*\([^;]*?\bgetUnitEntity\s*\(\s*\)")


def scan_file(path):
    findings = []
    with open(path, "rb") as handle:
        text = handle.read().decode("utf-8", "replace")

    for lineNumber, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        match = RAW_CALL.search(line)
        if match:
            findings.append((lineNumber, match.group(1), stripped))
    return findings


def main():
    violations = []

    for dirPath, dirNames, fileNames in os.walk(SOURCES_ROOT):
        dirNames[:] = [d for d in dirNames if d not in (".vs", "include", "lib", "nbproject")]
        for fileName in fileNames:
            if not fileName.endswith((".cpp", ".h")):
                continue
            if fileName in EXEMPT_FILES:
                continue
            fullPath = os.path.join(dirPath, fileName)
            for lineNumber, calledMethod, sourceLine in scan_file(fullPath):
                relativePath = os.path.relpath(fullPath, REPO_ROOT)
                violations.append((relativePath, lineNumber, calledMethod, sourceLine))

    if not violations:
        print("entity placement: OK -- every raw entity call passes a placed node.")
        return 0

    print("RAW ENTITY CALL ON A POSSIBLY-UNPLACED NODE (%d):" % len(violations))
    print("")
    for relativePath, lineNumber, calledMethod, sourceLine in violations:
        print("  %s:%d  %s" % (relativePath, lineNumber, calledMethod))
        print("      %s" % sourceLine)
    print("")
    print("Use getUnitEntityPlaced() instead of getUnitEntity() at these sites.")
    print("A node that was never given a location is presented from the world origin, so the")
    print("unit walks in from the map centre whatever the call was about. See")
    print("docs/reference/unit-rendering/08-the-run-from-origin-reconciliation.md")
    return 1


if __name__ == "__main__":
    sys.exit(main())
