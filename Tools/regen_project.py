#!/usr/bin/env python3
r"""Regenerate the IDE project to match the on-disk structure + rename C2C(VS2019) -> S2S.

The .vcxproj/.sln/.filters are DEAD-for-build (fbuild.bff is truth) -- IDE-display only. This
realigns them to the post-move disk layout so the IDE stops showing files as missing.
  - S2S.vcxproj: update every <Cl* Include="Name"> to "Folder\Name" (backslash) per disk.
  - S2S.vcxproj.filters: regenerate -- each item Include="Folder\Name" + <Filter>Folder</Filter>
    (root files: no filter), plus a <Filter Include="Folder"> def per bucket.
  - S2S.sln: rename + repoint the project reference.
Run from Sources/. Deterministic UUIDs (uuid5) so reruns are stable.
"""
import os, re, sys, uuid

SRCDIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "Sources")
SKIP = {"include","lib","nbproject",".vs",".vscode"}
OLD_VCX = os.path.join(SRCDIR, "C2C (VS2019).vcxproj")
OLD_FLT = os.path.join(SRCDIR, "C2C (VS2019).vcxproj.filters")
OLD_SLN = os.path.join(SRCDIR, "C2C (VS2019).sln")
NEW_VCX = os.path.join(SRCDIR, "S2S.vcxproj")
NEW_FLT = os.path.join(SRCDIR, "S2S.vcxproj.filters")
NEW_SLN = os.path.join(SRCDIR, "S2S.sln")
NS = uuid.UUID("12345678-1234-5678-1234-567812345678")

def scan():
    folder = {}  # basename -> folder ("" for root), backslash form
    for root, dirs, names in os.walk(SRCDIR):
        dirs[:] = [d for d in dirs if d not in SKIP]
        rel = os.path.relpath(root, SRCDIR)
        rel = "" if rel == "." else rel.replace(os.sep, "\\")
        for n in names:
            if n.endswith(".cpp") or n.endswith(".h"):
                folder[n] = rel
    return folder

def newpath(basename, folder):
    f = folder.get(basename)
    if f is None: return None
    return basename if f == "" else f + "\\" + basename

def fix_vcxproj(folder):
    txt = open(OLD_VCX, encoding="utf-8", errors="replace").read()
    def repl(m):
        attr, inc = m.group(1), m.group(2)
        base = inc.split("\\")[-1].split("/")[-1]
        np = newpath(base, folder)
        return f'{attr} Include="{np}"' if np else m.group(0)
    txt = re.sub(r'(<Cl(?:Compile|Include))\s+Include="([^"]+)"', repl, txt)
    open(NEW_VCX, "w", encoding="utf-8", newline="").write(txt)

def fix_filters(folder):
    txt = open(OLD_FLT, encoding="utf-8", errors="replace").read()
    head = txt.split("<ItemGroup>")[0]   # xml decl + <Project ...>
    # collect every Cl* item by basename
    items = []  # (tag, basename)
    for m in re.finditer(r'<(Cl(?:Compile|Include))\s+Include="([^"]+)"', txt):
        tag = m.group(1); base = m.group(2).split("\\")[-1].split("/")[-1]
        if base in folder: items.append((tag, base))
    buckets = sorted({folder[b] for _, b in items if folder[b] != ""})
    out = [head.rstrip("\n")]
    out.append("  <ItemGroup>")
    for bk in buckets:
        guid = str(uuid.uuid5(NS, bk)).upper()
        out.append(f'    <Filter Include="{bk}">')
        out.append(f"      <UniqueIdentifier>{{{guid}}}</UniqueIdentifier>")
        out.append("    </Filter>")
    out.append("  </ItemGroup>")
    for want in ("ClCompile", "ClInclude"):
        grp = [(t, b) for t, b in items if t == want]
        if not grp: continue
        out.append("  <ItemGroup>")
        for t, b in sorted(grp, key=lambda x: x[1]):
            f = folder[b]; inc = newpath(b, folder)
            if f == "":
                out.append(f'    <{t} Include="{inc}" />')
            else:
                out.append(f'    <{t} Include="{inc}">')
                out.append(f"      <Filter>{f}</Filter>")
                out.append(f"    </{t}>")
        out.append("  </ItemGroup>")
    out.append("</Project>")
    open(NEW_FLT, "w", encoding="utf-8", newline="").write("\n".join(out) + "\n")

def fix_sln():
    if not os.path.exists(OLD_SLN): return
    txt = open(OLD_SLN, encoding="utf-8", errors="replace").read()
    txt = txt.replace('"C2C (VS2019).vcxproj"', '"S2S.vcxproj"').replace('= "C2C",', '= "S2S",')
    open(NEW_SLN, "w", encoding="utf-8", newline="").write(txt)

def main():
    folder = scan()
    fix_vcxproj(folder)
    fix_filters(folder)
    fix_sln()
    print(f"wrote S2S.vcxproj / S2S.vcxproj.filters / S2S.sln ({len(folder)} files mapped)")
    print("buckets:", sorted({v for v in folder.values() if v}))

if __name__ == "__main__":
    main()
