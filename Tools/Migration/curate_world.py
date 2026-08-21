#!/usr/bin/env python3
"""Curate World (map size) to the top-down model (#430 audit item 15) -- a pure CONFIG entity
(state-repositories.md: "WORLD is CONFIG"): it enables nothing, deposits nothing per-turn, and every field is
a parameter some engine formula reads off the selected map size. The census over CIV4WorldInfo.xml confirms
NO per-turn effects, so there are NO modifier families -- the whole record is the `identity` block ("what am
I": a map size IS its config numbers, the cultureLevel cityRadius/cultureThreshold precedent, json.md 7).
Values are the human numbers the XML authors (every scale verified at its engine consumption site; nothing
here is x100 -- fixed-point-and-scales.md 3).

Emitted keys (identity.<key> = legacy tag -- consumption site, scale verdict):
- `defaultPlayers` = iDefaultPlayers -- default player count for the size (EXE reads it via the DllExport
  getter for the staging screens; CvGame.cpp:6963 ranged-civs cap; CvPlayerAI.cpp:22694 paranoia damping).
  Plain count.
- `targetNumCities` = iTargetNumCities -- the per-civ expected city count (start-spacing normalizer; the
  population-rank gates CvCity.cpp:5554/8658; civic-value weighting CvPlayerAI.cpp:14278). Plain count.
- `gridWidth` / `gridHeight` = iGridWidth/iGridHeight -- map dimensions in landscape cells (CvMap.cpp:163
  scales by the landscape's plots-per-cell). Plain counts.
- `terrainGrainChange` / `featureGrainChange` = iTerrainGrainChange/iFeatureGrainChange -- clumping deltas
  consumed ONLY by the Python map scripts (bound at CyInfoInterface3.cpp:200-201). Plain signed steps.
- `oceanMinAreaSize` = iOceanMinAreaSize -- min water-area tile count to rate as ocean (CvArea.cpp:509
  isLake; the isCoastal(...) callers across the AI). Plain tile count.
- `buildingPrereqModifier` = iBuildingPrereqModifier -- signed % delta on PrereqAmountBuildings counts
  (CvPlayer.cpp:7333 getModifiedIntValue). Human percent delta.
- `maxConscriptModifier` = iMaxConscriptModifier -- signed % delta on conscription cap
  (CvGameCoreUtils.cpp:255 *(100+mod)/100). Human percent delta.
- `warWearinessModifier` = iWarWearinessModifier -- signed % delta on war-weariness anger
  (CvPlayer.cpp:10964 getModifiedIntValue). Human percent delta.
- `tradeProfitPercent` = iTradeProfitPercent -- per-plot-distance trade-profit rate: min'd against the
  population term and scaled by the TRADE_PROFIT_PERCENT define into the x100 profit
  (CvCity.cpp:11598 getBaseTradeProfit). The authored number IS the human knob -- emitted as-is.
- `corporationMaintenancePercent` = iCorporationMaintenancePercent -- % scale on BOTH the corporation
  maintenance component (CvCity.cpp:7837) and the corporation yields/commerces-produced output
  (CvCity.cpp:12601/12626 -- the "world corp maintenance pct" of fixed-point-and-scales.md 4c). Human percent.
- `numCitiesAnarchyPercent` = iNumCitiesAnarchyPercent -- extra anarchy turns per city as a percent
  (CvPlayer.cpp:8997/9035 getNumCities()*pct/100). Human percent.
- `advancedStartPointsMod` = iAdvancedStartPointsMod -- % scale on the advanced-start points budget
  (CvInitCore.cpp:1654 *mod/100). Human percent.
- `cityLimitsScalePercent` = iCityLimitsScalePercent -- % scale on civic city limits by map size
  (CvDepositRead.cpp:78 + CvInfoValuation.cpp:515, *pct/100 under GAMEOPTION_EXP_OVEREXPANSION_PENALTIES).
  Human percent. The one field with a non-zero engine default (100 = no change) -- always emitted, and the
  poco defaults to 100 when absent (a modded size without it must scale by 100, never 0).

DEAD fields the census retired (authored in the XML, ZERO consumers -- exhaustive grep over Sources/ + the
CyInfoInterface bindings; the XML keeps them as curator input, revival = re-add the row here + regen):
- iUnitNameModifier -- the BTS unit-naming-chance scaler; no S2S read site exists (the legacy getter was
  never called and is not Python-bound). NOT emitted.
- iNumFreeBuildingBonuses -- the BTS "world default" for building free-bonus counts; S2S buildings author
  explicit per-building ExtraFreeBonus counts (curate_building.py), the world-default path does not exist
  in the DLL (no member, no getter). NOT emitted.

Zero-valued entries are dropped (0 is every emitted member's poco default except cityLimitsScalePercent,
whose authored values are never 0) -- lossless, mirroring curate_handicap.

  python3 curate_world.py --sample WORLDSIZE_STANDARD
  python3 curate_world.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import de_i, fold_text_to_identity
from store import Store, REPO

# legacy tag -> identity key (all human values, emitted as-is; drop-if-zero is lossless vs the poco defaults)
CONFIG_FIELDS = OrderedDict([
    ("iDefaultPlayers",                "defaultPlayers"),
    ("iTargetNumCities",               "targetNumCities"),
    ("iGridWidth",                     "gridWidth"),
    ("iGridHeight",                    "gridHeight"),
    ("iTerrainGrainChange",            "terrainGrainChange"),
    ("iFeatureGrainChange",            "featureGrainChange"),
    ("iOceanMinAreaSize",              "oceanMinAreaSize"),
    ("iBuildingPrereqModifier",        "buildingPrereqModifier"),
    ("iMaxConscriptModifier",          "maxConscriptModifier"),
    ("iWarWearinessModifier",          "warWearinessModifier"),
    ("iTradeProfitPercent",            "tradeProfitPercent"),
    ("iCorporationMaintenancePercent", "corporationMaintenancePercent"),
    ("iNumCitiesAnarchyPercent",       "numCitiesAnarchyPercent"),
    ("iAdvancedStartPointsMod",        "advancedStartPointsMod"),
    ("iCityLimitsScalePercent",        "cityLimitsScalePercent"),
])

# authored-but-dead legacy tags (docstring): consumed nowhere in the DLL or the Python bindings -- retired.
# Dropped outright, never parked in `identity`: identity carries no effects (json.md 7), so a dead effect
# routed there would land in the one block that must not hold it. The two maintenance percents scaled
# engine formulas that no longer exist -- maintenance is authored deposits now (economy.md).
DEAD_FIELDS = ("iUnitNameModifier", "iNumFreeBuildingBonuses",
               "iDistanceMaintenancePercent", "iColonyMaintenancePercent")

HOIST_TEXT = {"Description": "description", "Help": "help"}


def curate(typ, rec):
    out = OrderedDict()
    out["type"] = typ
    identity = OrderedDict()
    leftover = []
    for child in rec:
        tag = child.tag
        value = engine.text(child)
        if tag == "Type":
            continue
        elif tag in HOIST_TEXT:
            if value:
                out[HOIST_TEXT[tag]] = value
        elif tag in CONFIG_FIELDS:
            if engine.is_int(value) and int(value) != 0:
                identity[CONFIG_FIELDS[tag]] = int(value)
        elif tag in DEAD_FIELDS:
            continue
        else:
            if list(child) or value:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(child)
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md 7)
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("WorldInfo")
    results = OrderedDict()
    all_leftover = set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec)
        results[typ] = obj
        all_leftover.update(leftover)
    print("WorldInfo curated: %d  | pure config -- identity only, no modifier families" % len(results))
    if all_leftover:
        print("  !! UNHANDLED tags routed to identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified -- no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "worlds")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d WorldInfo JSON files under Assets/Data/worlds" % len(results))


if __name__ == "__main__":
    main()
