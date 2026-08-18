#!/usr/bin/env python
"""Census the whole-registry scans the maintained frontier exists to delete.

The enabler's central efficiency claim ([docs/specs/enabler.md] SS6/SS7) is that AI evaluation
iterates a small MAINTAINED set of currently-valid choices instead of asking the entity
database a question the maintained state already answers. Every surviving
`for (i = 0; i < GC.getNum<X>Infos(); i++)` in a decision path is that claim being violated.

⛔ The compiler can never name one of these -- deleting nothing, they are perfectly legal code --
so the todo says this class "closes only by being searched for". This IS the searcher, so that
the search is repeatable rather than a snapshot somebody has to redo.

TWO POPULATIONS, DIFFERENT FIXES, deliberately reported apart:
  ENABLER-DOMAIN (units/buildings/techs/civics/projects/processes/promotions/builds) re-points
    onto the maintained FRONTIER.
  OTHER-REGISTRY (unitcombats/specialists/terrains/features/bonuses/religions/properties/...)
    is the OWN-DATA INVERSION and re-points onto the entity's own compiled entries.

⚖ ADVISORY, never a hard failure, and the reason is honest: a loop's REGISTRY is mechanical but
its CONTEXT is not. Init, reset, serialization, save/load, UI enumeration and text rendering all
legitimately walk a whole registry, and no regex can tell those from a decision path. So this
reports a census for a human verdict -- exactly as the #ifdef attic checker does -- and the
number is a RATCHET: it may fall, and a rise means a scan was re-introduced.

Usage:  python Tools/verify-registry-scans.py [--list]
"""

import os
import re
import sys
from collections import defaultdict

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRECTORIES = [os.path.join("Sources", "AI"), os.path.join("Sources", "Engine")]

# A loop bounded by a registry-size call. Captures the registry name.
REGISTRY_LOOP_PATTERN = re.compile(r"GC\.getNum([A-Za-z]+)Infos\s*\(\s*\)")
LOOP_CONTEXT_PATTERN = re.compile(r"\bfor\s*\(|\bwhile\s*\(")
# A function definition at column zero: `Type Class::name(` or `Type name(`.
FUNCTION_DEFINITION_PATTERN = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_:<>,&*\s]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*$")

ENABLER_DOMAIN_REGISTRIES = frozenset([
    "Unit", "Building", "Tech", "Civic", "Project", "Process", "Promotion", "Build",
])


def enclosing_function(lines, line_index):
    """Walk backwards to the nearest column-zero function definition."""
    for candidate in range(line_index, max(-1, line_index - 400), -1):
        match = FUNCTION_DEFINITION_PATTERN.match(lines[candidate])
        if match and not lines[candidate].lstrip().startswith(("//", "*", "#")):
            return match.group(1)
    return "?"


def collect_scans():
    """Every registry-bounded loop under the scanned directories."""
    found = []
    for relative_directory in SCAN_DIRECTORIES:
        absolute_directory = os.path.join(REPO_ROOT, relative_directory)
        if not os.path.isdir(absolute_directory):
            continue
        for current_root, _directories, files in os.walk(absolute_directory):
            for filename in sorted(files):
                if not filename.endswith(".cpp"):
                    continue
                absolute_path = os.path.join(current_root, filename)
                handle = open(absolute_path, "r", encoding="utf-8", errors="replace")
                try:
                    lines = handle.read().splitlines()
                finally:
                    handle.close()

                relative_path = os.path.relpath(absolute_path, REPO_ROOT)
                for line_index, line in enumerate(lines):
                    registry_match = REGISTRY_LOOP_PATTERN.search(line)
                    if not registry_match:
                        continue
                    # The bound may sit on the line below the `for (`, so check both.
                    window = line
                    if line_index > 0:
                        window = lines[line_index - 1] + " " + line
                    if not LOOP_CONTEXT_PATTERN.search(window):
                        continue
                    found.append({
                        "file": relative_path.replace("\\", "/"),
                        "line": line_index + 1,
                        "registry": registry_match.group(1),
                        "function": enclosing_function(lines, line_index),
                    })
    return found


def main():
    show_list = "--list" in sys.argv
    scans = collect_scans()

    enabler_domain = [s for s in scans if s["registry"] in ENABLER_DOMAIN_REGISTRIES]
    other_registry = [s for s in scans if s["registry"] not in ENABLER_DOMAIN_REGISTRIES]

    print("registry-bounded loops found: %d  (Sources/AI + Sources/Engine)" % len(scans))
    print("  ENABLER-DOMAIN : %d   -> re-point onto the maintained frontier" % len(enabler_domain))
    print("  OTHER-REGISTRY : %d   -> own-data inversion; use the entity's compiled entries"
          % len(other_registry))

    by_registry = defaultdict(int)
    for scan in scans:
        by_registry[scan["registry"]] += 1
    print("")
    print("by registry: " + ", ".join(
        "%s=%d" % (name, count) for name, count in sorted(by_registry.items(), key=lambda pair: -pair[1])))

    if show_list:
        for heading, group in (("ENABLER-DOMAIN", enabler_domain), ("OTHER-REGISTRY", other_registry)):
            print("")
            print("=== %s (%d) ===" % (heading, len(group)))
            for scan in group:
                print("  %s:%d  %sInfos  %s"
                      % (scan["file"], scan["line"], scan["registry"], scan["function"]))
    else:
        print("")
        print("(run with --list for every site)")

    print("")
    print("ADVISORY: a registry walk is legitimate in init, reset, serialization, save/load, UI")
    print("enumeration and text rendering; only a DECISION path is a defect. Triage per site.")
    print("The counts are a RATCHET: they may fall. A rise means a scan was re-introduced.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
