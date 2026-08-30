#!/usr/bin/env python
"""Census the TOOLTIP COMPOSERS and how each one builds its text.

A composer is a function that fills a text buffer for the screen -- CvGameTextMgr's set*/parse* family and
CvDLLWidgetData's parse*Help family. The spec ([patterns.md] par. THE COHERENT SURFACE, item 5) says a composer
CONSUMES rendered entry lines and never hand-assembles from getters, and it says the enumeration must be
MECHANICAL:

    "The composer that renders NOTHING is the one to hunt, and it is silent -- an empty tooltip logs no line,
     fails no build, and reads like an entity with nothing to say. Enumerate composers mechanically rather than
     trusting a screen looks populated."

That is this tool. It cannot judge whether a tooltip LOOKS right; it answers the one question that is
mechanical -- does this composer go through the shared renderer, hand-build its own text, or emit nothing at
all -- so the conversion has a denominator instead of an impression.

VERDICTS
  RENDERER   reaches the shared entry renderer (appendEntityBlocks / appendEntryLines* / appendFlatChannelLine /
             entryDetailLine / appendEdgeLines / appendClassificationLines). Converted.
  DELEGATE   hands its buffer to another composer. It fills the buffer just as surely as one that appends -- it
             is simply not the one doing the filling, so the work is at its target, not here.
             ⚠ Missing this distinction reads every delegating widget function as an EMPTY tooltip, which is the
             opposite of the truth and buries the genuinely blank ones under a hundred false ones.
  HANDBUILT  writes text but never through the renderer -- the conversion candidates. This is the RATCHET.
  SILENT     fills nothing. Either a deliberate honest gap (the legacy read was cut and the composer awaits a
             rebuild) or a tooltip nobody has noticed is blank. Both want a human.

The HANDBUILT + SILENT counts are a RATCHET and may only FALL. It is ADVISORY -- a permanently-red gate on a
known in-progress conversion is one nobody can act on -- but a RISE means a composer was added off the shared
renderer, which is the thing the spec bans.

⛔ IT ANSWERS THE MECHANISM, NEVER THE LOOK. Whether a tooltip looks right is judged against the LOOK reference
([docs/reference/tooltip-look.md]), which is free text with icon placeholders and is where a tooltip is
DESIGNED -- this tool cannot and must not be read as saying a RENDERER composer is finished.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SOURCES = [
    os.path.join("Sources", "UI", "CvGameTextMgr.cpp"),
    os.path.join("Sources", "Infrastructure", "CvDLLWidgetData.cpp"),
]

# The shared renderer's entry points. A composer reaching ANY of these consumes rendered lines.
RENDERER_CALLS = (
    "appendEntityBlocks",
    "appendEntryLines",
    "appendEntryLinesFiltered",
    "appendFlatChannelLine",
    "appendEdgeLines",
    "appendClassificationLines",
    "entryDetailLine",
    "entryConditionText",
)

# Writing to the screen at all. A composer with none of these fills nothing.
WRITE_CALLS = (
    ".append(",
    "getText(",
    "setText(",
    "szBuffer +=",
    "szString +=",
    "szHelpString +=",
)

DEF = re.compile(
    r"^(?:void|bool|int|CvWString|std::wstring)\s+(CvGameTextMgr|CvDLLWidgetData)::([A-Za-z0-9_]+)\s*\(",
    re.M,
)

# A composer takes a text buffer to fill. Anything else in these files is a helper, not a tooltip.
BUFFER_PARAM = re.compile(r"(CvWStringBuffer\s*&|CvWString\s*&|std::wstring\s*&)")


def function_body(text, brace_open):
    """Return the source between the function's outermost braces."""
    depth = 0
    for index in range(brace_open, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[brace_open : index + 1]
    return text[brace_open:]


def buffer_name(signature):
    """The name of the text buffer this composer fills, so delegation can be spotted."""
    match = re.search(r"(?:CvWStringBuffer|CvWString|std::wstring)\s*&\s*([A-Za-z_][A-Za-z0-9_]*)", signature)
    return match.group(1) if match else None


def classify(body, buffer):
    for call in RENDERER_CALLS:
        if call in body:
            return "RENDERER"
    #	A composer that HANDS ITS BUFFER ON fills it just as surely as one that appends -- it is simply not the
    #	one doing the filling. Missing this reads every delegating widget function as an empty tooltip, which is
    #	the opposite of the truth and would bury the genuinely blank ones.
    if buffer is not None:
        if re.search(r"[A-Za-z0-9_>\.\-]+\s*\(\s*[^)]*\b%s\b" % re.escape(buffer), body):
            return "DELEGATE"
    for call in WRITE_CALLS:
        if call in body:
            return "HANDBUILT"
    return "SILENT"


def main():
    show_list = "--list" in sys.argv
    verdicts = {"RENDERER": [], "DELEGATE": [], "HANDBUILT": [], "SILENT": []}

    for relative in SOURCES:
        path = os.path.join(REPO, relative)
        if not os.path.exists(path):
            sys.stderr.write("missing source: %s\n" % relative)
            return 2
        text = open(path, encoding="utf-8", errors="replace").read()

        for match in DEF.finditer(text):
            owner = match.group(1)
            name = match.group(2)
            signature_end = text.find(")", match.end())
            signature = text[match.start() : signature_end + 1] if signature_end > 0 else ""
            if not BUFFER_PARAM.search(signature):
                continue
            brace_open = text.find("{", signature_end)
            if brace_open < 0:
                continue
            body = function_body(text, brace_open)
            verdicts[classify(body, buffer_name(signature))].append("%s::%s" % (owner, name))

    total = sum(len(v) for v in verdicts.values())
    print("tooltip composers: %d" % total)
    print("  RENDERER  %4d  (consume rendered entry lines -- converted)" % len(verdicts["RENDERER"]))
    print("  DELEGATE  %4d  (hand the buffer to another composer -- not themselves the work)" % len(verdicts["DELEGATE"]))
    print("  HANDBUILT %4d  (write text off the shared renderer -- the conversion candidates)" % len(verdicts["HANDBUILT"]))
    print("  SILENT    %4d  (fill nothing -- an honest gap, or a blank nobody noticed)" % len(verdicts["SILENT"]))

    if show_list:
        for verdict in ("SILENT", "HANDBUILT", "DELEGATE", "RENDERER"):
            print("\n== %s ==" % verdict)
            for name in sorted(verdicts[verdict]):
                print("   %s" % name)
    else:
        print("\nrun with --list for the per-composer verdicts")

    print(
        "\nADVISORY. HANDBUILT + SILENT is a RATCHET and may only FALL: a rise means a composer was added off\n"
        "the shared renderer, which is what [patterns.md] par. THE DIVISION OF LABOUR bans. It never fails the run --\n"
        "a permanently-red gate on a known in-progress conversion is one nobody can act on."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
