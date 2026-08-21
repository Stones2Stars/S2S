#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify-gettext-widths -- a 64-bit value must never be passed to the EXE's varargs getText.

`gDLL->getText(szTextKey, ...)` is the closed EXE's VARARGS text formatter. Arguments are
matched to the key's placeholders POSITIONALLY BY 4-BYTE SLOT, so an 8-byte argument
occupies TWO slots and every LATER placeholder reads one slot early. A `%s` that lands on
a shifted slot receives an INTEGER, and the EXE runs `wcslen` on it -- an ACCESS_VIOLATION
at an address exactly equal to the value.

That is worth a check rather than a rule for the reasons this repo keeps choosing one:

  - the compiler CANNOT see it: varargs accept anything, so every call site compiles clean;
  - it is SILENT until that specific tooltip is actually rendered, in the specific branch
    that reaches it -- so it survives every build and most play sessions;
  - the damage is invisible at the crash site: the fault is inside msvcr71 with only EXE
    frames on the stack, and the faulting value belongs to a DIFFERENT argument than the
    one that is actually wrong;
  - and it is trivially mechanical to detect, which is the whole test for a checker.

Worked case: `CvCity::getCulture()` was widened to int64_t (correct -- it wrapped negative
in long games). It was still passed raw to TXT_KEY_CITY_BAR_CULTURE (`%d1/%d2 (%s3: Lvl
%d4)`), so %d2 read culture's HIGH half and %s3 read the culture THRESHOLD as a wchar_t*.
Hovering the city bar faulted at faultAddr == the threshold, once per session, for months.

THE FIX is never a cast to int -- that re-introduces the wrap the widening exists to fix.
Pre-render the value and hand it to a %s placeholder, which is what the tree already does:

    gDLL->getText("TXT_KEY_X", CvWString::format(L"%I64d", x.getCulture()).GetCString())

SCOPE, deliberately: only the EXE `getText` sink, because that is where the process
faults. Our own CvString/CvWString::format is NOT checked -- there the format literal sits
at the call site, so `%I64d` vs `%d` is visible to a reader rather than hidden across a
module boundary.

Run from the repo root:  python Tools/verify-gettext-widths.py
"""

import os
import re
import sys

SOURCE_ROOT = "Sources"

# Names that CONSUME a 64-bit argument correctly -- a wide/narrow format call renders it to
# a string, so a 64-bit getter nested inside one is not reaching getText as a number.
CONSUMING_CALL = re.compile(r"\b(?:CvWString|CvString)::format\s*\(")

# A 64-bit returning function, harvested from the tree so the list cannot go stale.
WIDE_RETURN_DECL = re.compile(
    r"\b(?:int64_t|uint64_t|long\s+long)\s+"          # the return type
    r"(?:[A-Za-z_][A-Za-z0-9_]*::)?"                  # optional Class::
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\("                  # the name we want
)

# The same name declared at 32 bits somewhere -- what makes a name AMBIGUOUS rather than wide.
NARROW_RETURN_DECL = re.compile(
    r"\b(?:int|short|unsigned|uint|size_t|DWORD)\s+"
    r"(?:[A-Za-z_][A-Za-z0-9_]*::)?"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\("
)

GETTEXT_CALL = re.compile(r"\bgetText\s*\(")


def source_files():
    for root, dirs, files in os.walk(SOURCE_ROOT):
        for name in files:
            if name.endswith(".cpp") or name.endswith(".h"):
                yield os.path.join(root, name)


def read(path):
    handle = open(path, "r")
    try:
        return handle.read()
    except UnicodeDecodeError:
        handle.close()
        handle = open(path, "rb")
        return handle.read().decode("utf-8", "replace")
    finally:
        handle.close()


#	The longest plausible char literal, counting the quotes: 'x', '\n', '\x41'. A lone
#	apostrophe further from its partner than this is PROSE (an English contraction in a
#	comment), not a literal -- and treating one as a literal desynchronizes the mask for
#	the whole rest of the file, which makes the checker silently MISS call sites. A
#	checker that under-reports is worse than no checker, so this bound is load-bearing.
MAX_CHAR_LITERAL = 8


def strip_literals(text):
    """Blank comments and literals, PRESERVING LENGTH AND NEWLINES so indices still align.

    Comments go first: an apostrophe inside one would otherwise open a char literal that
    never closes.
    """
    out = list(text)
    index = 0
    length = len(text)

    def blank(start, stop):
        for position in range(start, min(stop, length)):
            if out[position] != "\n":
                out[position] = " "

    while index < length:
        char = text[index]
        pair = text[index:index + 2]
        if pair == "//":
            end = text.find("\n", index)
            end = length if end < 0 else end
            blank(index, end)
            index = end
            continue
        if pair == "/*":
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            blank(index, end)
            index = end
            continue
        if char == '"':
            start = index
            index += 1
            while index < length and text[index] != '"':
                index += 2 if text[index] == "\\" else 1
            index = min(index + 1, length)
            blank(start, index)
            continue
        if char == "'":
            close = index + 1
            while close < length and close - index <= MAX_CHAR_LITERAL:
                if text[close] == "\\":
                    close += 2
                    continue
                if text[close] == "'":
                    break
                close += 1
            if close < length and close - index <= MAX_CHAR_LITERAL and text[close] == "'":
                blank(index, close + 1)
                index = close + 1
            else:
                index += 1          # a bare apostrophe in prose -- leave it alone
            continue
        index += 1

    return "".join(out)


def call_extent(text, open_paren):
    """Return the index of the ')' closing the '(' at open_paren, or -1."""
    depth = 0
    index = open_paren
    while index < len(text):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return -1


def top_level_arguments(inner, masked_inner):
    """Split a call's argument text on the commas that sit at nesting depth zero."""
    #	⛔ '<' and '>' are NOT nesting here. `pCity->getX()` and a plain comparison are far
    #	commoner in an argument list than a template is, and counting them unbalances the
    #	depth so a later top-level comma stops splitting -- i.e. a MISS. A template's comma
    #	splits one argument in two instead, which costs only the reported position: every
    #	piece is still searched, so nothing goes undetected.
    arguments = []
    depth = 0
    start = 0
    for index, char in enumerate(masked_inner):
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 0:
            arguments.append(inner[start:index])
            start = index + 1
    arguments.append(inner[start:])
    return arguments


def remove_consuming_calls(argument):
    """Drop any CvWString/CvString::format(...) sub-expression -- it renders the value."""
    while True:
        match = CONSUMING_CALL.search(argument)
        if match is None:
            return argument
        close = call_extent(argument, match.end() - 1)
        if close < 0:
            return argument[:match.start()]
        argument = argument[:match.start()] + " " + argument[close + 1:]


def arity_range(params):
    """(min, max) arguments a parameter list accepts. A defaulted parameter may be omitted."""
    stripped = params.strip()
    if not stripped or stripped == "void":
        return 0, 0
    pieces = top_level_arguments(stripped, stripped)
    required = 0
    for piece in pieces:
        if "=" not in piece:
            required += 1
    return required, len(pieces)


def width_verdict(declarations, name, argument_count):
    """WIDE / NARROW / AMBIGUOUS / None for a call of `name` taking argument_count arguments.

    ⚑ ARITY is what separates the overloads a bare name cannot: CvCity::getCulture takes a
    PlayerTypes and returns int64_t, while CvEventInfo::getCulture takes nothing and
    returns int. Matching on (name, argument count) resolves that pair exactly, and every
    other same-name/same-arity pair stays AMBIGUOUS for a human, as it must.
    """
    matches = [is_wide for is_wide, low, high in declarations.get(name, [])
               if low <= argument_count <= high]
    if not matches:
        return None
    if all(matches):
        return "WIDE"
    if not any(matches):
        return "NARROW"
    return "AMBIGUOUS"


def collect_declarations(sources):
    """Harvest every candidate declaration from the tree, so the census cannot go stale.

    Each entry is (is_wide, min_arity, max_arity) so a call can be matched on NAME AND
    ARGUMENT COUNT rather than on the name alone -- see width_verdict for why that matters.
    A same-name, same-arity pair that still disagrees stays AMBIGUOUS and is reported for a
    human rather than failing the run, exactly as the other context-dependent checks in
    this family do.
    """
    KEYWORDS = ("if", "while", "for", "switch", "return")
    declarations = {}

    def note(text, match, is_wide):
        name = match.group(1)
        if name in KEYWORDS:
            return
        close = call_extent(text, match.end() - 1)
        if close < 0:
            return
        params = text[match.end():close]
        low, high = arity_range(params)
        declarations.setdefault(name, []).append((is_wide, low, high))

    for path, text in sources:
        masked = strip_literals(text)
        if len(masked) != len(text):
            continue
        for match in WIDE_RETURN_DECL.finditer(masked):
            note(masked, match, True)
        for match in NARROW_RETURN_DECL.finditer(masked):
            note(masked, match, False)
    return declarations


def main():
    if not os.path.isdir(SOURCE_ROOT):
        print("verify-gettext-widths: run me from the repo root (no %s/ here)." % SOURCE_ROOT)
        return 2

    sources = []
    for path in source_files():
        sources.append((path.replace(os.sep, "/"), read(path)))

    declarations = collect_declarations(sources)
    wide_names = set(name for name, entries in declarations.items()
                     if any(is_wide for is_wide, low, high in entries))
    if not wide_names:
        print("verify-gettext-widths: found no 64-bit returning functions -- refusing to")
        print("report a clean run off an empty census.")
        return 2

    candidate_call = re.compile(
        r"\b(" + "|".join(sorted(re.escape(n) for n in wide_names)) + r")\s*\(")

    review = []
    findings = []
    calls_checked = 0
    for path, text in sources:
        masked = strip_literals(text)
        #	The mask is index-for-index with the source. If that ever stops holding, every
        #	offset below points at the wrong place and the run reports CLEAN while seeing
        #	nothing -- so this is a hard stop, not a warning.
        if len(masked) != len(text):
            print("verify-gettext-widths: INTERNAL -- mask desynchronized on %s" % path)
            return 2
        for match in GETTEXT_CALL.finditer(masked):
            open_paren = match.end() - 1
            close_paren = call_extent(masked, open_paren)
            if close_paren < 0:
                continue
            inner = text[open_paren + 1:close_paren]
            masked_inner = masked[open_paren + 1:close_paren]
            arguments = top_level_arguments(inner, masked_inner)
            if len(arguments) < 2:
                continue          # no varargs -- nothing can shift
            calls_checked += 1
            line = text.count("\n", 0, match.start()) + 1
            for position, argument in enumerate(arguments[1:], start=1):
                bare = remove_consuming_calls(argument)
                for hit in candidate_call.finditer(bare):
                    inner_open = hit.end() - 1
                    inner_close = call_extent(bare, inner_open)
                    if inner_close < 0:
                        continue
                    own = bare[inner_open + 1:inner_close]
                    own_count = 0 if not own.strip() else len(top_level_arguments(own, own))
                    verdict = width_verdict(declarations, hit.group(1), own_count)
                    if verdict == "WIDE":
                        findings.append((path, line, position, hit.group(1), own_count))
                    elif verdict == "AMBIGUOUS":
                        review.append((path, line, position, hit.group(1), own_count))

    for path, line, position, name, count in review:
        print("REVIEW    %s:%d  argument %d calls %s() with %d argument(s) -- declared at"
              % (path, line, position, name, count))
        print("          BOTH widths at that arity, so which overload it reaches is a human verdict.")

    if findings:
        print("")
        for path, line, position, name, count in findings:
            print("WIDE ARG  %s:%d  argument %d passes %s()/%d -- 64-bit into varargs getText"
                  % (path, line, position, name, count))
        print("")
        print("getText calls checked: %d -- %d WIDE ARGUMENT(S), %d to review"
              % (calls_checked, len(findings), len(review)))
        print("An 8-byte argument takes TWO 4-byte slots, so every later placeholder reads")
        print("one slot early and a %s lands on an integer -- the EXE then runs wcslen on")
        print("it and faults at an address equal to that value. Do NOT cast to int; render")
        print("the value and use a %s placeholder:")
        print("    CvWString::format(L\"%I64d\", x).GetCString()")
        return 1

    print("getText calls checked: %d, against %d functions declared 64-bit somewhere"
          % (calls_checked, len(wide_names)))
    print("getText widths clean -- no 64-bit value reaches the EXE's varargs formatter.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
