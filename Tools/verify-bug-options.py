#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify-bug-options -- a BUG option the DLL reads must be reachable, and the logging
surface may only shrink.

Three checks, and each exists because the failure is SILENT. A BUG option is addressed by
string from C++ (`getBugOptionINT("Autolog__LogLevelStream", 1)`) and declared in XML far
away (`Assets/Config/*.xml`). Nothing connects the two: no compiler, no loader, no assert.
So a knob can be half-wired in either direction and the game runs, reporting nothing.

  1. READ BUT NOT DECLARED  (fails)
     `getBugOption*` on an id no `Assets/Config` file declares. The lookup then always
     answers the DEFAULT passed at the call site -- the knob is pinned, unreachable from
     the options screen AND from the .ini, and it looks exactly like a working option.
     Worked case: `Autolog__LogLevelStream` was read as `gStreamLogLevel` and declared
     nowhere, so the /events stream verbosity was welded to 1. It could not be turned up
     precisely when someone went looking for it.

  2. A DECLARED LOG LEVEL MUST BE READ  (fails)
     The mirror. A `LogLevel*` option nobody reads renders a dropdown that promises
     control it does not have -- you set "unit logging" to 4, nothing happens, and there
     is no error to chase. Worked case: LogLevelTeamBBAI / LogLevelCityBBAI /
     LogLevelUnitBBAI were all settable and all read by nothing, because there is ONE
     number: the DLL reads LogLevelPlayerBBAI and drives every scope global from it.
     Scoped to the log-level family on purpose -- most BUG options are read by Python,
     not the DLL, so a general declared-but-unread check would be noise.

  3. THE LEGACY LOGGING SURFACE IS A RATCHET  (fails only on a RISE)
     Direct `gDLL->logMsg` call sites, the scope-named globals, and the BBAI helpers are
     legacy. They CANNOT be removed yet -- the BBAI logging is still in place and
     replacing it is its own piece of work -- so this does not demand their removal. It
     only refuses GROWTH: today's counts are the ceiling. That is the whole anti-
     rollerskate mechanism, because the pull is never "revive the legacy logger", it is
     "add one more line next to the twenty already here".
     ⛔ A NEW gate reads the UNIVERSAL level; a new emit goes through the event spine
     (docs/spine.md). If a count below rises, that is the thing to fix -- never the
     baseline. A count that FALLS should be committed with the baseline lowered.

Run from the repo root:  python Tools/verify-bug-options.py
"""

import io
import os
import re
import sys


#	The legacy logging ceiling. Lower these when the surface shrinks; never raise them.
LEGACY_LOGGING_CEILING = {
    "gDLL->logMsg call sites": 71,
    "gTeamLogLevel reads": 11,
    "gCityLogLevel reads": 10,
    "gUnitLogLevel reads": 33,
    "logBBAI call sites": 14,
}

SOURCE_ROOT = "Sources"
CONFIG_ROOT = os.path.join("Assets", "Config")


def read_text(path):
    return io.open(path, encoding="utf-8", errors="ignore").read()


def source_files():
    for dirpath, dirnames, filenames in os.walk(SOURCE_ROOT):
        if "SourceArchive" in dirpath:
            continue
        for name in filenames:
            if name.endswith(".cpp") or name.endswith(".h"):
                yield os.path.join(dirpath, name)


def collect_reads():
    """Every BUG option addressed from C++, mapped to the file that reads it."""
    reads = {}
    pattern = re.compile(r'getBugOption(?:INT|BOOL|STRING|FLOAT)?\s*\(\s*"([^"]+)"')
    for path in source_files():
        text = read_text(path)
        for match in pattern.finditer(text):
            reads.setdefault(match.group(1), os.path.basename(path))
    return reads


def collect_declarations():
    """Every option/list declared in Assets/Config, keyed as <optionsId>__<id>.

    The prefix comes from the enclosing `<options id="...">` element, NOT the filename --
    "BULL City Bar.xml" declares the CityBar family, so deriving it from the file name
    reports almost every option as missing.
    """
    declared = {}
    block = re.compile(r'<options\b[^>]*\bid="([^"]+)"(.*?)</options>', re.S)
    entry = re.compile(r'<(?:option|list)\b[^>]*\bid="([^"]+)"[^>]*?(/?)>', re.S)
    if not os.path.isdir(CONFIG_ROOT):
        return declared
    for name in sorted(os.listdir(CONFIG_ROOT)):
        if not name.endswith(".xml"):
            continue
        text = read_text(os.path.join(CONFIG_ROOT, name))
        for options_block in block.finditer(text):
            prefix = options_block.group(1)
            body = options_block.group(2)
            for option in entry.finditer(body):
                option_id = option.group(1)
                tail = body[option.end():option.end() + 400]
                getter = re.search(r'\bget="([^"]+)"', option.group(0))
                declared[prefix + "__" + option_id] = {
                    "file": name,
                    "id": option_id,
                    #	An option is Python-readable through its GETTER: the explicit get="..." when one is
                    #	declared, otherwise the auto-derived get<Id>. A <change> handler is a consumer too.
                    "getters": [getter.group(1)] if getter else ["get" + option_id],
                    "has_change": option.group(2) != "/" and "<change" in tail.split("</")[0],
                }
    return declared


def python_text():
    """Every Python source under Assets, concatenated -- the consumer side of a BUG option."""
    chunks = []
    for root in (os.path.join("Assets", "Python"),):
        for dirpath, dirnames, filenames in os.walk(root):
            for name in filenames:
                if name.endswith(".py"):
                    chunks.append(read_text(os.path.join(dirpath, name)))
    return "\n".join(chunks)


def count_occurrences(needle, word_boundary):
    total = 0
    pattern = re.compile(r"\b" + re.escape(needle) + r"\b" if word_boundary else re.escape(needle))
    for path in source_files():
        total += len(pattern.findall(read_text(path)))
    return total


def main():
    reads = collect_reads()
    declared = collect_declarations()
    failures = 0

    python_source = python_text()

    # 1 -- read but not declared: the knob is pinned at its call-site default.
    unreachable = sorted(name for name in reads if name not in declared)
    for name in unreachable:
        print("UNREACHABLE  %-42s read in %s but declared in no Assets/Config file"
              % (name, reads[name]))
    if unreachable:
        print("")
        print("A read with no declaration always answers the default passed at the call")
        print("site, so the option cannot be changed from the options screen OR from the")
        print(".ini -- and it looks like a working knob. Declare it, or drop the read.")
        print("")
        failures += len(unreachable)

    # 2 -- a declared log level nobody consumes promises control that does not exist.
    #	⚠ A reference from an options TAB does not count: the three dead dropdowns this check exists for were
    #	all rendered by BugAutologOptionsTab and read by nothing, which is exactly the defect.
    dead_levels = []
    for name in sorted(declared):
        if "LogLevel" not in name:
            continue
        info = declared[name]
        if name in reads or info["has_change"]:
            continue
        if any(re.search(r"\b" + re.escape(g) + r"\b", python_source) for g in info["getters"]):
            continue
        dead_levels.append(name)
    for name in dead_levels:
        print("DEAD KNOB    %-42s declared in %s, read by no DLL code and no Python getter"
              % (name, declared[name]["file"]))
    if dead_levels:
        print("")
        print("A settable log level the DLL never reads renders a dropdown that does")
        print("nothing. There is ONE number: the DLL drives every scope global from the")
        print("universal level, so the legacy BBAI sites mimic it. Drop the declaration")
        print("rather than adding a read -- a new gate reads the universal level.")
        print("")
        failures += len(dead_levels)

    # 3 -- the legacy logging surface may only shrink.
    measured = {
        "gDLL->logMsg call sites": count_occurrences("gDLL->logMsg(", False),
        "gTeamLogLevel reads": count_occurrences("gTeamLogLevel", True),
        "gCityLogLevel reads": count_occurrences("gCityLogLevel", True),
        "gUnitLogLevel reads": count_occurrences("gUnitLogLevel", True),
        "logBBAI call sites": count_occurrences("logBBAI", True),
    }
    risen = []
    fallen = []
    for label in sorted(LEGACY_LOGGING_CEILING):
        ceiling = LEGACY_LOGGING_CEILING[label]
        now = measured[label]
        if now > ceiling:
            risen.append((label, ceiling, now))
        elif now < ceiling:
            fallen.append((label, ceiling, now))
    for label, ceiling, now in risen:
        print("LEGACY GREW  %-42s %d -> %d" % (label, ceiling, now))
    if risen:
        print("")
        print("The legacy logging surface is a RATCHET: it cannot be removed yet (the")
        print("BBAI logging is still in place), so this only refuses GROWTH. A new gate")
        print("reads the UNIVERSAL level and a new emit goes through the event spine")
        print("(docs/spine.md) -- do not raise the ceiling to make this pass.")
        print("")
        failures += len(risen)

    for label, ceiling, now in fallen:
        print("shrank       %-42s %d -> %d  (lower the ceiling in this file)"
              % (label, ceiling, now))

    print("bug options: %d read from C++, %d declared across Assets/Config" % (len(reads), len(declared)))
    print("legacy logging: " + ", ".join(
        "%s=%d" % (label.split()[0], measured[label]) for label in sorted(measured)))

    if failures:
        return 1
    print("bug options clean -- every read is reachable, every log level is live, and the")
    print("legacy logging surface has not grown.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
