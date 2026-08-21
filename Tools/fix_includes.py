#!/usr/bin/env python3
"""Case-insensitive include-fixer for the post-move tree (structural cleanup 2026-06-19).

The first move pass rewrote cross-bucket includes case-SENSITIVELY, so pre-existing case-mismatched
includes (e.g. CvBuildingInfo.h: #include "IDValuemap.h" -> real file IDValueMap.h) were missed and
broke when the target moved to a bucket. This re-runs the rewrite over the CURRENT tree with a
case-INSENSITIVE lookup, emitting the REAL filename. Idempotent for already-qualified includes.

Rule: an #include "X" whose basename resolves (case-insensitively) to a header now in a BUCKET folder
is rewritten to "Folder/RealName.h" (cross-folder) or "RealName.h" (same folder, MSVC same-dir rule).
Includes to shared layers (Infos/Cascade/Repos/Utils), root glue, and system/<...> are left alone.
"""
import os, re, sys

SRCDIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Sources")
SKIP_DIRS = {"include", "lib", "nbproject", ".vs", ".vscode"}
BUCKET_FOLDERS = {"Engine", "AI", "Infrastructure", "Tools", "UI", "Python", "Defines"}
INC_RE = re.compile(r'(#\s*include\s*")([^"]+)(")')

def scan():
    """relfolder-by-basename for every project .cpp/.h under Sources/."""
    ff = {}                     # lower(basename) -> (relfolder, realbasename)
    files = []                  # (abspath, relfolder)
    for root, dirs, names in os.walk(SRCDIR):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        rel = os.path.relpath(root, SRCDIR)
        rel = "" if rel == "." else rel.replace(os.sep, "/")
        for n in names:
            if n.endswith(".cpp") or n.endswith(".h"):
                ff[n.lower()] = (rel, n)
                files.append((os.path.join(root, n), rel))
    return ff, files

def main():
    ff, files = scan()
    apply = "--apply" in sys.argv
    total = 0; touched = 0; samples = []
    for path, myfolder in files:
        txt = open(path, encoding="utf-8", errors="replace").read()
        cnt = [0]
        def repl(m):
            inc = m.group(2); base = os.path.basename(inc)
            hit = ff.get(base.lower())
            if not hit: return m.group(0)
            folder, real = hit
            if folder not in BUCKET_FOLDERS: return m.group(0)
            new = real if folder == myfolder else f"{folder}/{real}"
            if new != inc:
                cnt[0] += 1
                if len(samples) < 15: samples.append(f"  {myfolder or '(root)'}/{os.path.basename(path)}: {inc} -> {new}")
            return m.group(1) + new + m.group(3)
        txt2 = INC_RE.sub(repl, txt)
        if cnt[0]:
            touched += 1; total += cnt[0]
            if apply: open(path, "w", encoding="utf-8", newline="").write(txt2)
    print(f"{'APPLIED' if apply else 'DRY'}: {total} include fixes across {touched} files")
    for s in samples: print(s)

if __name__ == "__main__":
    main()
