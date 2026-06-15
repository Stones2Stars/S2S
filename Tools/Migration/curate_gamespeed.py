#!/usr/bin/env python3
"""Curate GameSpeed to the top-down model (#428) — a CONFIG/GLOBAL entity (enables nothing): the master
PACING multiplier. It sits at the TOP of the cascade and multiplies everything beneath, so it deposits at
`world` scope (owner: "a base multiplier on everything, lives in global").

Two stored scalars (verified vs Sources/Infos/CvGameSpeedInfo.{h,cpp} — everything else is DERIVED:
getHammerCostPercent = iSpeedPercent + an optional UPSCALED_HAMMER_COST_MODIFIER game-option; turns/calendar
from CvEraInfo). The single `iSpeedPercent` fans out into the universal cost + duration families
(owner ruling 2026-06-14):

- `costs.world.{train,construct,create,research}.percent` = iSpeedPercent — the MULTIPLIER on base costs that
  live on the infos (unit/building/project production cost; tech research cost). FINE-GRAINED per produced-thing
  (Era distinguishes them per-era; GameSpeed sets all four = iSpeedPercent). The ~99% global source.
- `growth.world.percent` = iSpeedPercent — the food-to-grow scale, its OWN family (like Era + Handicap), NOT
  `costs.food`.
- `durations.world.{anger,decay,happiness}.percent` = iSpeedPercent — length of anger / production-decay /
  happiness effects. Member list is PROVISIONAL: the full set is enumerated by the consumption rewrite
  (~150 ad-hoc `getSpeedPercent()/100` sites across CvCity/CvPlayer/CvGame/…; the C++ readers-phase task).
- `unitYieldScale.world.percent` = iUnitYieldScalePercent — a YIELD multiplier (the `<AdaptUnitYield>` channel,
  ~sqrt of speed), NOT a cost; its own family.
- `cultureThreshold.world.percent` = iSpeedPercent — the culture-points-to-reach-a-level scale. CultureLevel's
  per-speed `SpeedThresholds` were REDUNDANT precomputation of `base(Normal) * iSpeedPercent/100` (values
  identical, and the GameSpeed XML notes the coupling). The scale lives HERE (owner ruling 2026-06-14);
  CultureLevel keeps only the Normal base and derives per-speed via this member. Its own family.

  python3 curate_gamespeed.py --sample GAMESPEED_NORMAL
  python3 curate_gamespeed.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO

# hammer cost split per produced-thing (unit/building/project) + tech; food-to-grow is the `growth` family.
COST_MEMBERS = ["train", "construct", "create", "research"]
DURATION_MEMBERS = ["anger", "decay", "happiness"]     # PROVISIONAL — completed by the consumption rewrite


def curate(typ, rec):
    out = OrderedDict()
    out["type"] = typ
    for tag, key in (("Description", "description"), ("Help", "help")):
        t = engine.text(rec.find(tag))
        if t:
            out[key] = t
    speed = engine.text(rec.find("iSpeedPercent"))
    if engine.is_int(speed):
        sp = int(speed)
        out["costs"] = OrderedDict([("world", OrderedDict((m, {"percent": sp}) for m in COST_MEMBERS))])
        out["growth"] = {"world": {"percent": sp}}      # food-to-grow scale: its OWN family (like Era/Handicap), not a cost
        out["durations"] = OrderedDict([("world", OrderedDict((m, {"percent": sp}) for m in DURATION_MEMBERS))])
        # culture-points-to-reach-a-level scale (= iSpeedPercent; verified identical to CultureLevel's per-speed
        # SpeedThresholds and coupled by the GameSpeed XML note "...update CultureLevelInfos.xml"). CultureLevel
        # holds the NORMAL base; the reader derives per-speed via this (owner ruling 2026-06-14: the percent
        # belongs on GameSpeed). Its OWN family — a threshold scale, not a cost/duration.
        out["cultureThreshold"] = {"world": {"percent": sp}}
    uys = engine.text(rec.find("iUnitYieldScalePercent"))
    if engine.is_int(uys) and int(uys) != 0:
        out["unitYieldScale"] = {"world": {"percent": int(uys)}}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("GameSpeedInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    print("GameSpeedInfo curated: %d  | families: costs, growth, durations, cultureThreshold, unitYieldScale"
          % len(results))
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
