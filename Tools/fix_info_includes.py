#!/usr/bin/env python3
r"""Ensure every file #includes the specific Cv<X>Info.h headers it actually uses (IWYU for Infos).

⚠ WIP — the 2026-06-19 run was REVERTED. It injects into HEADERS too, and adding Info includes to
foundational/EXE-bound headers (CvInfoBase.h, CvEnums.h, CvCity/Unit/Player/...) creates include CYCLES.
Before reuse: EXCLUDE foundational/EXE-bound headers + use forward-decls in headers. See
docs/dev/plans/sources-structural-cleanup.md §1B for the full lessons.


Completes the CvInfos.h umbrella retirement: a file "uses" an Info type if it names the type
(\bCv\w+Info\b) OR calls an accessor get<X>Info(  (-> Cv<X>Info). For each file, add any needed
Cv<X>Info.h that isn't already included (bare -- Infos/ is on /I), inserted right after the PCH
include (#include "CvGameCoreDLL.h") or the first #include. Idempotent. --apply to write.
"""
import os, re, glob, sys

SRCDIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Sources")
SKIP = {"include", "lib", "nbproject", ".vs", ".vscode"}
TYPE_RE = re.compile(r'\b(Cv[A-Za-z0-9_]+Info)\b')
ACC_RE  = re.compile(r'\bget([A-Z][A-Za-z0-9_]*?)Info\s*\(')

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
    changed = 0; added = 0; unmapped = set(); samples = []
    for path in all_files():
        base = os.path.basename(path)
        txt = open(path, encoding="utf-8", errors="replace").read()
        types = set(TYPE_RE.findall(txt))
        for x in ACC_RE.findall(txt):
            types.add("Cv" + x + "Info")
        needed = []
        for t in sorted(types):
            h = cls2hdr.get(t)
            if not h:
                if t in types and t not in ("CvInfos",): unmapped.add(t)
                continue
            if h == base: continue
            if f'#include "{h}"' in txt: continue
            needed.append(h)
        if not needed: continue
        block = "".join(f'#include "{h}"\n' for h in needed)
        # anchor: after the PCH include, else after first #include
        m = re.search(r'(?m)^[ \t]*#include "CvGameCoreDLL\.h"[ \t]*\r?\n', txt)
        if not m:
            m = re.search(r'(?m)^[ \t]*#include [^\n]*\r?\n', txt)
        if not m:
            continue
        new = txt[:m.end()] + block + txt[m.end():]
        changed += 1; added += len(needed)
        if len(samples) < 10: samples.append(f"  {os.path.relpath(path, SRCDIR)}: +{len(needed)}")
        if apply:
            open(path, "w", encoding="utf-8", newline="").write(new)
    print(f"{'APPLIED' if apply else 'DRY'}: {changed} files, +{added} Info includes")
    for s in samples: print(s)
    if unmapped:
        print(f"UNMAPPED ({len(unmapped)}):", ", ".join(sorted(unmapped)[:20]))

if __name__ == "__main__":
    main()
