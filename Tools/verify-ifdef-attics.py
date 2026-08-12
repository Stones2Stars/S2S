#!/usr/bin/env python
"""verify-ifdef-attics.py -- find `#ifdef` blocks NOTHING can ever switch on.

⛔ WHAT IS WRONG IS *WHAT IS BEHIND THE GUARD*, NOT THE GUARD (owner). Some `#ifdef`s
are useful and stay. What is wrong is hiding **CACHING** or **GAME MECHANICS** behind
one instead of expressing them properly:

  CACHING behind a guard      -> wrong. A cache is either the design or it is not; a
                                 switchable one means nobody decided.
  A GAME MECHANIC behind one  -> wrong. That is a GAMEOPTION_* -- the entity-level
                                 enabled/disabled gate ([DEC-entity-gate]) -- evaluated
                                 live, visible to the player, and authored in data.
  DIAGNOSTICS / TOOLING       -> LEGITIMATE. `MINIDUMP`, `MEMTRACK` and their kin stay.
  A DELIBERATE OFF-SWITCH     -> LEGITIMATE, and the reason belongs in the subsystem's
                                 reference doc. `THE_GREAT_WALL` is off because
                                 rendering it has caused CTDs; a sweep that eats it
                                 re-introduces a crash nobody remembers.

⇒ NONE of that is decidable from the preprocessor. So this tool deliberately fails on
ONE thing only, the single mechanical verdict available:

  a guard with NO `#define` ANYWHERE -- not in Sources/, not in fbuild.bff, not even a
  commented-out one -- can never be turned on by anybody. It is an abandoned alternate
  parked beside the live code, which is what version control is for, and it costs
  something real: it holds the NAMES of removed things, so the next agent finds them and
  re-treads what was killed, and being preprocessor-skipped it is invisible to the
  compiler census that would otherwise name it.

Everything else is REPORTED for a human verdict and never failed on.

⛔ TWO TRAPS THIS TOOL EXISTS TO STOP, both measured:

 1. **A `#define` in Sources/ is TU-LOCAL.** It holds only where that definition is
    VISIBLE, so the SAME guard is ON in some translation units and OFF in others and
    there is no single arm to keep. A blanket collapse turned the save wrapper's
    `DEBUG_TRACE` from `;` into a live `OutputDebugString` on every tagged read --
    `DETAILED_TRACE` is defined in `CvGameCoreDLL.cpp`, which the wrapper never
    includes. Load time tripled and the game crashed at `eip=0`.
 2. **A guard defined by EVERY config cannot vary**, so its `#else` half has never
    compiled and the live arm reads as one deliberate mode of a switch that nobody
    audits. (`_MOD_FRACTRADE` hid a scale reduce inside an aggregation for fifteen
    years.)

Exit 0 = no abandoned alternates. Exit 1 = at least one.

Usage:  python Tools/verify-ifdef-attics.py [--quiet]
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = os.path.join(REPO, "Sources")
BFF = os.path.join(SOURCES, "fbuild.bff")

# Guards defined by somebody who is not us. Absence from Sources/ + fbuild.bff is their
# NORMAL state and says nothing -- read the test as "defined by NOBODY", not "by not-us".
EXTERNAL = set("""
__INTELLISENSE__ __cplusplus _MSC_VER _M_IX86 _M_X64 _WIN32 _WIN64 WIN32 WIN64
_WINDOWS _DEBUG NDEBUG APSTUDIO_INVOKED APSTUDIO_READONLY_SYMBOLS RC_INVOKED
_AFXDLL __GNUC__ __clang__ _INC_WINDOWS _CONSOLE _USRDLL _LIB __FILE__ __LINE__
_M_AMD64 _M_ARM __BORLANDC__ __WATCOMC__ __MWERKS__ _MT _DLL
""".split())

SKIP_DIRS = ("include", "lib", ".vs", ".vscode", "nbproject")
# Vendored third-party FILES inside our tree: their guards belong to their upstream author.
SKIP_FILES = ("StackWalker.h", "StackWalker.cpp")

GUARD_RE = re.compile(r"^\s*#\s*(ifdef|ifndef)\s+([A-Za-z_][A-Za-z0-9_]*)")
DEFINED_RE = re.compile(r"^\s*#\s*(?:if|elif)\b.*?\bdefined\s*\(?\s*([A-Za-z_][A-Za-z0-9_]*)")
DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)")
COMMENTED_DEFINE_RE = re.compile(r"^\s*(?://|/\*)\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)")


def source_files():
    for root, dirs, files in os.walk(SOURCES):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for name in files:
            if name in SKIP_FILES:
                continue
            if name.endswith((".cpp", ".h", ".hpp", ".inl")):
                yield os.path.join(root, name)


def read(path):
    handle = open(path, "rb")
    try:
        return handle.read().decode("utf-8", "replace").splitlines()
    finally:
        handle.close()


def bff_defines():
    """(always, sometimes) guard names from fbuild.bff.

    `/DNAME` in .CommonDefines / .C2CDefines lands in every config; one in a per-config
    block varies. Comment lines are skipped -- the bff carries whole historical command
    lines in comments, and counting those reports a retired define as live.
    """
    always, sometimes = set(), set()
    if not os.path.exists(BFF):
        return always, sometimes
    bucket = None
    for line in read(BFF):
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        header = re.match(r"^\.(\w+)\s*=", stripped)
        if header:
            bucket = always if header.group(1) in ("CommonDefines", "C2CDefines") else sometimes
        for match in re.finditer(r"/D([A-Za-z_][A-Za-z0-9_]*)", stripped):
            (bucket if bucket is not None else sometimes).add(match.group(1))
    return always, sometimes


def main():
    quiet = "--quiet" in sys.argv

    source_defined = {}      # guard -> Sources/ files that #define it (TU-locality)
    commented = set()
    used = {}

    for path in source_files():
        rel = os.path.relpath(path, REPO).replace("\\", "/")
        lines = read(path)
        for index, line in enumerate(lines):
            match = DEFINE_RE.match(line)
            if match:
                source_defined.setdefault(match.group(1), set()).add(rel)
            match = COMMENTED_DEFINE_RE.match(line)
            if match:
                commented.add(match.group(1))

            guard = None
            match = GUARD_RE.match(line)
            if match:
                guard = match.group(2)
                # An include guard is `#ifndef X` immediately followed by `#define X`.
                if match.group(1) == "ifndef":
                    following = lines[index + 1] if index + 1 < len(lines) else ""
                    nxt = DEFINE_RE.match(following)
                    if nxt and nxt.group(1) == guard:
                        guard = None
            else:
                match = DEFINED_RE.match(line)
                if match:
                    guard = match.group(1)
            if guard:
                used.setdefault(guard, []).append((rel, index + 1))

    always, sometimes = bff_defines()

    abandoned, tu_local, unconditional, switches = [], [], [], []
    for guard in sorted(used):
        if guard in EXTERNAL or guard.startswith("__"):
            continue
        if guard in sometimes and guard not in always:
            continue                                   # a real build-config guard: it VARIES
        if guard in source_defined:
            tu_local.append((guard, used[guard], sorted(source_defined[guard])))
        elif guard in always:
            unconditional.append((guard, used[guard]))
        elif guard in commented:
            switches.append((guard, used[guard]))
        else:
            abandoned.append((guard, used[guard]))

    def show(title, rows, note):
        if not rows or quiet:
            return
        print(title)
        for row in rows:
            guard, sites = row[0], row[1]
            extra = ("  (#define'd in %s)" % ", ".join(row[2])) if len(row) > 2 else ""
            print("  %-38s %d site(s)%s" % (guard, len(sites), extra))
        print("  %s" % note)
        print("")

    show("OFF-SWITCHES (a commented-out #define exists) -- REVIEW, do not sweep:", switches,
         "Legitimate if it guards DIAGNOSTICS or is a deliberate off-switch; wrong if it hides\n"
         "  CACHING or a GAME MECHANIC (that is a GAMEOPTION_*). Record WHY it is off in the\n"
         "  subsystem's reference doc -- that reason is all that protects it from the next sweep.")
    show("TU-LOCAL (#define'd in a Sources/ file) -- NEVER collapse mechanically:", tu_local,
         "The define holds only where it is VISIBLE, so this guard is ON in some translation\n"
         "  units and OFF in others and there is no single arm to keep. Resolve per SITE.\n"
         "  (No include-graph analysis here -- a cross-file site is 'check', never 'OFF'.)")
    show("UNCONDITIONAL (defined by every config) -- the #else half has never compiled:", unconditional,
         "The guard cannot vary, so the live arm is plain code wearing a switch. Collapsing is\n"
         "  safe; then RE-READ the survivor, which nobody has audited as ordinary code.")

    if not abandoned:
        if not quiet:
            print("verify-ifdef-attics: clean -- no abandoned alternates (%d guards checked)" % len(used))
        return 0

    print("ABANDONED ALTERNATES -- no #define ANYWHERE, not even commented. Nobody can ever")
    print("switch these on, so they are dead code. Delete the block; git is the archive:")
    for guard, sites in abandoned:
        print("  %s" % guard)
        for rel, number in sites:
            print("      %s:%d" % (rel, number))
    print("")
    print("%d abandoned guard(s). See AGENTS.md Conventions §Design." % len(abandoned))
    return 1


if __name__ == "__main__":
    sys.exit(main())
