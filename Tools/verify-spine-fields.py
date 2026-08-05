#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify-spine-fields -- a spine field's DECLARED type must match how emits pass it.

A CvSpineEvent field is declared once in the field-info switch (`*peType = SFT_X`) and
filled at each emit by an adder (`addI` / `addStr` / `addW` / `addF` / `addB`). The two
must agree, because the renderer switches on the DECLARED type: a field declared SFT_STR
but filled with addI makes `spineRenderEventLine` read the integer AS A `char*`, so the
process dies with an ACCESS_VIOLATION at an address equal to the value.

That is worth a check rather than a rule for the reasons this repo keeps choosing one:

  - it is SILENT until the field is actually rendered, so it survives every build and
    every load that does not happen to emit that fact at a level the log gate passes;
  - the compiler cannot see it -- both sides are ints at the call site, and the mismatch
    lives between a switch arm and a call in another function;
  - and it is trivially mechanical to detect, which is the whole test for a checker.

Worked case: SPF_OBJECT_KIND was moved from `addStr(propertyObjectKindName(...))` to a
raw `addI` (correct -- a per-property, per-object load emit must not resolve a name string
at emit time) while its declaration stayed SFT_STR. The tree was red at the time, so
nothing could run; the first green build crashed on load reading faultAddr 0x00000005 --
the object-kind value itself.

Run from the repo root:  python Tools/verify-spine-fields.py
"""

import io
import os
import re
import sys
import collections

SPINE = os.path.join("Sources", "Spine", "CvEventSpine.cpp")

# The adder each declared type must be filled by. Every typed INDEX kind (SFT_BUILDING,
# SFT_UNIT, SFT_PROPERTY, ...) is an id carried as a plain int, so it takes addI like
# SFT_INT does; only the four non-int payloads have adders of their own.
ADDER_FOR_TYPE = {
    "SFT_STR": "Str",
    "SFT_WSTR": "W",
    "SFT_FLOAT": "F",
    "SFT_BOOL": "B",
}
DEFAULT_ADDER = "I"


def main():
    if not os.path.isfile(SPINE):
        sys.stderr.write("verify-spine-fields: cannot find %s -- run from the repo root\n" % SPINE)
        return 2

    source = io.open(SPINE, encoding="utf-8").read()

    declared = {}
    for match in re.finditer(r"case\s+(SPF_[A-Z_0-9]+):\s*\*peType\s*=\s*(SFT_[A-Z_0-9]+);", source):
        declared[match.group(1)] = match.group(2)

    used = collections.defaultdict(set)
    for match in re.finditer(r"\.add(I|Str|W|F|B)\(\s*(SPF_[A-Z_0-9]+)", source):
        used[match.group(2)].add(match.group(1))

    if not declared:
        sys.stderr.write("verify-spine-fields: no field declarations found -- has the "
                         "field-info switch moved? Fix this tool, never widen it.\n")
        return 2

    failures = []
    for tag in sorted(declared):
        expected = ADDER_FOR_TYPE.get(declared[tag], DEFAULT_ADDER)
        adders = used.get(tag)
        if not adders:
            continue                      # declared but never emitted -- not this check's business
        if adders != set([expected]):
            failures.append((tag, declared[tag], expected, sorted(adders)))

    emitted = len([t for t in declared if used.get(t)])
    if failures:
        for tag, decl_type, expected, adders in failures:
            print("MISMATCH  %-22s declared %-15s wants add%-4s but is emitted with %s"
                  % (tag, decl_type, expected, ", ".join("add" + a for a in adders)))
        print("")
        print("spine fields checked: %d declared, %d emitted -- %d MISMATCH"
              % (len(declared), emitted, len(failures)))
        print("A mismatch is a CRASH at render, not a formatting defect: the renderer")
        print("switches on the declared type, so an int reaches it as a char*. Fix the")
        print("side that is wrong -- and prefer the raw int, since a payload carries")
        print("typed fields and never a resolved string (event-spine.md).")
        return 1

    print("spine fields checked: %d declared, %d emitted" % (len(declared), emitted))
    print("spine fields clean -- every declared type matches how its emits fill it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
