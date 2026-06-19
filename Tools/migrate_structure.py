#!/usr/bin/env python3
"""S2S Sources/ structural-cleanup migration (2026-06-19).

Phase A (this script, --dry default): compute the file -> bucket -> target-folder map from the
legacy .vcxproj.filters classification, re-bucketed under the new broad top-level. Reports counts
+ unmapped + new files. Writes Tools/movemap.tsv. Moves nothing unless --apply.

Buckets (broad, fine-tune later): Engine / AI / Infos / Cascade / Repos / Infrastructure / Tools /
UI / Python / Defines, plus (root) for PCH + global glue. See docs/dev/plans/sources-structural-cleanup.md.
"""
import re, os, glob, sys

SRC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # repo root
SRCDIR = os.path.join(SRC, "Sources")
FILTERS = os.path.join(SRCDIR, "C2C (VS2019).vcxproj.filters")
BS = chr(92)  # backslash, kept out of literals to dodge escaping hell

def load_filter_map():
    txt = open(FILTERS, encoding="utf-8", errors="replace").read()
    pat = re.compile(r'<Cl(?:Compile|Include)\s+Include="([^"]+)"\s*(?:/>|>(.*?)</Cl(?:Compile|Include)>)', re.S)
    fmap = {}
    for m in pat.finditer(txt):
        inc = m.group(1); body = m.group(2) or ""
        fm = re.search(r"<Filter>([^<]*)</Filter>", body)
        filt = (fm.group(1) if fm else "").replace(BS, "/")   # normalize to forward slash
        fmap[os.path.basename(inc)] = filt
    return fmap

ROOT_GLUE = {"CvGameCoreDLL.cpp","CvGameCoreDLL.h","CvGameCoreDLLDefNew.h","CvGameCoreDLLUndefNew.h",
             "resource.h","_precompile.cpp","GlobalDefines.h","AI_Defines.h"}

# Explicit homes for files the legacy .filters left unclassified (or in a default filter).
OVERRIDES = {
    "CvArmy.h":"Engine", "CvArmy.cpp":"Engine",
    "CvUnitSelectionCriteria.cpp":"Engine", "CvUnitSelectionCriteria.h":"Engine",
    "CvOverlord.h":"Engine",
    "ConstructRequirement.h":"Engine",
    "Win32.cpp":"Tools", "Win32.h":"Tools",
    "CvCityLogTags.h":"AI",   # [CIT] logging tags (collision-proofed in Phase 2)
}

def bucket(name, filt):
    if name in OVERRIDES: return OVERRIDES[name]
    if name in ROOT_GLUE: return "(root)"
    if name.startswith("Cy"): return "Python"
    if re.search(r"AI\.(cpp|h)$", name) or name.startswith("BetterBTSAI") or "ContractBroker" in name: return "AI"
    if "Base/Infos" in filt or re.search(r"Info\.(cpp|h)$", name) or re.search(r"Info[A-Z]", name): return "Infos"
    if "Base/Cascade" in filt: return "Cascade"
    if "Base/Repos" in filt: return "Repos"
    if filt.startswith("Source/UI") or "/UI" in filt: return "UI"
    if "Infrastructure" in filt: return "Infrastructure"
    if "/Tools" in filt: return "Tools"
    if re.search(r"DataDefinitions|DataTypes|Globals", filt): return "Defines"
    if "Base" in filt: return "Engine"
    return "UNMAPPED:" + (filt or "<none>")

def main():
    fmap = load_filter_map()
    disk = sorted(set(os.path.basename(p) for p in
                      glob.glob(os.path.join(SRCDIR,"*.cpp")) + glob.glob(os.path.join(SRCDIR,"*.h"))))
    raw = {f: bucket(f, fmap.get(f, "")) for f in disk}
    # .cpp/.h pairing: a stem's two files share one bucket; prefer the non-default/non-unmapped one.
    def is_weak(b): return b.startswith("UNMAPPED") or b == "(root)"
    for f in list(raw):
        stem, ext = os.path.splitext(f)
        if ext not in (".cpp", ".h"): continue
        other = stem + (".h" if ext == ".cpp" else ".cpp")
        if other in raw and raw[f] != raw[other]:
            a, b = raw[f], raw[other]
            if is_weak(a) and not is_weak(b): raw[f] = b
            elif is_weak(b) and not is_weak(a): raw[other] = a
    rows = [(f, raw[f], fmap.get(f, "")) for f in disk]
    from collections import Counter
    c = Counter(b for _, b, _ in rows)
    print("=== on-disk loose files:", len(disk), "===")
    for k, v in sorted(c.items(), key=lambda x: -x[1]):
        print(f"{v:4d}  {k}")
    print("\n=== files NOT in .filters (new/unassigned) ===")
    for f, b, _ in rows:
        if f not in fmap: print(f"  {f} -> {b}")
    print("\n=== UNMAPPED (need a rule) ===")
    for f, b, filt in rows:
        if b.startswith("UNMAPPED"): print(f"  {f}  [{filt}]")
    out = os.path.join(SRC, "Tools", "movemap.tsv")
    open(out, "w").write("\n".join(f"{f}\t{b}" for f, b, _ in rows) + "\n")
    print("\nwrote", out)

if __name__ == "__main__":
    main()
