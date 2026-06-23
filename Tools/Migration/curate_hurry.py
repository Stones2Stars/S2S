#!/usr/bin/env python3
"""Curate Hurry to the top-down model (#428) — a tiny CONFIG entity defining a production-RUSH mechanic.

NOT a POCO (2026-06-14 PM audit, wf verify-pocos): the first-pass mapping dumped its 3 gameplay fields into
`identity`, but they drive real city/AI logic (CvCity.cpp:4089-4090 canHurry gate + 6086-6119 cost/yield calc,
6144 anger-duration; CvPlayer.cpp:13903 pop-rush tracking; CvPlayerAI.cpp:14750 civic value). They are the
hurry's intrinsic mechanic parameters:
- `iGoldPerProduction` / `iProductionPerPopulation` -> `conversion` (the rush RATE — gold spent per hammer, or
  hammers gained per population sacrificed). Mutually exclusive per hurry; the 0 (not-applicable) rate is dropped
  (its absence reconstructs as 0 — how the engine distinguishes a gold-rush from a pop-rush, getX()>0).
  NB: the cascade MODIFIER on hurry cost lives elsewhere (BuildingInfo.iHurryCostModifier, a city-scope percent);
  these are the BASE rates it acts on, so they stay intrinsic to the hurry, not a deposited modifier.
- `bAnger` -> `causesAnger` (using this hurry inflicts temporary hurry-anger; CvCity.cpp:6144).
`Button` -> art.icon; Type/Description standard. (HurryInfo dropped from curate_pocos.POCOS — it was mis-curated.)

DEFERRED source-side GRANT edges (author at the SOURCE pass, NOT here — a hurry is granted by others):
- Civics grant HURRY_GOLD (all 20 hurry-granting civics across Currency/Economy/Power -> enables.hurries).
- HURRY_POPULATION comes ONLY from a Slavery special-building via BuildingInfo.isHurry (CvPlayer.cpp:7503); the
  grantor is BUILDING_WORLDVIEW_SLAVERY. More of the SLAVERY "system" clusters around the WORLDVIEW buildings —
  sleeping dog: wire BuildingInfo -> enables.hurries AND map the rest of the slavery system at the Building pass.

DO NOT CONFLATE with the unit "hurry": two distinct mechanics share the verb. (1) THIS entity = CvHurryInfo /
HurryTypes, the gold/pop city-rush. (2) UnitInfo.iBaseHurry/iHurryMultiplier = great-engineer "Extra
Construction" (consume the unit in a city to add production), via CvUnit::getHurryProduction / CvUnit::canHurry —
an entirely separate data + code path, carried at the Unit pass (see curate_unit.py GP_ACTIONS["hurry"]).

  python3 curate_hurry.py --sample HURRY_GOLD HURRY_POPULATION
  python3 curate_hurry.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from store import Store, REPO


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    desc = engine.text(rec.find("Description"))
    if desc:
        out["description"] = desc
    conversion = OrderedDict()
    gpp = engine.text(rec.find("iGoldPerProduction"))
    if engine.is_int(gpp) and int(gpp) != 0:
        conversion["goldPerProduction"] = int(gpp)          # gold spent per hammer rushed (gold-rush)
    ppp = engine.text(rec.find("iProductionPerPopulation"))
    if engine.is_int(ppp) and int(ppp) != 0:
        conversion["productionPerPopulation"] = int(ppp)    # hammers gained per population sacrificed (pop-rush)
    if conversion:
        out["conversion"] = conversion
    if engine.text(rec.find("bAnger")) in ("1", "true", "True"):
        out["causesAnger"] = True
    art_blocks = {}
    cc.put_art(art_blocks, "Button", engine.text(rec.find("Button")))   # -> ui.art.icon via ART_BLOCK
    cc.emit_art(out, art_blocks)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: all)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("HurryInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    print("HurryInfo curated: %d" % len(results))
    if args.sample is not None:
        for nm in (args.sample or list(results)):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "hurries")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d HurryInfo JSON files under Assets/Data/hurries" % len(results))


if __name__ == "__main__":
    main()
