#!/usr/bin/env python3
"""Post-curation additions -- deep-merge hand-authored additions ON TOP of the curated JSON (the FINAL offline step).

Building/entity curation is complete (owner 2026-07-21), so gameplay additions no longer belong in the legacy XML
(curator input). They are authored HERE, as a SEPARATE, reviewable, revertible layer, and re-applied as the LAST
step of the offline Python pipeline. The GAME never knows any of this exists -- it reads only the final Assets/Data
JSON (owner: "the c++ should not know or care that the json is now different from xml; the game does not, and should
not know that there is such a thing as curation"). Curation + additions are one offline entity that PRODUCES that JSON.

Shape: each Assets/Data/_additions/<type>.json maps an ENTITY id -> a partial object DEEP-MERGED into that entity's
curated JSON (dicts recurse; leaves/lists are SET/overridden). Re-run after ANY re-curate so additions land last.

  python3 curate_additions.py            # dry-run: report what WOULD merge (+ any missing entities)
  python3 curate_additions.py --write    # apply the merges in place
"""
import argparse, glob, json, os
from collections import OrderedDict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(REPO, "Assets", "Data")
ADDITIONS = os.path.join(DATA, "_additions")


def deep_merge(base, add):
    """Recursively merge `add` INTO `base`: matching dicts recurse; everything else (leaves, lists) is set/overridden."""
    for k, v in add.items():
        if isinstance(v, dict) and isinstance(base.get(k), dict):
            deep_merge(base[k], v)
        else:
            base[k] = v


def find_entity_file(type_dir, entity_id):
    """Locate an entity's curated JSON by convention (lowercase id + .json), searching subfolders (buildings are
    era-foldered, most types are flat). Returns the path, or None if not found."""
    fname = entity_id.lower() + ".json"
    hits = glob.glob(os.path.join(type_dir, "**", fname), recursive=True)
    return hits[0] if hits else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    if not os.path.isdir(ADDITIONS):
        print("no _additions/ folder (%s) -- nothing to apply" % ADDITIONS)
        return
    total_applied = total_missing = 0
    for af in sorted(glob.glob(os.path.join(ADDITIONS, "*.json"))):
        type_name = os.path.splitext(os.path.basename(af))[0]   # "buildings", "units", ...
        type_dir = os.path.join(DATA, type_name)
        adds = json.load(open(af, encoding="utf-8"), object_pairs_hook=OrderedDict)
        applied = missing = 0
        for entity_id, partial in adds.items():
            ef = find_entity_file(type_dir, entity_id)
            if ef is None:
                print("  MISSING: %s -> %s.json not found under %s/" % (entity_id, entity_id.lower(), type_name))
                missing += 1
                continue
            if args.write:
                d = json.load(open(ef, encoding="utf-8"), object_pairs_hook=OrderedDict)
                deep_merge(d, partial)
                # match the curators' exact serialization (indent=1, ensure_ascii=False, no trailing newline) so an
                # addition is a MINIMAL diff (only the merged keys move), never a whole-file reformat.
                with open(ef, "w", encoding="utf-8") as f:
                    json.dump(d, f, indent=1, ensure_ascii=False)
            applied += 1
        print("%-16s %d applied, %d missing" % (type_name, applied, missing))
        total_applied += applied
        total_missing += missing
    verb = "merged" if args.write else "would merge"
    print("%s: %d entities %s across all _additions; %d missing" %
          ("WROTE" if args.write else "DRY-RUN", total_applied, verb, total_missing))


if __name__ == "__main__":
    main()
