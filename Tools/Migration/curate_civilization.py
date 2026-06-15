#!/usr/bin/env python3
"""Curate Civilization to the top-down model (#428) — a SOURCE entity (game-start grants + per-civ policy/art).

NOT a POCO (the first-pass mapping only saw the spawnRate pair; the POCO caveat applies hard). CivilizationInfo
actively GRANTS game-start state, GATES capability, deposits one modifier, and carries the civ identity/art —
hand-written read() (CvWString names, audio-tag sounds, CivicOption-keyed InitialCivics, the Cities list,
pre-base ArtDefineTag merge) → a bespoke curator. Verdicts (light-four-classification):
- TEXT: Description / ShortDescription / Adjective / Civilopedia.
- `grants` (one-shot, at game start; flat — landing scope implicit in the grant TYPE, consumer-side):
  `buildings` ← FreeBuildings (the capital: Palace + civ-class buildings, re-asserted on capital move),
  `civics` ← InitialCivics (empire: one starting civic per civic-option slot),
  `techs` ← FreeTechs (team). **FreeTechs is Neanderthal-ONLY** (everyone else starts at base): only
  CIVILIZATION_NPC_NEANDERTHAL grants CAVE_DWELLING/GATHERING/NOMADISM/SCAVENGING/LANGUAGE. Live (CvGame:1226).
- `spawnRate.empire.{general,npcPeace}.percent` ← iSpawnRateModifier / iSpawnRateNPCPeaceModifier — the ONE
  cascade modifier (PERCENT, (100+mod)/100; CvGame:6359/6363). Barb/NPC civs only.
- `policies` (booleans): `playable` / `aiPlayable` (can a human/AI pick this civ) + `stronglyRestricted` (NPC
  build-lockdown, CvCity:2205/2547).
- `disables.techs` — per-civ permanent research ban (anti-enable; canEverResearch=false, CvPlayer:8266). The
  `disables` OBJECT mirrors `grants` (owner) so it extends to other kinds (buildings/units/…) later. LIVE but
  Neanderthal-ONLY: CIVILIZATION_NPC_NEANDERTHAL can never research TECH_SEDENTARY_LIFESTYLE (stays nomadic).
- `art`: DefaultPlayerColor / ArtDefineTag / ArtStyleType / UnitArtStyleType + the two civ sounds.
- `identity`: Leaders (civ↔leader eligibility), Cities (`cityNames` pool), DerivativeCiv (civ-split lineage).
No dead structure found (every getter has a live non-class consumer).

  python3 curate_civilization.py --sample CIVILIZATION_AMERICA CIVILIZATION_NPC_NEANDERTHAL
  python3 curate_civilization.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from store import Store, REPO

TEXT = OrderedDict([
    ("Description", "description"), ("ShortDescription", "shortDescription"),
    ("Adjective", "adjective"), ("Civilopedia", "civilopedia"),
])
GRANTS = OrderedDict([      # container tag -> (child tag, grant key)
    ("FreeTechs", ("FreeTech", "techs")),
    ("FreeBuildings", ("BuildingType", "buildings")),
    ("InitialCivics", ("CivicType", "civics")),
])
SPAWN = OrderedDict([("iSpawnRateModifier", "general"), ("iSpawnRateNPCPeaceModifier", "npcPeace")])
POLICIES = OrderedDict([("bPlayable", "playable"), ("bAIPlayable", "aiPlayable"),
                        ("bStronglyRestricted", "stronglyRestricted")])
ART = OrderedDict([
    ("DefaultPlayerColor", "playerColor"), ("ArtDefineTag", "artDefine"),
    ("ArtStyleType", "artStyle"), ("UnitArtStyleType", "unitArtStyle"),
    ("CivilizationSelectionSound", "selectionSound"), ("CivilizationActionSound", "actionSound"),
])
# every tag we knowingly handle — anything else gets flagged (leftover check)
KNOWN = ({"Type", "Cities", "Leaders", "DisableTechs", "DerivativeCiv"} | set(TEXT) | set(GRANTS)
         | set(SPAWN) | set(POLICIES) | set(ART))


def _list(rec, container, child):
    out, node = [], rec.find(container)
    if node is not None:
        for c in node.findall(child):
            t = engine.text(c)
            if t and t != "NONE":
                out.append(t)
    return out


def curate(typ, rec):
    out = OrderedDict([("type", typ)])
    for tag, key in TEXT.items():
        t = engine.text(rec.find(tag))
        if t:
            out[key] = t
    grants = OrderedDict()
    for container, (child, key) in GRANTS.items():
        vals = _list(rec, container, child)
        if vals:
            grants[key] = vals
    if grants:
        out["grants"] = grants
    spawn = OrderedDict()
    for tag, member in SPAWN.items():
        v = engine.text(rec.find(tag))
        if engine.is_int(v) and int(v) != 0:
            spawn[member] = {"percent": int(v)}
    if spawn:
        out["spawnRate"] = {"empire": spawn}
    policies = OrderedDict()
    for tag, key in POLICIES.items():
        if engine.text(rec.find(tag)) in ("1", "true", "True"):
            policies[key] = True
    if policies:
        out["policies"] = policies
    dt = _list(rec, "DisableTechs", "DisableTech")
    if dt:
        out["disables"] = {"techs": dt}   # `disables` OBJECT, symmetric with `grants` — extensible to other kinds later
    art = OrderedDict()
    for tag, key in ART.items():
        t = engine.text(rec.find(tag))
        if t and t != "NONE":
            art[key] = t
    if art:
        out["art"] = art
    identity = OrderedDict()
    leaders = _list(rec, "Leaders", "Leader")
    if leaders:
        identity["leaders"] = leaders
    cities = _list(rec, "Cities", "City")
    if cities:
        identity["cityNames"] = cities
    dc = engine.text(rec.find("DerivativeCiv"))
    if dc and dc != "NONE":
        identity["derivativeCiv"] = dc
    if identity:
        out["identity"] = identity
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    table = Store().table("CivilizationInfo")
    results = OrderedDict((typ, curate(typ, rec)) for typ, rec in table.items())
    leftovers = {}
    for typ, rec in table.items():
        for c in rec:
            if c.tag not in KNOWN and (list(c) or engine.text(c)):
                leftovers.setdefault(c.tag, 0)
                leftovers[c.tag] += 1
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("CivilizationInfo curated: %d  | grants: %d  spawnRate: %d  policies: %d  disables: %d"
          % (len(results), has("grants"), has("spawnRate"), has("policies"), has("disables")))
    if leftovers:
        print("  !! UNHANDLED tags (review): %s" % ", ".join("%s×%d" % (t, n) for t, n in sorted(leftovers.items())))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            obj = dict(results.get(nm, {"(not found)": nm}))
            if "identity" in obj and "cityNames" in obj["identity"]:   # trim the long name list for display
                obj["identity"] = dict(obj["identity"])
                obj["identity"]["cityNames"] = ["...(%d names)..." % len(obj["identity"]["cityNames"])]
            print(json.dumps(obj, indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "civilizations")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d CivilizationInfo JSON files under Assets/Data/civilizations" % len(results))


if __name__ == "__main__":
    main()
