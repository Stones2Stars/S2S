#!/usr/bin/env python
"""
census-python-boundary.py -- re-derive the Python <-> C++ boundary map.

WHY THIS EXISTS (and why it is a script rather than prose)
----------------------------------------------------------
docs/reference/python-read-map.md is a NEEDS census: it says what the one
data-fetching library must be SERVED, sized per consumer family. Its numbers are
measurements of a tree that is actively being cut, so they drift by nature -- and
a drifted census reads authoritative long after it is fiction (the exact failure
DEC-spec-plus-todo names). The doc used to state a method in prose and invite the
reader to re-derive it; nobody did, and it went on claiming a 2,109-name binding
surface long after that surface was cut to CyEnabler's availability reads.

So the method is executable. Re-run this, paste the tables, and the doc is current.

WHAT IT MEASURES -- BOTH DIRECTIONS OF THE BRIDGE
-------------------------------------------------
  A. PYTHON -> C++ (the CUT direction).  The published surface is every
     .def("<name>" under Sources/.  The read half is Sources/Python/CyEnabler.cpp;
     the rest are container/debug/util publishes carrying no entity data.
     Everything Python asks for that this does NOT answer is DEMAND on the
     library-to-be, bucketed by receiver into the read KINDs.

  B. C++ -> PYTHON (the KEPT direction).  The engine names Python functions in
     XML (<PythonCallback>, <PythonCanDo>, ...).  Each declared name is resolved
     against every def in Assets/Python, so a callback the engine can name but
     Python cannot answer shows up as a hole.
     (patterns.md: this direction is REQUIRED functionality and stays.)

TWO DISTORTIONS, BOTH ONE-DIRECTIONAL -- SO THE TOTALS ARE A FLOOR
------------------------------------------------------------------
  * A name Python also defines itself is dropped wholesale.  getText is the case
    that matters (BugUtil.py defines one), so its sites fall out of the demand
    tables entirely; the raw count is reported separately.
  * Receiver bucketing is heuristic, so the STATE/COMPUTED split moves at the
    margin.  The totals do not depend on it.

  ⛔ Neither distortion licenses a "this read is dead" call.  Reachability is NOT
  statically provable here (python-read-map.md 5.7): BUG resolves handlers from
  config strings, CvDomesticAdvisor eval()s engine method names out of a table,
  and the XML callbacks decide what actually runs.  A read found is a read to
  serve; a read NOT found is not evidence of absence.

USAGE
-----
  python Tools/census-python-boundary.py            # summary + markdown tables
  python Tools/census-python-boundary.py --demand   # + the full demand list
"""
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PYROOT = os.path.join(ROOT, "Assets", "Python")

RE_DEF = re.compile(r'\.def\s*\(\s*"([^"]+)"')
RE_PYDEF = re.compile(r'^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)')
RE_PYCALL = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(')
RE_PYTAG = re.compile(r'<(Python[A-Za-z]*)>\s*([^<\s][^<]*?)\s*</\1>')

# An engine-shaped name: the verbs the engine surface uses.
ENGINEISH = re.compile(r'^(get|is|can|set|change|calculate|find|AI_|do|create|init|has)')

# Receiver -> read KIND.  Order matters; first match wins.
TEXT_RECV = re.compile(r'(?i)(text|translator|trnsltr|gametext)')
INFO_RECV = re.compile(r'(?i)^(gc|GC)$|info$')
STATE_RECV = re.compile(
    r'(?i)^(p?(city|player|plot|unit|team|game|area|deal|group)'
    r'|cy(city|player|plot|unit|team|game|area)|ploop\w+|pactive\w+|pcapital)')
MUT_NAME = re.compile(r'^(set|change|do|create|init|kill|add|remove|apply)')
COMPUTED_NAME = re.compile(r'^(can|is|AI_|calculate|find|has)')

# <PythonName> names map/build display entries -- it is NOT a callback tag, so its
# names are not expected to resolve to a def.
NOT_A_CALLBACK_TAG = "PythonName"

PEDIA_DIRS = ("Screens/Pedia", "Screens/Sevopedia")
PEDIA_EXTRA = ("UnitUpgradesGraph.py",)


def read(path):
    for encoding in ("utf-8", "latin-1"):
        try:
            with open(path, "r", encoding=encoding) as handle:
                return handle.read()
        except (UnicodeDecodeError, OSError):
            continue
    return ""


def walk(base, suffixes):
    for current, dirs, names in os.walk(base):
        dirs[:] = [d for d in dirs if d not in (".vs", ".git")]
        for name in names:
            if name.lower().endswith(suffixes):
                yield os.path.join(current, name)


def published_surface():
    """Every .def name under Sources/, plus the read half broken out."""
    names = set()
    read_half = set()
    for path in walk(os.path.join(ROOT, "Sources"), (".cpp", ".h")):
        found = set(RE_DEF.findall(read(path)))
        if not found:
            continue
        names |= found
        if os.path.basename(path).startswith("CyEnabler"):
            read_half |= found
    return names, read_half


def python_defs():
    defined = set()
    for path in walk(PYROOT, (".py",)):
        for line in read(path).splitlines():
            match = RE_PYDEF.match(line)
            if match:
                defined.add(match.group(1))
    return defined


def classify(receiver, method):
    if TEXT_RECV.search(receiver) or method == "getText":
        return "TEXT"
    if MUT_NAME.match(method) and STATE_RECV.match(receiver):
        return "MUTATION"
    if INFO_RECV.search(receiver):
        return "INFO"
    if STATE_RECV.match(receiver):
        return "COMPUTED" if COMPUTED_NAME.match(method) else "STATE"
    return "other / UI"


def scan_demand(published, defined):
    per_dir = defaultdict(lambda: {"files": 0, "sites": 0})
    kinds = defaultdict(int)
    kind_names = defaultdict(set)
    pedia = defaultdict(int)
    names = set()
    sites = 0
    files = 0
    lines = 0
    called = set()
    demand = defaultdict(lambda: {"sites": 0, "recv": defaultdict(int)})

    for path in walk(PYROOT, (".py",)):
        rel_dir = os.path.relpath(os.path.dirname(path), PYROOT).replace("\\", "/")
        if rel_dir == ".":
            rel_dir = "(repo root)"
        base = os.path.basename(path)
        is_pedia = rel_dir.startswith(PEDIA_DIRS) or base in PEDIA_EXTRA
        files += 1
        per_dir[rel_dir]["files"] += 1
        for line in read(path).splitlines():
            lines += 1
            if line.strip().startswith("#"):
                continue
            for receiver, method in RE_PYCALL.findall(line.split("#", 1)[0]):
                if receiver == "self":
                    continue
                called.add(method)
                if method in published or method in defined:
                    continue
                if not ENGINEISH.match(method):
                    continue
                names.add(method)
                sites += 1
                per_dir[rel_dir]["sites"] += 1
                demand[method]["sites"] += 1
                demand[method]["recv"][receiver] += 1
                kind = classify(receiver, method)
                kinds[kind] += 1
                kind_names[kind].add(method)
                pedia["pedia" if is_pedia else "rest"] += 1
                if is_pedia:
                    pedia["kind:" + kind] += 1

    return dict(per_dir=per_dir, kinds=kinds, kind_names=kind_names, pedia=pedia,
                names=names, sites=sites, files=files, lines=lines,
                called=called, demand=demand)


def scan_callbacks(defined):
    declarations = defaultdict(int)
    names = defaultdict(set)
    for path in walk(os.path.join(ROOT, "Assets"), (".xml",)):
        for tag, value in RE_PYTAG.findall(read(path)):
            value = value.strip()
            if value and " " not in value:
                declarations[tag] += 1
                names[tag].add(value)
    callback_names = set()
    for tag, values in names.items():
        if tag != NOT_A_CALLBACK_TAG:
            callback_names |= values
    hosts = defaultdict(int)
    for path in walk(PYROOT, (".py",)):
        rel = os.path.relpath(path, PYROOT).replace("\\", "/")
        for line in read(path).splitlines():
            match = RE_PYDEF.match(line)
            if match and match.group(1) in callback_names:
                hosts[rel] += 1
    unresolved = sorted(n for n in callback_names if n not in defined)
    return declarations, names, callback_names, hosts, unresolved


def main():
    published, read_half = published_surface()
    defined = python_defs()
    demand = scan_demand(published, defined)
    declarations, cb_names, callback_names, hosts, unresolved = scan_callbacks(defined)

    raw_gettext = 0
    for path in walk(PYROOT, (".py",)):
        raw_gettext += len(re.findall(r'\.getText\s*\(', read(path)))

    print("=" * 74)
    print("DIRECTION A -- PYTHON -> C++  (the CUT direction)")
    print("=" * 74)
    print("published .def names under Sources/ : %d" % len(published))
    print("  of which the READ half (CyEnabler): %d" % len(read_half))
    print()
    print("| Measure | Value |")
    print("|---|---|")
    print("| Python files | **%d** |" % demand["files"])
    print("| Lines | **%s** |" % format(demand["lines"], ","))
    print("| Distinct methods called on any receiver | %s |" % format(len(demand["called"]), ","))
    print("| Published `.def` names (read half -- `CyEnabler`) | **%d** |" % len(read_half))
    print("| **UNSERVED engine-shaped reads** | **%s names / %s call sites** |"
          % (format(len(demand["names"]), ","), format(demand["sites"], ",")))
    print()

    print("--- by directory ---")
    print("| Directory | Files | Unserved sites |")
    print("|---|--:|--:|")
    for name, data in sorted(demand["per_dir"].items(), key=lambda kv: -kv[1]["sites"]):
        if data["sites"]:
            print("| `%s` | %d | %d |" % (name, data["files"], data["sites"]))
    print()

    print("--- by read KIND ---")
    print("| Kind | Sites | Distinct names |")
    print("|---|--:|--:|")
    for kind, count in sorted(demand["kinds"].items(), key=lambda kv: -kv[1]):
        print("| **%s** | %s | %d |" % (kind, format(count, ","), len(demand["kind_names"][kind])))
    print()
    kind_site_total = sum(demand["kinds"].values())
    kind_name_total = sum(len(v) for v in demand["kind_names"].values())
    print("sites sum to %s (== the unserved total).  Distinct names do NOT sum"
          % format(kind_site_total, ","))
    print("(%d > %d): one name on two receiver kinds counts in both."
          % (kind_name_total, len(demand["names"])))
    print()
    print("NOTE  The TEXT row is a RESIDUE, not the plane: getText is Python-defined,")
    print("      so all %s of its call sites drop out by the rule in 1.1." % format(raw_gettext, ","))
    print()

    pedia_total = demand["pedia"]["pedia"]
    print("--- pedia vs rest ---")
    print("pedia slice: %s sites  (rest: %s)" % (format(pedia_total, ","), format(demand["pedia"]["rest"], ",")))
    for kind in ("INFO", "STATE", "COMPUTED", "MUTATION", "other / UI"):
        print("   pedia %-12s %d" % (kind, demand["pedia"].get("kind:" + kind, 0)))
    print()

    print("=" * 74)
    print("DIRECTION B -- C++ -> PYTHON  (the KEPT direction)")
    print("=" * 74)
    print("| Tag | Declarations | Distinct names |")
    print("|---|--:|--:|")
    for tag in sorted(declarations, key=lambda t: -declarations[t]):
        if tag == NOT_A_CALLBACK_TAG:
            continue
        print("| `<%s>` | %d | %d |" % (tag, declarations[tag], len(cb_names[tag])))
    print("| **Total** | **%s** | **%d** |"
          % (format(sum(declarations[t] for t in declarations if t != NOT_A_CALLBACK_TAG), ","),
             len(callback_names)))
    print()
    print("resolved by a Python def : %d / %d" % (len(callback_names) - len(unresolved), len(callback_names)))
    if unresolved:
        print("UNRESOLVED (engine can name it, Python cannot answer):")
        for name in unresolved[:20]:
            print("   %s" % name)
    print()
    print("hosts:")
    for host, count in sorted(hosts.items(), key=lambda kv: -kv[1])[:5]:
        print("   %-45s %d" % (host, count))
    print()
    print("`<%s>` (%d declarations / %d names) is NOT a callback tag -- it names"
          % (NOT_A_CALLBACK_TAG, declarations.get(NOT_A_CALLBACK_TAG, 0),
             len(cb_names.get(NOT_A_CALLBACK_TAG, ()))))
    print("map/build display entries, so it is excluded and does not resolve to a def.")

    if "--demand" in sys.argv:
        print()
        print("=" * 74)
        print("FULL DEMAND LIST  (name / sites / top receivers)")
        print("=" * 74)
        for name, data in sorted(demand["demand"].items(), key=lambda kv: -kv[1]["sites"]):
            top = sorted(data["recv"].items(), key=lambda kv: -kv[1])[:3]
            print("%-44s %5d  %s" % (name, data["sites"],
                                     ", ".join("%s(%d)" % (r, c) for r, c in top)))


main()
