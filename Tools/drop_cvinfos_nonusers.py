#!/usr/bin/env python3
r"""Zero-risk first slice of the CvInfos.h umbrella retirement (2026-06-19): drop the umbrella ONLY
from files that use NO Info type at all (pure dead include). CONSERVATIVE detection -- a file is a
"user" (umbrella KEPT) if it mentions ANY of: a Cv...Info type token, a get...Info( accessor, or the
art macros (ART_INFO / ARTFILEMGR). Only genuine non-users lose the include, so the file itself can't
break; the only residual risk is a unity batch-mate that relied on this file's umbrella transitively
-- which the clean rebuild surfaces as that batch-mate's own latent missing include (fix it there).
--apply to write; default dry-run. See docs/dev/plans/sources-structural-cleanup.md §1B.
"""
import os, re, sys

SRCDIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Sources")
SKIP = {"include", "lib", "nbproject", ".vs", ".vscode"}
UMBRELLA_RE = re.compile(r'(?m)^[ \t]*#include "CvInfos\.h"[ \t]*\r?\n')
# Broad "uses an Info?" probes -- err toward KEEPING the umbrella.
USES = [
    re.compile(r'\bCv[A-Za-z0-9_]*Info\b'),       # any Cv...Info type (incl mid-name like CvArtInfoBuilding)
    re.compile(r'\bget[A-Z][A-Za-z0-9_]*Info'),   # any get...Info( accessor (incl getArtInfoBuilding)
    re.compile(r'ART_INFO|ARTFILEMGR'),           # the art macros that expand to art-info types
]

def all_files():
    out = []
    for root, dirs, names in os.walk(SRCDIR):
        dirs[:] = [d for d in dirs if d not in SKIP]
        for n in names:
            if n.endswith(".cpp") or n.endswith(".h"):
                out.append(os.path.join(root, n))
    return out

def main():
    apply = "--apply" in sys.argv
    dropped = 0; kept = 0; samples = []
    for path in all_files():
        txt = open(path, encoding="utf-8", errors="replace").read()
        if not UMBRELLA_RE.search(txt):
            continue
        # strip the umbrella line itself before probing (so "CvInfos" doesn't count as a use)
        probe = UMBRELLA_RE.sub("", txt)
        if any(rx.search(probe) for rx in USES):
            kept += 1
            continue
        dropped += 1
        if len(samples) < 12: samples.append("  " + os.path.relpath(path, SRCDIR))
        if apply:
            open(path, "w", encoding="utf-8", newline="").write(UMBRELLA_RE.sub("", txt))
    print(f"{'APPLIED' if apply else 'DRY'}: dropped umbrella from {dropped} non-users; KEPT in {kept} users")
    for s in samples: print(s)

if __name__ == "__main__":
    main()
