"""Fail when the SIMPLE trait set has leaked into the COMPLEX one.

The two sets are SEPARATE and self-complete: no overlay, no base-fill, no superset relationship, and a complex
game has never used rung 0 of any trait -- a line is 1 -> 2 -> 3
([DEC-trait-sets-separate], docs/specs/modifier.md §4).

This exists because that ruling has been re-litigated repeatedly. It was corrected in conversation, RECORDED
INVERTED under an "(owner ruling)" stamp, restated in four separate passages, and rebuilt in good faith by each
agent that read one of them. Prose is the weakest rung of
[DEC-hard-typing-or-rollerskate](docs/architecture/decisions.md#dec-hard-typing-or-rollerskate); a check does not
have to be remembered.

  the leak: an UN-DIGITED record in complex/ for a line that HAS numbered rungs. That record is the simple
  base copied across, sitting as a rung 0 nothing in a complex game ever holds -- and while it is held it
  contributes thresholds and values the ladder never granted.

A line with NO numbered rungs may legitimately carry a bare record (there is no rung 0 concept for it).

Run from the repo root:  python Tools/verify-trait-sets.py
"""

import json
import os
import re
import sys

SIMPLE_DIR = os.path.join("Assets", "Data", "traits", "simple")
COMPLEX_DIR = os.path.join("Assets", "Data", "traits", "complex")
LEADER_DIR = os.path.join("Assets", "Data", "leaderheads")

COMPLEX_PREFIX = "TRAIT_COMPLEX_"
SIMPLE_PREFIX = "TRAIT_"


def load_records(directory):
    """type id -> (filename, record), skipping manifests and anything that is not an entity object."""
    records = {}
    if not os.path.isdir(directory):
        return records
    for name in sorted(os.listdir(directory)):
        if not name.endswith(".json") or name.startswith("_"):
            continue
        with open(os.path.join(directory, name)) as handle:
            record = json.load(handle)
        if isinstance(record, dict) and "type" in record:
            records[record["type"]] = (name, record)
    return records


def rung_number(type_id):
    """The trailing rung number, or None for an un-digited base record."""
    match = re.search(r"(\d+)$", type_id)
    return int(match.group(1)) if match else None


def stem_of(type_id):
    return re.sub(r"\d+$", "", type_id)


def main():
    complex_records = load_records(COMPLEX_DIR)
    simple_records = load_records(SIMPLE_DIR)
    if not complex_records:
        print("verify-trait-sets: no complex trait records found -- run from the repo root")
        return 2

    stems_with_rungs = set()
    for type_id in complex_records:
        if rung_number(type_id) is not None:
            stems_with_rungs.add(stem_of(type_id))

    leaked = sorted(
        type_id for type_id in complex_records
        if rung_number(type_id) is None and type_id in stems_with_rungs
    )

    # A leaderhead must not name a leaked base in its complexTraits list.
    holders = []
    for name in sorted(os.listdir(LEADER_DIR)) if os.path.isdir(LEADER_DIR) else []:
        if not name.endswith(".json") or name.startswith("_"):
            continue
        with open(os.path.join(LEADER_DIR, name)) as handle:
            leader = json.load(handle)
        if not isinstance(leader, dict):
            continue
        named = [t for t in leader.get("complexTraits", []) or [] if t in leaked]
        if named:
            holders.append((name, named))

    # A complex record must carry the complex prefix, and a simple record must not.
    misprefixed = sorted(t for t in complex_records if not t.startswith(COMPLEX_PREFIX))
    misprefixed += sorted(t for t in simple_records if t.startswith(COMPLEX_PREFIX))

    print("trait sets: %d simple, %d complex (%d numbered rungs)"
          % (len(simple_records), len(complex_records),
             sum(1 for t in complex_records if rung_number(t) is not None)))

    if not leaked and not holders and not misprefixed:
        print("trait sets clean -- complex carries no rung 0, and the sets share no id.")
        return 0

    if misprefixed:
        print("\nRECORDS IN THE WRONG SET (%d):" % len(misprefixed))
        for type_id in misprefixed:
            print("    %s" % type_id)

    if leaked:
        print("\nSIMPLE BASES LEAKED INTO complex/ AS RUNG 0 (%d):" % len(leaked))
        print("  each is an un-digited record for a line that has numbered rungs, so a complex game")
        print("  never holds it -- the ladder is 1 -> 2 -> 3 ([DEC-trait-sets-separate]).")
        for type_id in leaked:
            rungs = sorted(t for t in complex_records
                           if stem_of(t) == type_id and rung_number(t) is not None)
            print("    %-34s (ladder: %s)" % (type_id, ", ".join(rungs) or "-"))

    if holders:
        print("\nLEADERHEADS NAMING A LEAKED BASE IN complexTraits (%d):" % len(holders))
        for name, named in holders:
            print("    %-34s %s" % (name, ", ".join(named)))

    print("\nFix the CURATOR so the leak cannot be re-emitted, then regenerate")
    print("([DEC-recurate-on-decision]) -- never by hand-editing Assets/Data.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
