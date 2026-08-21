#!/usr/bin/env python3
r"""Retire the CvInfos.h umbrella (structural cleanup 2026-06-19).

⚠ WIP — the 2026-06-19 run was REVERTED (fragile tail). DO NOT rerun naively: it injects Info
includes into FOUNDATIONAL/EXE-bound headers (CvInfoBase.h, CvEnums.h, CvCity/Unit/Player/...) which
creates include CYCLES. Before reuse: add a foundational/EXE-bound-header EXCLUDE list + handle headers
via forward-decls (only by-value use needs the include). Full lessons: docs/dev/plans/sources-structural-cleanup.md §1B.


The umbrella pulls 113 Info headers; it is included by ~178 files and is INCOMPLETE (omits some).
Replace each '#include "CvInfos.h"' with the specific Cv<X>Info.h headers that file actually uses
(bare -- Infos/ is on /I). Files using no Info type just drop the include. Build + a UnityNumFiles
perturbation catch any transitive-reliance breaks (files that used an Info type via a batch-mate's
umbrella). --apply to write; default dry-run.
"""
import os, re, glob, sys

SRCDIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Sources")
SKIP = {"include", "lib", "nbproject", ".vs", ".vscode"}
UMBRELLA_RE = re.compile(r'(?m)^[ \t]*#include "CvInfos\.h"[ \t]*\r?\n')

def class_to_header():
    m = {}
    for h in glob.glob(os.path.join(SRCDIR, "Infos", "*.h")):
        base = os.path.basename(h)
        txt = open(h, encoding="utf-8", errors="replace").read()
        for cm in re.finditer(r'(?m)^\s*class\s+(Cv[A-Za-z0-9_]+Info)\b', txt):
            m.setdefault(cm.group(1), base)
    return m

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
    cls2hdr = class_to_header()
    print(f"mapped {len(cls2hdr)} Info classes -> headers")
    changed = 0; added = 0; dropped = 0; unmapped = set()
    samples = []
    for path in all_files():
        txt = open(path, encoding="utf-8", errors="replace").read()
        if not UMBRELLA_RE.search(txt):
            continue
        base = os.path.basename(path)
        # Info classes referenced in this file
        used = set(re.findall(r'\bCv[A-Za-z0-9_]+Info\b', txt))
        for u in used:
            if u not in cls2hdr and u not in ("CvInfos",):
                unmapped.add(u)
        hdrs = sorted({cls2hdr[u] for u in used if u in cls2hdr})
        # don't include self, and don't duplicate an already-present include
        hdrs = [h for h in hdrs if h != base and f'#include "{h}"' not in txt]
        if hdrs:
            block = "".join(f'#include "{h}"\n' for h in hdrs)
        else:
            block = ""
            dropped += 1
        new = UMBRELLA_RE.sub(block, txt, count=1)
        # if multiple umbrella lines (rare), drop the rest
        new = UMBRELLA_RE.sub("", new)
        if new != txt:
            changed += 1; added += len(hdrs)
            if len(samples) < 12:
                samples.append(f"  {os.path.relpath(path, SRCDIR)}: +{len(hdrs)} specifics" + (" (dropped, no Info use)" if not hdrs else ""))
            if apply:
                open(path, "w", encoding="utf-8", newline="").write(new)
    print(f"{'APPLIED' if apply else 'DRY'}: {changed} files, +{added} specific includes, {dropped} bare-dropped")
    for s in samples: print(s)
    if unmapped:
        print(f"UNMAPPED Cv*Info tokens ({len(unmapped)}) — verify these resolve elsewhere:")
        for u in sorted(unmapped)[:30]: print("   ", u)

if __name__ == "__main__":
    main()
