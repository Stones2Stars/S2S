#!/usr/bin/env python3
"""Curate GameSpeed to the top-down model (#428) — a CONFIG/GLOBAL entity (enables nothing): the master
PACING percentage. It sits at the TOP of the cascade and scales everything beneath, so it deposits at
`world` scope (owner: "a base multiplier on everything, lives in global").

Two stored scalars, authored AS THE PERCENTAGES THEY ARE (owner rulings 2026-06-15): the data leads and the
C++ data-fetching is reworked to fit it; the file must read COLD to a modder; never reverse-engineer the
unit/shape from how the engine currently fetches it. Both verified vs Sources/Infos/CvGameSpeedInfo.{h,cpp}
(everything else there is DERIVED: getHammerCostPercent = iSpeedPercent ± an UPSCALED option; turns/calendar
from CvEraInfo):

- `speed.world.percent` = iSpeedPercent — the master game-pace percentage (Normal=100 → 100%, Eternity=1000 →
  1000%, default 100). ONE value the modder sets; the ENGINE applies it across costs (unit/building/project/
  research), effect durations (anger/decay/happiness), food-to-grow, and the culture-points-to-reach-a-level
  scale. That application is the engine/readers job (§7 combine-mode), NOT enumerated in the data — an earlier
  pass fanned this one value into costs/growth/durations/cultureThreshold members, which (a) repeated one
  number across many keys, (b) faked independently-tunable knobs that aren't, and (c) required codebase
  knowledge to read; collapsed per the cold-modder ruling.
- `missionYieldMultiplier.world.percent` = iUnitYieldScalePercent — the multiplier (as a percentage) on yields
  a unit MISSION produces: a merchant's trade mission boosting another city, a subdued animal slaughtered for
  food/production (the `<AdaptUnitYield>` channel, ~sqrt of speed; Normal=500, Eternity=1575). A separate value,
  independently tunable.

CultureLevel dedup (its turn at Tier A): its per-speed `SpeedThresholds` were a redundant precomputation of
`base(Normal) * iSpeedPercent/100` — CultureLevel keeps only the Normal base and derives per-speed by
referencing this single `speed` value, instead of duplicating the table.

  python3 curate_gamespeed.py --sample GAMESPEED_NORMAL
  python3 curate_gamespeed.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO


def curate(typ, rec):
    out = OrderedDict()
    out["type"] = typ
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = engine.text(rec.find(tag))
        if t:
            out[key] = t
    speed = engine.text(rec.find("iSpeedPercent"))
    if engine.is_int(speed):
        out["speed"] = {"world": {"percent": int(speed)}}
    uys = engine.text(rec.find("iUnitYieldScalePercent"))
    if engine.is_int(uys) and int(uys) != 0:
        out["missionYieldMultiplier"] = {"world": {"percent": int(uys)}}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("GameSpeedInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    print("GameSpeedInfo curated: %d  | families: speed, missionYieldMultiplier" % len(results))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "gamespeeds")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d GameSpeedInfo JSON files under Assets/Data/gamespeeds" % len(results))


if __name__ == "__main__":
    main()
