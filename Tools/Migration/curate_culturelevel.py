#!/usr/bin/env python3
"""Curate CultureLevel to the top-down model (#428) — a per-city-level CONFIG entity, NOT a POCO.

The first-pass mapping under-classified it (POCO caveat). Reading the consumers (classification
light-four-classification.json) shows:
- ONE live additive modifier — `iCityDefenseModifier` = the city-defense % for being at this level
  (CvCity.cpp:10184) -> `defense.city.amount.percent`. The top-level `defense` family has TWO members (owner
  ruling 2026-06-14): `amount` = the additive extra-defense percent (this field), and `min` = the floor clamp
  some buildings raise above 0 (you must contend with at least e.g. 25%) — `min` is authored at the Building
  pass (BuildingInfo.iMinDefense), not here.
- an ENABLER/conditioner: it gates buildings via `BuildingInfo.PrereqCultureLevel` -> `enables.buildings`
  (wired in store.PREREQ_FIELDS; CultureLevel is the conditioner you must HAVE, so it inverts onto the level).
- everything else is NON-ADDITIVE per-level intrinsic -> `identity` (section 3: caps/overrides are not cascade
  families): `iCityRadius` (REPLACE/override workable radius), the 4 wonder CAPS (max world/team/national[/OCC]).
- `PrereqGameOption` -> a load-stable per-game availability gate (also computes the runtime active level,
  CvGlobals.cpp:3587) -> `loadPrune.onGameOptions` (the enabler-spec §6/§12 auxiliary section — named as its own
  section, NOT parked in identity; 2026-06-15 retrofit).
- `m_iLevel` is RUNTIME-derived (no XML tag) -> not emitted. `ReplacementID`/`ReplacementCondition` are the
  CvInfoReplacements conditional-whole-Info edge (handled by store): the base CULTURELEVEL_POOR carries
  `replacedBy` -> CULTURELEVEL_ALT_POOR under GAMEOPTION_CULTURE_1_CITY_TILE_FOUNDING; ALT_POOR curates as its
  own record (the `replaces` reverse derives cold-path).

SpeedThresholds COLLAPSE (owner ruling 2026-06-14): the per-speed table is REDUNDANT precomputation of
`base(Normal) * GameSpeed.iSpeedPercent/100` — the values are identical to iSpeedPercent and the GameSpeed XML
even notes the coupling ("If you remove any of these you need to update CultureLevelInfos.xml"). So the scale
"belongs in GameSpeed" (owner): GameSpeed carries it as its master pace percentage, and CultureLevel keeps ONLY
the NORMAL base count. The reader derives `threshold(level, speed) = base * GameSpeed.speed.world.percent / 100`.
NB (2026-06-15): GameSpeed was COLLAPSED to a single `speed.world.percent` (info #1) — the earlier separate
`cultureThreshold` member is GONE; the derivation reads `GameSpeed.speed` (same value, iSpeedPercent). 0 levels
break the ratio (all geometric — verified, override count 0); a hypothetical non-geometric level is kept
losslessly as a per-speed override. Raw culture points, NOT x100 (the call site x100s, e.g. CvCity.cpp:13008).

  python3 curate_culturelevel.py --sample CULTURELEVEL_FLEDGLING
  python3 curate_culturelevel.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import fold_text_to_identity
from store import Store, REPO

# The per-speed culture scale IS GameSpeed.iSpeedPercent (verified identical to the XML SpeedThresholds, and the
# GameSpeed XML notes the coupling). It now lives on GameSpeed as `cultureThreshold.world.percent`
# (curate_gamespeed.py); CultureLevel keeps only the NORMAL base. This map (= iSpeedPercent, relative to
# Normal=100) is used ONLY to detect any non-geometric level (an override -> kept losslessly). 0 expected.
SPEED_PERCENT = OrderedDict([
    ("GAMESPEED_ETERNITY", 1000), ("GAMESPEED_EONS", 800), ("GAMESPEED_SNAIL", 600),
    ("GAMESPEED_MARATHON", 400), ("GAMESPEED_EPIC", 300), ("GAMESPEED_LONG", 200),
    ("GAMESPEED_NORMAL", 100), ("GAMESPEED_BLITZ", 50), ("GAMESPEED_ULTRAFAST", 25),
])
BASE_SPEED = "GAMESPEED_NORMAL"

# Per-city wonder COUNT caps -> the declarative `allowed` ceiling (owner 2026-06-17): a culture level grants a
# city an allowance of each wonder CATEGORY. It is part of the ENABLING (not identity) — the cap gates how many
# of a category a city may hold. Keyed by the wonder-category DISCRIMINATOR (owner: worldWonders/teamWonders/
# nationalWonders + the reserved totalWonders); city scope is implicit (a culture level is per-city). The new
# canDoStuff gate enforces it (a category building drops from the city frontier when its count hits the allowance)
# and owns ignoring it under NO_WONDER_LIMIT / CHALLENGE_ONE_CITY (engine, never the parser). `iMaxNationalWondersOCC`
# is DROPPED: One City Challenge is NOT feasible in this mod (owner 2026-07-01), so its per-level DOUBLED national
# cap (the OCC override at CvCity.cpp:2172 — NB: OCC swaps to a higher number, it does not turn limits off) is out
# of scope and intentionally not carried. enabler-spec §5/§13.7.
WONDER_CAPS = OrderedDict([
    ("iMaxWorldWonders", "worldWonders"), ("iMaxTeamWonders", "teamWonders"),
    ("iMaxNationalWonders", "nationalWonders"),
])


def _collapse_thresholds(rec):
    """SpeedThresholds is REDUNDANT precomputation of base(Normal) * GameSpeed.iSpeedPercent/100, so drop the
    table and keep only the NORMAL base (the scale lives on GameSpeed.cultureThreshold). Returns the scalar base,
    or {base, overrides} if any speed breaks the iSpeedPercent ratio (lossless safety; 0 expected)."""
    node = rec.find("SpeedThresholds")
    if node is None:
        return None
    vals = {}
    for st in node.findall("SpeedThreshold"):
        gs, th = engine.text(st.find("GameSpeedType")), engine.text(st.find("iThreshold"))
        if gs and engine.is_int(th):
            vals[gs] = int(th)
    if not vals:
        return None
    base = vals.get(BASE_SPEED, 0)
    overrides = OrderedDict()
    for gs, sp in SPEED_PERCENT.items():
        if gs in vals and vals[gs] != int(round(base * sp / 100.0)):
            overrides[gs] = vals[gs]
    return OrderedDict([("base", base), ("overrides", overrides)]) if overrides else base


def _replaced_by(typ, store):
    """The CvInfoReplacements conditional-whole-Info edge (store-detected): base -> {cultureLevel, onGameOption}."""
    r = store.replacement_of(typ)
    if not r:
        return None
    out = OrderedDict([("cultureLevel", r["replacement"])])
    cond = r.get("condition")
    if cond is not None:
        has = cond.find("Has")
        gid = engine.text(has.find("ID")) if has is not None else ""
        if gid:
            out["onGameOption"] = gid
    return out


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    desc = engine.text(rec.find("Description"))
    if desc:
        out["description"] = desc
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    allowed = OrderedDict()                                  # per-city wonder-category COUNT caps (the enabling ceiling)
    for tag, key in WONDER_CAPS.items():
        v = engine.text(rec.find(tag))
        if engine.is_int(v):
            allowed[key] = int(v)
    if allowed:
        out["allowed"] = allowed
    cdm = engine.text(rec.find("iCityDefenseModifier"))
    if engine.is_int(cdm) and int(cdm) != 0:                 # additive modifier (drop the 0-identity, lossless).
        # top-level `defense` family, member `amount` = additive extra-defense %. The sibling `min` member
        # (the floor clamp some buildings raise above 0, e.g. >=25%) is authored at the Building pass (iMinDefense).
        out["defense"] = {"city": {"amount": {"percent": int(cdm)}}}
    pgo = engine.text(rec.find("PrereqGameOption"))
    if pgo and pgo != "NONE":                                # load-stable game-option gate -> loadPrune (§6/§12)
        out["loadPrune"] = OrderedDict([("onGameOptions", [pgo])])
    rb = _replaced_by(typ, store)
    if rb:
        out["replacedBy"] = rb
    identity = OrderedDict()
    rad = engine.text(rec.find("iCityRadius"))
    if engine.is_int(rad):                                   # REPLACE/override (workable radius), not additive
        identity["cityRadius"] = int(rad)
    thr = _collapse_thresholds(rec)
    if thr is not None:
        identity["cultureThreshold"] = thr
    if identity:
        out["identity"] = identity
    fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    s = Store()
    table = s.table("CultureLevelInfo")
    results, overrides = OrderedDict(), 0
    for typ, rec in table.items():
        obj = curate(typ, rec, s)
        results[typ] = obj
        ct = (obj.get("identity") or {}).get("cultureThreshold")
        overrides += len(ct.get("overrides", {})) if isinstance(ct, dict) else 0
    print("CultureLevelInfo curated: %d" % len(results))
    print("  with enables:     %d" % sum(1 for o in results.values() if "enables" in o))
    print("  with allowed:     %d" % sum(1 for o in results.values() if "allowed" in o))
    print("  with defense:     %d" % sum(1 for o in results.values() if "defense" in o))
    print("  with replacedBy:  %d" % sum(1 for o in results.values() if "replacedBy" in o))
    print("  threshold overrides (~0 expected, purely geometric): %d" % overrides)
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "culturelevels")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        for typ, obj in results.items():
            with open(os.path.join(out_dir, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d CultureLevelInfo JSON files under Assets/Data/culturelevels" % len(results))


if __name__ == "__main__":
    main()
