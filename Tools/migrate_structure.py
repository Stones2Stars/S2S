#!/usr/bin/env python3
"""S2S Sources/ structural-cleanup migration (2026-06-19).

DRY (default): compute the file->bucket map from the legacy .vcxproj.filters classification,
re-bucketed under the new broad flat top-level. Reports counts; writes Tools/movemap.tsv.

--apply: (1) os.rename each loose file into its bucket folder; (2) rewrite #include "..." that
target a NEW BUCKET-folder header -> path-qualified "Bucket/Name.h" (cross-folder) or bare
(same folder, MSVC same-dir rule). Includes to Infos/Cascade/Repos/Utils/root-glue and
system/<...> includes are LEFT ALONE (resolved via the existing /I: include,$SOURCE_DIR$,Infos,Cascade).
PCH glue stays at root. No fbuild /I change needed.

Buckets: Engine / AI / Infos / Cascade / Repos / Infrastructure / Tools / UI / Python / Defines + (root).
Plan: docs/dev/plans/sources-structural-cleanup.md.
"""
import re, os, glob, sys

SRC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # repo root
SRCDIR = os.path.join(SRC, "Sources")
FILTERS = os.path.join(SRCDIR, "C2C (VS2019).vcxproj.filters")
BS = chr(92)

# Bucket folders whose includes must be PATH-QUALIFIED (not on /I). The shared layers
# (Infos/Cascade/Repos/Utils) and root are reachable via /I or same-dir, so leave those includes alone.
BUCKET_FOLDERS = {"Engine", "AI", "Infrastructure", "Tools", "UI", "Python", "Defines"}
LEAVE_FOLDERS  = {"Infos", "Cascade", "Repos", "Utils", "(root)"}

ROOT_GLUE = {"CvGameCoreDLL.cpp","CvGameCoreDLL.h","CvGameCoreDLLDefNew.h","CvGameCoreDLLUndefNew.h",
             "resource.h","_precompile.cpp","GlobalDefines.h","AI_Defines.h"}
OVERRIDES = {
    "CvArmy.h":"Engine", "CvArmy.cpp":"Engine",
    "CvUnitSelectionCriteria.cpp":"Engine", "CvUnitSelectionCriteria.h":"Engine",
    "CvOverlord.h":"Engine", "ConstructRequirement.h":"Engine",
    "Win32.cpp":"Tools", "Win32.h":"Tools",
    "CvCityLogTags.h":"AI",
}

def load_filter_map():
    txt = open(FILTERS, encoding="utf-8", errors="replace").read()
    pat = re.compile(r'<Cl(?:Compile|Include)\s+Include="([^"]+)"\s*(?:/>|>(.*?)</Cl(?:Compile|Include)>)', re.S)
    fmap = {}
    for m in pat.finditer(txt):
        inc = m.group(1); body = m.group(2) or ""
        fm = re.search(r"<Filter>([^<]*)</Filter>", body)
        fmap[os.path.basename(inc)] = (fm.group(1) if fm else "").replace(BS, "/")
    return fmap

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

def loose_map():
    """basename -> bucket, for the loose root files, with .cpp/.h pairing."""
    fmap = load_filter_map()
    disk = sorted(set(os.path.basename(p) for p in
                      glob.glob(os.path.join(SRCDIR,"*.cpp")) + glob.glob(os.path.join(SRCDIR,"*.h"))))
    raw = {f: bucket(f, fmap.get(f, "")) for f in disk}
    def weak(b): return b.startswith("UNMAPPED") or b == "(root)"
    for f in list(raw):
        stem, ext = os.path.splitext(f)
        if ext not in (".cpp",".h"): continue
        other = stem + (".h" if ext==".cpp" else ".cpp")
        if other in raw and raw[f]!=raw[other]:
            a,b = raw[f],raw[other]
            if weak(a) and not weak(b): raw[f]=b
            elif weak(b) and not weak(a): raw[other]=a
    return raw

def folder_of_bucket(b):
    return "" if b == "(root)" else b

def all_file_folders(lm):
    """basename -> folder (relative to Sources/, '' for root), for ALL project files post-move."""
    ff = {}
    for f, b in lm.items():
        ff[f] = folder_of_bucket(b)
    for sub in ("Infos","Cascade","Repos","Utils"):
        for p in glob.glob(os.path.join(SRCDIR, sub, "*.cpp")) + glob.glob(os.path.join(SRCDIR, sub, "*.h")):
            ff[os.path.basename(p)] = sub
    return ff

def report(lm):
    from collections import Counter
    c = Counter(lm.values())
    print("=== on-disk loose files:", len(lm), "===")
    for k,v in sorted(c.items(), key=lambda x:-x[1]): print(f"{v:4d}  {k}")
    bad = [f for f,b in lm.items() if b.startswith("UNMAPPED")]
    print("UNMAPPED:", bad if bad else "none")
    open(os.path.join(SRC,"Tools","movemap.tsv"),"w").write("\n".join(f"{f}\t{b}" for f,b in sorted(lm.items()))+"\n")

INC_RE = re.compile(r'(#\s*include\s*")([^"]+)(")')

def rewrite_includes(path, myfolder, ff):
    txt = open(path, encoding="utf-8", errors="replace").read()
    changed = [0]
    def repl(m):
        inc = m.group(2); base = os.path.basename(inc)
        folder = ff.get(base)
        if folder is None: return m.group(0)               # external/system/unknown -> leave
        if folder not in BUCKET_FOLDERS: return m.group(0)  # shared layer / root glue -> leave
        new = base if folder == myfolder else f"{folder}/{base}"
        if new != inc: changed[0]+=1
        return m.group(1)+new+m.group(3)
    txt = INC_RE.sub(repl, txt)
    if changed[0]:
        open(path,"w",encoding="utf-8",newline="").write(txt)
    return changed[0]

def apply():
    lm = loose_map()
    report(lm)
    if any(b.startswith("UNMAPPED") for b in lm.values()):
        print("ABORT: unmapped files remain"); sys.exit(1)
    ff = all_file_folders(lm)
    # 1) move loose files into bucket folders
    moved = 0
    for f, b in lm.items():
        if b == "(root)": continue
        dest_dir = os.path.join(SRCDIR, b)
        os.makedirs(dest_dir, exist_ok=True)
        src = os.path.join(SRCDIR, f); dst = os.path.join(dest_dir, f)
        if os.path.exists(src):
            os.replace(src, dst); moved += 1
    print(f"\nmoved {moved} files")
    # 2) rewrite bucket-targeting includes in ALL project files (at their post-move paths)
    total = 0; touched = 0
    for base, folder in ff.items():
        path = os.path.join(SRCDIR, folder, base) if folder else os.path.join(SRCDIR, base)
        if not os.path.exists(path): continue
        n = rewrite_includes(path, folder, ff)
        if n: touched += 1; total += n
    print(f"rewrote {total} includes across {touched} files")

if __name__ == "__main__":
    if "--apply" in sys.argv: apply()
    else: report(loose_map())
