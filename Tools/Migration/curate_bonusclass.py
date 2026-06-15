#!/usr/bin/env python3
"""Curate BonusClass to the top-down model (#428) — the bonus CATEGORY/class AXIS (resource classes).

NOT a POCO (2026-06-14 PM audit). 11 classes categorize resources: MISC, CROP, LIVESTOCK, SEAFOOD, STRATEGIC,
LUXURY, PRODUCTION, and the "not placed on map" producer classes MANUFACTURED / CULTURE / GENMODS / WONDER
(produced by buildings/cultural wonders). The categorization itself is consumed via the BONUS's `bonusClassType`
(bonus-side, already migrated) — heavily by AI resource awareness/interest (CvPlayerAI:4408-4415/4973,
CvPlayer:25590-25606) and display filtering (CvGameTextMgr:4107). The CLASS ENTITY carries exactly ONE data
field (confirmed vs CvBonusClassInfo.h — only m_iUniqueRange):
- `iUniqueRange` -> `mapGeneration.uniqueRange` — the min-spacing that prevents STACKING bonuses of the same
  class in close proximity; a **C2C_World mapscript feature** (consumed via CvMapGenerator:60-101 during
  placement, using the class range + the bonus's own range). 0 = no constraint (dropped). Parallel to the bonus's
  own uniqueRange in its `mapGeneration` group.
The records carry NO Description (pure structural axis + the one map-gen field). The "not placed on map" semantic
is HARDCODED in consumers (CvGameTextMgr filter) + XML comments, NOT a class field -> not emitted.
(Dropped from curate_pocos.POCOS — it was mis-homed there with iUniqueRange in `identity`.)

  python3 curate_bonusclass.py --sample BONUSCLASS_CROP BONUSCLASS_MISC BONUSCLASS_STRATEGIC
  python3 curate_bonusclass.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    ur = engine.text(rec.find("iUniqueRange"))
    if engine.is_int(ur) and int(ur) != 0:          # 0 = no map-gen spacing constraint -> dropped (faithful)
        out["mapGeneration"] = {"uniqueRange": int(ur)}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: all)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("BonusClassInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    print("BonusClassInfo curated: %d  | with mapGeneration: %d"
          % (len(results), sum(1 for o in results.values() if "mapGeneration" in o)))
    if args.sample is not None:
        for nm in (args.sample or list(results)):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "bonusclasses")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d BonusClassInfo JSON files under Assets/Data/bonusclasses" % len(results))


if __name__ == "__main__":
    main()
