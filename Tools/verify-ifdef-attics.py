#!/usr/bin/env python
"""verify-ifdef-attics.py -- FEATURE `#ifdef`s are banned outright (owner).

  "We will never use them in the code, they are an anachronism from when source
   control was not really a thing."  /  "ifdef sections come from a time when people
   did not understand git."

So there is no off-switch population to curate and no attic to triage: a block parked
behind a feature guard is a block git should be holding instead. Deleting one loses
nothing -- `git log` is the archive -- while keeping one costs something real, because
it holds the NAMES of removed things and is invisible to the compiler census that
would otherwise name them.

THE ONE LEGITIMATE GUARD IS A BUILD-CONFIGURATION ONE -- a symbol that genuinely
VARIES between configs (`FASSERT_ENABLE` is Assert/Debug/Testing only; `_DEBUG`,
`NDEBUG`, `FINAL_RELEASE`), plus the platform/toolchain predefines somebody else
defines. Everything else fails:

  defined by NO config, no `#define` anywhere  -> a parked block. DELETE it.
  defined by EVERY config (never omitted)      -> the `#else` half has never compiled,
                                                  and the live arm reads as one mode of
                                                  a switch, so nobody audits it. Collapse
                                                  the guard and re-read the survivor as
                                                  the plain code it is.

Exit 0 = clean. Exit 1 = at least one banned guard.

Usage:  python Tools/verify-ifdef-attics.py [--quiet]
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = os.path.join(REPO, "Sources")
BFF = os.path.join(SOURCES, "fbuild.bff")

# Guards defined by somebody who is not us. Absence from Sources/ + fbuild.bff is
# their NORMAL state and says nothing about them -- read the test as "defined by
# NOBODY", never "defined by not-us".
EXTERNAL = set("""
__INTELLISENSE__ __cplusplus _MSC_VER _M_IX86 _M_X64 _WIN32 _WIN64 WIN32 WIN64
_WINDOWS _DEBUG NDEBUG APSTUDIO_INVOKED APSTUDIO_READONLY_SYMBOLS RC_INVOKED
_AFXDLL __GNUC__ __clang__ _INC_WINDOWS _CONSOLE _USRDLL _LIB __FILE__ __LINE__
_M_AMD64 _M_ARM __BORLANDC__ __WATCOMC__ __MWERKS__ _MT _DLL
""".split())

# Vendored third-party trees: their guards are their own business.
SKIP_DIRS = ("include", "lib", ".vs", ".vscode", "nbproject")

# Vendored third-party FILES sitting inside our own tree. Their guards belong to
# their upstream author, so absence from Sources/ says nothing about them.
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
    """Return (always, sometimes) guard-name sets from fbuild.bff.

    A `/DNAME` inside .CommonDefines or .C2CDefines lands in every config; one inside
    a per-config block varies. Comment lines are skipped -- the bff carries whole
    historical command lines in comments, and counting those would report a retired
    define as live.
    """
    always = set()
    sometimes = set()
    if not os.path.exists(BFF):
        return always, sometimes
    bucket = None
    for line in read(BFF):
        stripped = line.strip()
        if stripped.startswith("//"):
            continue
        header = re.match(r"^\.(\w+)\s*=", stripped)
        if header:
            name = header.group(1)
            if name in ("CommonDefines", "C2CDefines"):
                bucket = always
            elif name.startswith("Config") or name.startswith("C2CDefines"):
                bucket = sometimes
            else:
                bucket = sometimes
        for match in re.finditer(r"/D([A-Za-z_][A-Za-z0-9_]*)", stripped):
            (bucket if bucket is not None else sometimes).add(match.group(1))
    return always, sometimes


def main():
    quiet = "--quiet" in sys.argv

    defined = set()
    source_defined = {}          # guard -> the Sources/ files that #define it (TU-locality)
    commented = set()
    used = {}

    for path in source_files():
        rel = os.path.relpath(path, REPO).replace("\\", "/")
        lines = read(path)
        for index, line in enumerate(lines):
            number = index + 1
            match = DEFINE_RE.match(line)
            if match:
                defined.add(match.group(1))
                source_defined.setdefault(match.group(1), set()).add(rel)
            match = COMMENTED_DEFINE_RE.match(line)
            if match:
                commented.add(match.group(1))

            guard = None
            match = GUARD_RE.match(line)
            if match:
                guard = match.group(2)
                # An include guard is `#ifndef X` immediately followed by `#define X`.
                # It is not a switch and never an attic.
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
                used.setdefault(guard, []).append((rel, number))

    always, sometimes = bff_defines()
    defined |= always | sometimes

    banned = []
    unsafe = []
    for guard in sorted(used):
        if guard in EXTERNAL or guard.startswith("__"):
            continue
        if guard in sometimes and guard not in always:
            continue  # a genuine build-configuration guard: it VARIES
        if guard in source_defined:
            # ⛔ TU-LOCAL. A `#define` in Sources/ is in effect only where that definition is VISIBLE, so this
            # guard is ON in some translation units and OFF in others. Collapsing it needs a per-TU verdict.
            unsafe.append((guard, used[guard], sorted(source_defined[guard])))
        elif guard in always:
            banned.append((guard, used[guard], "defined by EVERY config -- the #else half has never compiled"))
        elif guard in commented:
            banned.append((guard, used[guard], "a commented-out #define -- a parked feature; git is the archive"))
        else:
            banned.append((guard, used[guard], "defined NOWHERE -- the block never compiles"))

    if unsafe:
        print("TU-LOCAL FEATURE GUARDS -- banned, but NEVER collapse these mechanically:")
        for guard, sites, wheres in unsafe:
            print("  %s  (#define'd in %s)" % (guard, ", ".join(wheres)))
            for rel, number in sites:
                same = any(rel == w for w in wheres)
                # NOT an include-graph analysis: a site in another file MAY still see the define through a
                # header chain. Flagged as "check", never asserted as OFF.
                print("      %s:%d%s" % (rel, number, "" if same else "   <-- other TU: check whether it includes the definer"))
        print("")
        print("  A `#define` in Sources/ holds only where it is VISIBLE. A use site that does not see it")
        print("  compiles the OTHER arm -- so one guard is simultaneously ON in some TUs and OFF in others,")
        print("  and there is no single arm to keep. Resolve each SITE against what that TU actually sees.")
        print("  Measured: a blanket collapse turned the save wrapper's DEBUG_TRACE from `;` into a live")
        print("  OutputDebugString on every tagged read -- the define lives in CvGameCoreDLL.cpp, which the")
        print("  wrapper never sees. Load time tripled and the game crashed.")
        print("")

    if not banned and not unsafe:
        if not quiet:
            print("verify-ifdef-attics: clean (%d guards checked)" % len(used))
        return 0

    if not banned:
        return 1
    print("BANNED FEATURE GUARDS -- delete the block; git is the archive:")
    for guard, sites, why in banned:
        print("  %s -- %s" % (guard, why))
        for rel, number in sites:
            print("      %s:%d" % (rel, number))
    print("")
    print("%d banned guard(s). Only a guard that VARIES BY BUILD CONFIG is legitimate." % len(banned))
    return 1


if __name__ == "__main__":
    sys.exit(main())
