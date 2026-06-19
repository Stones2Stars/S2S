#!/usr/bin/env python3
"""cascade_sim -- offline cascade SIMULATOR prototype (calc-emulator-spec.md §2a).

Feeds the NEW cascade model a real loadout and computes the per-turn values OFFLINE, reading the migrated
Assets/Data JSON deposits directly -- the Python prototype of the in-game cascade engine ("simulate the
simulation"). It is a port of the DLL's rjParseModifiers[city scope] + cascadeModifierCitySlot + cascadeModifierApply.

Increment 1: the YIELD channel from BUILDINGS (city-scope flat/percent). It compares the Python sum to:
  - the dump's DLL-cascade (cascadeFlat/cascadePercent) -> proves Python reads the JSON the SAME way the DLL does;
  - (later) legacy -> the missing non-building sources = XML->JSON migration BLINDSPOTS.
Conditional (`enabled`/`disabled`) deposits are COUNTED but not yet evaluated (increment 2 ports the enabler
condition evaluator against the loadout dict -- techs/civics/buildings/resources present). Other source classes
(civics/bonuses/specialists/plots) + channels are later increments.

Run:
    python cascade_sim.py --file samples/london.json
"""
import argparse
import glob
import json
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILDINGS_DIR = os.path.join(REPO, "Assets", "Data", "buildings")
YIELDS = ("food", "production", "commerce")


def building_index():
    """basename (building_x.json) -> full path, globbed once from Assets/Data/buildings/**."""
    idx = {}
    for p in glob.glob(os.path.join(BUILDINGS_DIR, "**", "building_*.json"), recursive=True):
        idx[os.path.basename(p).lower()] = p
    return idx


def type_to_filename(btype):
    """BUILDING_FORGE -> building_forge.json (the curator's naming convention)."""
    return "building_" + btype[len("BUILDING_"):].lower() + ".json"


def parse_city_yield_deposits(bjson):
    """{yield: {'flat': (uncond, condCount), 'percent': (uncond, condCount)}} for CITY scope.
    Mirrors rjParseModifiers: family.city.{flat,percent} = scalar | {value, enabled?/disabled?} | array-of-those."""
    out = {}
    for y in YIELDS:
        fam = bjson.get(y)
        if not isinstance(fam, dict):
            continue
        city = fam.get("city")
        if not isinstance(city, dict):
            continue
        rec = {}
        for unit in ("flat", "percent"):
            uncond, cond = 0, 0
            v = city.get(unit)
            if v is not None:
                for it in (v if isinstance(v, list) else [v]):
                    if isinstance(it, (int, float)):
                        uncond += int(it)
                    elif isinstance(it, dict) and "value" in it:
                        if "enabled" in it or "disabled" in it:
                            cond += 1            # conditional -- needs the enabler eval (increment 2)
                        else:
                            uncond += int(it["value"])
            rec[unit] = (uncond, cond)
        out[y] = rec
    return out


def simulate_yields(d):
    """Sum present-buildings' UNCONDITIONAL city-scope yield deposits. -> (sim, missing-file count)."""
    idx = building_index()
    sim = {y: {"flat": 0, "percent": 0, "condFlat": 0, "condPct": 0} for y in YIELDS}
    missing = 0
    for bt in d.get("buildings", []):
        path = idx.get(type_to_filename(bt))
        if path is None:
            missing += 1
            continue
        with open(path) as fh:
            dep = parse_city_yield_deposits(json.load(fh))
        for y, rec in dep.items():
            uf, cf = rec["flat"]
            up, cp = rec["percent"]
            sim[y]["flat"] += uf
            sim[y]["condFlat"] += cf
            sim[y]["percent"] += up
            sim[y]["condPct"] += cp
    return sim, missing


def main():
    ap = argparse.ArgumentParser(description="cascade_sim -- offline cascade simulator (yields, buildings)")
    ap.add_argument("--file", required=True, help="cityInput dump fixture (loadout + yields)")
    args = ap.parse_args()
    with open(args.file) as fh:
        d = json.load(fh)
    sim, missing = simulate_yields(d)
    dumped = {y["family"]: y for y in d.get("yields", [])}

    print("=== cascade_sim [%s]: Python cascade (buildings, city-scope yields) vs DLL-cascade ===" % d.get("cityName", "?"))
    print("  family      py-flat  dll-flat | py-pct  dll-pct | cond(f/p) | py==DLL?")
    allok = True
    for y in YIELDS:
        s = sim[y]
        dy = dumped.get(y, {})
        dflat, dpct = dy.get("cascadeFlat", 0), dy.get("cascadePercent", 0)
        match = (s["flat"] == dflat and s["percent"] == dpct)
        allok = allok and match
        print("  %-10s %8d %8d | %6d %7d | %3d/%-3d | %s"
              % (y, s["flat"], dflat, s["percent"], dpct, s["condFlat"], s["condPct"],
                 "OK" if match else "DIFF"))
    if missing:
        print("  [%d present buildings had no JSON file found -- modules/naming gap]" % missing)
    print("\n  Python sums UNCONDITIONAL city-scope building deposits; the DLL also evaluates `enabled` conditionals")
    print("  live -- so a DIFF is expected wherever cond(f/p) > 0 (increment 2 ports the enabler-condition eval).")
    print("  %s" % ("ALL MATCH (no active conditionals here)" if allok else "DIFF present -> the conditional-deposit gap, quantified by cond(f/p)."))


if __name__ == "__main__":
    main()
