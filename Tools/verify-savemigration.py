#!/usr/bin/env python
"""Verify Assets/savemigration.txt against the tree.

The ledger is load-bearing twice over: it drains a cut field's orphan bytes out of every old save, and it
is the only record of what OWES a value once a field is gone (AGENTS.md). Both halves fail silently, so
save.md documents the checks and nothing ran them -- this does.

  A. DRAIN-LIVE-STATE (save.md par.3, the dangerous one). A LISTED tag whose member is still serialized
     drains a live WRAPPER_READ's element, so the member keeps its default and the value is lost on EVERY
     load -- quieter than the unlisted-orphan desync, and mechanically checkable: intersect the ledger's
     bare Class::m_field entries with members still carrying a WRAPPER_READ/WRITE in their owning class.
     Resolve a hit by asking which side is right, never by reflex -- a member whose only writer is
     applyEvent is genuine one-shot event state that CORRECTLY stays serialized, so there the ENTRY is the
     defect.

  B. BRACKETED ENTRY (save.md par.3). A decorated per-element tag (m_ppaai...[iI]) normalizes to a name the
     C++ source literal does not match, so an entry for one silently fails to drain. Those keep their
     enum-remapping drain loop instead; an entry for one is inert-but-false.

  C. PROSE MISREAD (save.md par.3). Nothing marks prose as prose except the leading '|', so a wrapped note
     line that BEGINS with a Class:: token registers as an entry -- and would then drain a tag nobody meant
     to cut. Detected as trailing PROSE after the tag; a bare tag, a '(type)' parenthetical or a '-> rename'
     are all legitimate. ⚠ A non-m_ name is NOT by itself a defect: save.md sanctions the bracket-FREE
     decorated sub-tag (a variable-length block's ...Size / ...Type / ...Value), which names a stream tag
     rather than a member.

Exit 1 on any finding. Run after editing the ledger or cutting a serialized member.
"""
import os
import re
import sys
import glob
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEDGER = os.path.join(ROOT, 'Assets', 'savemigration.txt')

WRAPPED = re.compile(r'WRAPPER_(?:READ|WRITE)[A-Z_]*\s*\([^;]*?(m_[A-Za-z0-9_]+)')


def sm_token(text):
    """The engine's sm_token: the FIRST whitespace-delimited token, kept only if it contains '::'."""
    token = text.strip().split(None, 1)
    token = token[0] if token else ''
    return token if '::' in token else ''


def read_entries():
    """Mirror CvTaggedSaveFormatWrapper's sm_ensureLoaded EXACTLY -- a check that models a stricter parser
    than the engine's under-reports, which is how the prose hazard hides. Yields (kind, tag, lineno, line)."""
    out = []
    for lineno, raw in enumerate(open(LEDGER, encoding='utf-8', errors='replace'), 1):
        stripped = raw.lstrip()
        if not stripped.strip() or stripped[0] in '|=#':
            continue
        arrow = raw.find('->')
        if arrow != -1:
            old, new = sm_token(raw[:arrow]), sm_token(raw[arrow + 2:])
            if old and new:
                out.append(('rename', old, lineno, raw.strip()))
        else:
            cut = sm_token(raw)
            if cut:
                out.append(('cut', cut, lineno, raw.strip()))
    return out


def read_serialized():
    """member names still carrying a WRAPPER_READ/WRITE, keyed by owning source basename."""
    table = collections.defaultdict(set)
    for path in glob.glob(os.path.join(ROOT, 'Sources', '**', '*.cpp'), recursive=True):
        if '.vs' in path:
            continue
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except IOError:
            continue
        owner = os.path.basename(path)[:-4]
        for match in WRAPPED.finditer(text):
            table[owner].add(match.group(1))
    return table


def main():
    entries = read_entries()
    serialized = read_serialized()
    findings = []

    for kind, tag, lineno, line in entries:
        cls, _, member = tag.partition('::')
        if '[' in tag or ']' in tag:
            findings.append((lineno, 'B bracketed-entry',
                             '%s can never match the normalized tag -- keep its drain loop instead' % tag))
            continue
        # Whatever follows the tag must be nothing, a "(type)" parenthetical, or the rename arrow. Anything
        # else is prose that the engine's first-token parse silently registered as a real entry.
        tail = line[len(tag):].strip()
        if tail and not tail.startswith('(') and not tail.startswith('->'):
            findings.append((lineno, 'C prose-misread',
                             'trailing prose makes this register as a %s: %r' % (kind, line[:66])))
            continue
        # Only a CUT drains; a rename remaps. And a bracket-free decorated sub-tag names a stream tag rather
        # than a member, so it cannot collide with a WRAPPER'd member name.
        if kind == 'cut' and member.startswith('m_') and member in serialized.get(cls, ()):
            findings.append((lineno, 'A drains-live-state',
                             '%s is LISTED but still has a WRAPPER_READ/WRITE -- every load loses it' % tag))

    print('savemigration entries checked: %d' % len(entries))
    if not findings:
        print('savemigration clean -- no drained live state, no unmatchable entries.')
        return 0
    for lineno, kind, detail in findings:
        print('  savemigration.txt:%d  [%s]  %s' % (lineno, kind, detail))
    print('\n%d finding(s). See save.md par.3 -- resolve by deciding which SIDE is right, never by reflex.' % len(findings))
    return 1


if __name__ == '__main__':
    sys.exit(main())
