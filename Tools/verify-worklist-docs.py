#!/usr/bin/env python
"""
verify-worklist-docs.py -- fail the WORKLIST docs when they carry STATE.

WHY THIS EXISTS (and why it is a check rather than a rule)
----------------------------------------------------------
DEC-spec-plus-todo has always said a doc is a SPEC (timeless design) or a TODO
(a short bulleted list of what is NOT done) -- never both, and never status woven
through prose. The rule was in place and was ignored anyway: the todo grew to 785
lines of censuses, counts, file:line refs and verified-in-tree claims, and it then
handed out work that no longer existed (entries anchored on symbols long deleted).

A rule has to be remembered by every future agent, which is the enforcement model
this project keeps watching fail. A CHECK does not: it makes the violation
unsayable rather than forbidden -- the same move that fixed the duplicated skill
reads (patterns.md: "a contract makes the violation unsayable").

⛔ The trap this closes is NOT limited to stale text: state in a worklist gets
TRUSTED, including by the agent who wrote it minutes earlier. That is the failure
mode this is aimed at.

WHAT IT CHECKS
--------------
Only the two docs that carry an explicit no-status contract:
  * docs/plans/structural-cleanup/todo.md     -- "What has to be DONE, nothing else"
  * docs/plans/structural-cleanup/roadmap.md  -- "carries NO status and NO worklist"

Everything else under docs/plans/ is deliberately exempt: docs/plans/parked/ is
carried AS-IS with stale status expected, and the audits/studies/censuses beside
the roadmap (property-audit, stub-census, the unitcombat maps) are state BY DESIGN.

Usage:
    python Tools/verify-worklist-docs.py            # check; exit 1 on any finding
    python Tools/verify-worklist-docs.py --list     # show what is checked and why
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def out(line):
    """Write a line the Windows console can always render (the docs are full of ⛔/⚑/§)."""
    try:
        print(line)
    except UnicodeEncodeError:
        enc = getattr(sys.stdout, "encoding", None) or "ascii"
        print(line.encode(enc, "replace").decode(enc, "replace"))

CHECKED = [
    os.path.join("docs", "plans", "structural-cleanup", "todo.md"),
    os.path.join("docs", "plans", "structural-cleanup", "roadmap.md"),
]

# Each rule: (name, compiled pattern, why it is state)
#
# ⚠ PRECISION IS THE WHOLE VALUE HERE. A check that cries wolf gets ignored, which would leave it exactly as
# worthless as the rule it replaces. Every pattern below is deliberately narrow, and each was tightened after a
# real false positive: "reverse-landed" (a technical term for what the reverse pass does) tripped a naive
# \bLANDED\b, and "every removal verified against source/data" (a REQUIREMENT for future work) tripped a naive
# "verified against". Prefer missing a violation to inventing one.
RULES = [
    ("completion-marker",
     # CAPS-only status words -- the spellings DEC-spec-plus-todo names -- and never inside a hyphenated term.
     re.compile(r"(?:✅|(?<![-\w])(?:LANDED|PARTLY LANDED)\b|^\s*[-*]\s*\[[xX]\])", re.M),
     "a finished item is DELETED, never ticked -- git history is the record of work done"),

    ("source-location",
     re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\.(?:cpp|h|py)\s*[:(]\s*\d+"),
     "a file:line ref is a claim about the tree and drifts the moment anything moves"),

    ("verification-claim",
     # Only RECORDED-verification shapes. Bare "verified" is left alone: a worklist legitimately REQUIRES that
     # future work be verified, and banning the word would fight the docs rather than the drift.
     re.compile(r"(?:\bverified in tree\b|\bconfirmed in tree\b|\bre-measured\b|"
                r"^\s*>?\s*\*?\*?Verified against:|\bMeasured(?::|\s+at\b)|\bas of (?:today|this writing)\b)",
                re.I | re.M),
     "the tree is the authority; a recorded verification is stale the moment it is written"),

    ("bolded-count",
     # A bolded bare number in a worklist is a census result essentially every time -- "**164** (of 522)".
     re.compile(r"\*\*\s*(?:~\s*)?\d[\d,]*\s*\+?\s*(?:sites?|call sites?|files?|lines?|"
                r"declarations?|names?|getters?|units?|entries|authorings?)?\s*\*\*", re.I),
     "a bolded count is a census result; counts drift and then get trusted"),

    ("tree-count",
     # A counted assertion about what is IN the tree or the data. Deliberately excludes counts that describe a
     # TOOL rather than the tree (e.g. "MSVC stops at 100 errors per TU"), which are durable facts, not state.
     re.compile(r"\b\d[\d,]+\s*\+?\s*(?:units?|buildings?|promotions?|techs?|call sites?|sites?|files?|"
                r"authorings?|declarations?|distinct names?|consumers?|TUs?)\b", re.I),
     "a count of what is in the tree/data is state -- it drifts, and then it gets trusted"),

    ("status-table",
     re.compile(r"^\|.*\|\s*(?:status|landed|built|done|progress)\s*\|", re.I | re.M),
     "a per-item status/completion table is the ledger DEC-spec-plus-todo forbids outright"),
]


def check(path):
    full = os.path.join(REPO, path)
    if not os.path.isfile(full):
        return [(0, "missing-file", path, "checked doc does not exist")]
    try:
        raw = open(full, encoding="utf-8").read()
    except UnicodeDecodeError:
        raw = open(full, encoding="utf-8", errors="replace").read()

    # Blank out two regions rather than dropping them, so reported line numbers stay true:
    #   * the PREAMBLE (everything before the first "## " heading) -- it STATES the no-state contract, so it has
    #     to be able to name the very things it bans;
    #   * fenced code blocks -- a command or an authoring example is not a claim about the tree.
    lines = raw.splitlines(True)
    first_heading = next((i for i, l in enumerate(lines) if l.startswith("## ")), 0)
    fenced = False
    kept = []
    for i, line in enumerate(lines):
        if line.lstrip().startswith("```"):
            fenced = not fenced
            kept.append("\n" * line.count("\n"))
            continue
        if i < first_heading or fenced:
            kept.append("\n" * line.count("\n"))
        else:
            kept.append(line)
    text = "".join(kept)

    # line offsets so a match can report its line
    starts = [0]
    for line in text.splitlines(True):
        starts.append(starts[-1] + len(line))

    def line_of(pos):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= pos:
                lo = mid
            else:
                hi = mid - 1
        return lo + 1

    findings = []
    seen = set()
    body = text.splitlines()
    for name, pattern, why in RULES:
        for mo in pattern.finditer(text):
            ln = line_of(mo.start())
            if (ln, name) in seen:      # one report per (line, rule) -- a line often trips a rule twice
                continue
            seen.add((ln, name))
            snippet = body[ln - 1].strip() if ln - 1 < len(body) else ""
            if len(snippet) > 110:
                snippet = snippet[:107] + "..."
            findings.append((ln, name, snippet, why))
    findings.sort()
    return findings


def main():
    if "--list" in sys.argv:
        out("Checked docs (the two with an explicit no-status contract):")
        for p in CHECKED:
            out("  " + p.replace("\\", "/"))
        out("\nRules:")
        for name, _, why in RULES:
            out("  %-20s %s" % (name, why))
        out("\nEverything else under docs/plans/ is exempt BY DESIGN "
            "(parked/ is carried as-is; the audits and censuses beside the roadmap are state on purpose).")
        return 0

    # An explicit path checks that file instead -- for looking at a doc before it lands.
    targets = [a for a in sys.argv[1:] if not a.startswith("-")] or CHECKED

    total = 0
    for path in targets:
        findings = check(path)
        if not findings:
            continue
        total += len(findings)
        out("\n%s" % path.replace("\\", "/"))
        for ln, name, snippet, why in findings:
            out("  %5d  [%s]  %s" % (ln, name, snippet))
            out("         -> %s" % why)

    if total:
        out("\n%d finding(s). A WORKLIST states what has to be DONE -- the tree holds the state," % total)
        out("the specs hold the design, git history holds what was achieved.")
        out("Fix by DELETING the state, or by moving a durable ruling into its owning spec.")
        return 1

    out("worklist docs clean -- no state found in %d doc(s)." % len(CHECKED))
    return 0


if __name__ == "__main__":
    sys.exit(main())
