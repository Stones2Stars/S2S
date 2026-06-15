#!/usr/bin/env python3
"""Curate Victory to the top-down model (#428) — a CONFIG entity defining the game's victory CONDITIONS.

NOT a POCO (2026-06-14 PM audit, wf verify-pocos): the fast-path dumped its gameplay fields into `identity`, but
they are read every turn by CvGame::testVictory (CvGame.cpp:6081 + 265/685/2773-2825/7504-7627) and AI
victory-stage evaluation. They are the win-condition definition, gathered under one `condition` section:
- KIND/property flags -> condition booleans: `bConquest` (eliminate rivals), `bTargetScore`/`bEndScore`
  (score/time), `bDiploVote` (UN vote), `bTotalVictory` (mastery), `bPermanent` (the always-available score
  victory; NB its getter isPermanent() appears UNREAD today -> purge candidate, kept faithfully for now).
- THRESHOLDS -> condition numbers: `iLandPercent`/`iMinLandPercent`/`iPopulationPercentLead` (domination),
  `iReligionPercent` (religious), `iNumCultureCities` + `CityCulture` (a CultureLevel ref) (cultural),
  `iVictoryDelayTurns` (space-race travel delay).
`VictoryMovie` -> art.movie; Type/Description/Civilopedia standard. (Dropped from curate_pocos.POCOS.)

Quirks (faithful): `getTotalCultureRatio()` has a live getter but NO XML record sets `iTotalCultureRatio`, so
it is never emitted (dormant mechanic). VICTORY_SCIENTIFIC carries no condition fields at all (its trigger lives
elsewhere — hardcoded / space-race adjacent).

  python3 curate_victory.py --sample VICTORY_DOMINATION VICTORY_TOTAL VICTORY_CULTURAL VICTORY_SCORE
  python3 curate_victory.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO

# boolean kind/property flags -> clean condition keys (only `true` emitted)
COND_FLAGS = OrderedDict([
    ("bConquest", "conquest"), ("bTargetScore", "targetScore"), ("bEndScore", "endScore"),
    ("bDiploVote", "diploVote"), ("bTotalVictory", "totalVictory"), ("bPermanent", "permanent"),
])
# numeric thresholds -> clean condition keys (only non-zero emitted)
COND_NUMS = OrderedDict([
    ("iLandPercent", "landPercent"), ("iMinLandPercent", "minLandPercent"),
    ("iPopulationPercentLead", "populationPercentLead"), ("iReligionPercent", "religionPercent"),
    ("iNumCultureCities", "numCultureCities"), ("iVictoryDelayTurns", "delayTurns"),
])


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    for tag, key in (("Description", "description"), ("Civilopedia", "civilopedia")):
        t = engine.text(rec.find(tag))
        if t:
            out[key] = t
    cond = OrderedDict()
    for tag, key in COND_FLAGS.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            cond[key] = True
    cc = engine.text(rec.find("CityCulture"))           # a CultureLevel ref (enum-as-int): cultural victory
    if cc and cc != "NONE":
        cond["cityCulture"] = cc
    for tag, key in COND_NUMS.items():
        v = engine.text(rec.find(tag))
        if engine.is_int(v) and int(v) != 0:
            cond[key] = int(v)
    if cond:
        out["condition"] = cond
    mv = engine.text(rec.find("VictoryMovie"))
    if mv:
        out["art"] = {"movie": mv}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: all)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("VictoryInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    print("VictoryInfo curated: %d  | with condition: %d  | with art: %d"
          % (len(results), sum(1 for o in results.values() if "condition" in o),
             sum(1 for o in results.values() if "art" in o)))
    if args.sample is not None:
        for nm in (args.sample or list(results)):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "victories")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d VictoryInfo JSON files under Assets/Data/victories" % len(results))


if __name__ == "__main__":
    main()
