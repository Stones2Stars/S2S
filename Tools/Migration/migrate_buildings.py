#!/usr/bin/env python3
"""
#428 XML -> JSON building migration converter (offline).

Reads the building XML, emits whole-entity channel-object JSON. The XML already carries Type-strings,
so there's no enum-reverse problem (the reason we do this offline, not via a C++ writeJson).

This is the FIRST cut: faithful whole-entity + the modifier channel-object + de-assed maps + sections.
Cross-entity inversions (Tech*/Bonus* moving onto their conditioner) are a LATER pass -- here those maps
stay on the building so nothing is lost; we reshape, we don't yet relocate.

Run:  python3 Tools/Migration/migrate_buildings.py --sample 2      # print 2 buildings, write nothing
      python3 Tools/Migration/migrate_buildings.py --write          # write all to Assets/Data/buildings/<era>/
"""
import argparse
import glob
import json
import os
import xml.etree.ElementTree as ET

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
XML_DIR = os.path.join(REPO, "Assets", "XML", "Buildings")
OUT_DIR = os.path.join(REPO, "Assets", "Data", "buildings")

# ---- modifier channel map: XML tag -> (scope, channel, kind) ---------------------------------------
# Per the spec, scope is city/area/player; kind is flat/percent/enabler. Yield/commerce keys are short.
FLAT = {
    "iHappiness": ("city", "happiness"), "iHealth": ("city", "health"),
    "iAreaHappiness": ("area", "happiness"), "iAreaHealth": ("area", "health"),
    "iGlobalHappiness": ("player", "happiness"), "iGlobalHealth": ("player", "health"),
    "iGreatPeopleRateChange": ("city", "greatPeopleRate"), "iExperience": ("city", "unitExperience"),
    "iFreeSpecialist": ("city", "freeSpecialist"), "iHealRateChange": ("city", "healRate"),
    "iTradeRoutes": ("city", "tradeRoutes"), "iCoastalTradeRoutes": ("city", "coastalTradeRoutes"),
    "iGlobalTradeRoutes": ("player", "tradeRoutes"), "iWorldTradeRoutes": ("team", "tradeRoutes"),
    "iAirlift": ("city", "airlift"), "iAirUnitCapacity": ("city", "airUnitCapacity"),
    "iStateReligionHappiness": ("city", "stateReligionHappiness"), "iMinDefense": ("city", "minDefense"),
    "iAreaFreeSpecialist": ("area", "freeSpecialist"), "iGlobalFreeSpecialist": ("player", "freeSpecialist"),
    "iGlobalExperience": ("player", "unitExperience"), "iFreeTechs": ("player", "freeTechs"),
}
PERCENT = {
    "iMaintenanceModifier": ("city", "maintenance"), "iHurryCostModifier": ("city", "hurryCost"),
    "iHurryAngerModifier": ("city", "hurryAnger"), "iGreatPeopleRateModifier": ("city", "greatPeopleRate"),
    "iFoodKept": ("city", "foodKept"), "iMilitaryProductionModifier": ("city", "militaryProduction"),
    "iSpaceProductionModifier": ("city", "spaceProduction"), "iDefense": ("city", "defense"),
    "iBombardDefense": ("city", "bombardDefense"), "iEspionageDefense": ("city", "espionageDefense"),
    "iAirModifier": ("city", "airDefense"), "iNukeModifier": ("city", "nukeDefense"),
    "iWarWearinessModifier": ("city", "warWeariness"), "iGlobalMaintenanceModifier": ("player", "maintenance"),
    "iAnarchyModifier": ("player", "anarchy"), "iGoldenAgeModifier": ("player", "goldenAge"),
    "iGreatGeneralRateModifier": ("player", "greatGeneralRate"), "iWorkerSpeedModifier": ("player", "workerSpeed"),
    "iInflationModifier": ("player", "inflation"), "iAllCityDefense": ("player", "cityDefense"),
}
ENABLER = {
    "bNoUnhappiness": ("city", "noUnhappiness"), "bNoUnhealthyPopulation": ("city", "unhealthyPopulation"),
    "bBuildingOnlyHealthy": ("city", "buildingUnhealth"), "bNukeImmune": ("city", "nukeImmunity"),
    "bProvidesFreshWater": ("city", "freshWater"), "bAreaCleanPower": ("area", "cleanPower"),
    "bBorderObstacle": ("area", "borderObstacle"), "bZoneOfControl": ("city", "zoneOfControl"),
    "bForceAllTradeRoutes": ("city", "forceAllTradeRoutes"),
}

YIELDS = ["food", "production", "commerce"]
COMMERCES = ["gold", "research", "culture", "espionage"]

# coarse section routing for tags we don't specifically reshape
ART_PREFIXES = ("ArtDefine", "Button", "Movie", "Sound")
ART_TAGS = {"ArtDefineTag", "MovieDefineTag", "ConstructSound", "Advisor"}
# NOTE: only real Prereq* tags are prerequisites. BonusYieldChanges / BonusHealthChanges /
# VicinityBonusYieldChanges are conditional MODIFIERS (the conditioner-inversion candidates -> bonus),
# NOT prereqs -- they must not match here. PrereqBonus / PrereqVicinityBonus still match via "Prereq".
PREREQ_SUBSTR = ("Prereq",)
PREREQ_EXACT = {"ConstructCondition", "TrainCondition"}  # BoolExpr gates (#195 territory), not identity
COST_SUBSTR = ("Cost",)

# ---- cross-entity INVERSIONS (blueprint §2): building tag -> (destEntity, shape, valueNames) ----------
# Each of these is a map keyed by the DESTINATION Type (TechType/BonusType/...). We strip it off the
# building and HOME it onto the destination as buildingBoosts.{BUILDING}.<shape>. valueNames names a
# yield/commerce array; None keeps the value as-is (scalar or de-assed). This is the whole point of the
# migration -- the building stops referencing tech/bonus/etc.; the conditioner owns the conditional effect.
INVERSION = {
    "TechYieldChanges":            ("tech", "yield", YIELDS),
    "TechYieldModifiers":          ("tech", "yieldModifier", YIELDS),
    "TechCommerceChanges":         ("tech", "commerce", COMMERCES),
    "TechCommerceModifiers":       ("tech", "commerceModifier", COMMERCES),
    "TechHappinessChanges":        ("tech", "happiness", None),
    "TechHealthChanges":           ("tech", "health", None),
    "TechSpecialistChanges":       ("tech", "freeSpecialists", None),
    "BonusHealthChanges":          ("bonus", "health", None),
    "BonusHappinessChanges":       ("bonus", "happiness", None),
    "BonusYieldChanges":           ("bonus", "yield", YIELDS),
    "BonusYieldModifiers":         ("bonus", "yieldModifier", YIELDS),
    "BonusCommerceModifiers":      ("bonus", "commerceModifier", COMMERCES),
    "BonusCommercePercentChanges": ("bonus", "commercePercent", COMMERCES),
    "VicinityBonusYieldChanges":   ("bonus", "vicinityYield", YIELDS),
    "BonusProductionModifiers":    ("bonus", "productionModifier", None),
    "BonusDefenseChanges":         ("bonus", "defense", None),
    "BonusAidModifiers":           ("bonus", "aidModifier", None),
    "ImprovementFreeSpecialists":  ("improvement", "freeSpecialists", None),
    "ImprovementYieldChanges":     ("improvement", "yield", YIELDS),
    "GlobalImprovementYieldChanges": ("improvement", "yield", YIELDS),
    "TerrainYieldChanges":         ("terrain", "yield", YIELDS),
    "ReligionChanges":             ("religion", "religionSpread", None),
    "GlobalBuildingExtraCommerces": ("building", "effectiveCommerce", COMMERCES),
    "BuildingHappinessChanges":    ("building", "effectiveHappiness", None),
    "BuildingProductionModifiers": ("building", "effectiveBuildCost", None),
    "GlobalBuildingProductionModifiers": ("building", "effectiveBuildCost", None),
    "GlobalBuildingCostModifiers": ("building", "effectiveCost", None),
}
# Owner decision #3 (blueprint §7): UnitProductionModifiers (building->unit) stays building-side keyed by
# Unit -- the conditioning is weak ("building present -> unit cheaper"). NOT inverted here.


def strip_ns(root):
    """Civ4 XML declares a default xmlns, so ET returns '{x-schema:...}Tag'. Flatten to local names."""
    for el in root.iter():
        if isinstance(el.tag, str) and "}" in el.tag:
            el.tag = el.tag.split("}", 1)[1]
    return root


def text(e):
    return (e.text or "").strip()


def is_int(s):
    try:
        int(s); return True
    except (ValueError, TypeError):
        return False


def child_tags(e):
    return [c.tag for c in e]


def yield_array_to_named(elem, names):
    """<YieldChanges><iYield>1</iYield>...</> -> {food:1,...} (positional, short keys, zeros dropped)."""
    vals = [int(text(c)) for c in elem if is_int(text(c))]
    out = {}
    for i, v in enumerate(vals):
        if v != 0 and i < len(names):
            out[names[i]] = v
    return out


def generic(elem):
    """Faithful XML->JSON for anything we don't specifically reshape (de-asses obvious key/value maps)."""
    kids = list(elem)
    if not kids:
        t = text(elem)
        return int(t) if is_int(t) else t
    # uniform repeated child => list
    tags = [k.tag for k in kids]
    if len(set(tags)) == 1:
        return [generic(k) for k in kids]
    return {k.tag: generic(k) for k in kids}


def keyed_entries(parent, names):
    """<Foo><FooChange><XType>K</XType><value...></FooChange>...> -> [(K, value), ...].

    The first child whose tag ends in 'Type' is the destination key; the remainder is the value
    (a named yield/commerce array when `names` is given, a scalar, or a de-assed object)."""
    out = []
    for entry in list(parent):
        key, rest = None, []
        for c in entry:
            if key is None and c.tag.endswith("Type"):
                key = text(c)
            else:
                rest.append(c)
        if not key:
            continue
        if len(rest) == 1 and list(rest[0]):
            val = yield_array_to_named(rest[0], names) if names else generic(rest[0])
        elif len(rest) == 1:
            tx = text(rest[0])
            val = int(tx) if is_int(tx) else tx
        else:
            val = {c.tag: generic(c) for c in rest}
        if val not in (None, {}, []):
            out.append((key, val))
    return out


def convert(elem, inversions):
    b = {"type": None}
    modifiers, prereqs, cost, art, identity = {}, {}, {}, {}, {}

    def put_mod(scope, channel, kind, value):
        modifiers.setdefault(scope, {}).setdefault(channel, {})[kind] = value

    btype = elem.find("Type")
    btype = text(btype) if btype is not None else None

    def home(dest_kind, dest_type, shape, value):
        """Deposit a conditional effect onto the DESTINATION entity (the inversion)."""
        d = inversions.setdefault(dest_kind, {}).setdefault(dest_type, {}).setdefault(btype, {})
        d[shape] = value

    for c in elem:
        tag, t = c.tag, text(c)
        if tag == "Type":
            b["type"] = t
        elif tag in INVERSION:
            dest_kind, shape, names = INVERSION[tag]
            for dest_type, val in keyed_entries(c, names):
                home(dest_kind, dest_type, shape, val)
        elif tag in FLAT and is_int(t) and int(t) != 0:
            s, ch = FLAT[tag]; put_mod(s, ch, "flat", int(t))
        elif tag in PERCENT and is_int(t) and int(t) != 0:
            s, ch = PERCENT[tag]; put_mod(s, ch, "percent", int(t))
        elif tag in ENABLER and is_int(t) and int(t) != 0:
            s, ch = ENABLER[tag]; put_mod(s, ch, "enabler", True)
        elif tag in ("YieldChanges", "YieldModifiers"):
            named = yield_array_to_named(c, YIELDS)
            if named:
                put_mod("city", "yield", "flat" if tag == "YieldChanges" else "percent", named)
        elif tag in ("CommerceChanges", "CommerceModifiers"):
            named = yield_array_to_named(c, COMMERCES)
            if named:
                put_mod("city", "commerce", "flat" if tag == "CommerceChanges" else "percent", named)
        elif tag in ART_TAGS or tag.startswith(ART_PREFIXES):
            if t:
                art[tag] = t
        elif any(s in tag for s in PREREQ_SUBSTR) or tag in PREREQ_EXACT:
            prereqs[tag] = generic(c)
        elif any(s in tag for s in COST_SUBSTR):
            cost[tag] = int(t) if is_int(t) else generic(c)
        else:
            if list(c) or t:
                identity[tag] = generic(c)

    out = {"type": b["type"]}
    if modifiers: out["modifiers"] = modifiers
    if prereqs:   out["prerequisites"] = prereqs
    if cost:      out["cost"] = cost
    if art:       out["art"] = art
    if identity:  out["identity"] = identity
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", type=int, default=0, help="print N buildings, write nothing")
    ap.add_argument("--audit", action="store_true", help="tag frequency per section (find unclassified)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()

    buildings = []
    inversions = {}
    for path in sorted(glob.glob(os.path.join(XML_DIR, "*CIV4BuildingInfos.xml"))):
        try:
            root = strip_ns(ET.parse(path).getroot())
        except ET.ParseError as e:
            print("PARSE FAIL", path, e); continue
        for info in root.iter("BuildingInfo"):
            j = convert(info, inversions)
            if j.get("type"):
                buildings.append((j["type"], j, os.path.basename(path)))

    print("parsed %d buildings from %d files" % (len(buildings), len(glob.glob(os.path.join(XML_DIR, "*CIV4BuildingInfos.xml")))))
    print("\n=== inversions HOMED onto destination entities (building cross-refs relocated) ===")
    for dest_kind in sorted(inversions):
        targets = inversions[dest_kind]
        deposits = sum(len(b) for b in targets.values())
        print("  %-12s %4d target %-12s receiving %5d building deposits"
              % (dest_kind, len(targets), "entities", deposits))
    # show one fully-homed destination so the shape is visible
    if inversions.get("bonus"):
        sample_t = sorted(inversions["bonus"], key=lambda k: -len(inversions["bonus"][k]))[0]
        print("\n--- %s.buildingBoosts (homed from %d buildings) ---" % (sample_t, len(inversions["bonus"][sample_t])))
        shown = {k: inversions["bonus"][sample_t][k] for k in list(inversions["bonus"][sample_t])[:4]}
        print(json.dumps(shown, indent=2))
    if args.sample:
        for typ, j, src in buildings[:args.sample]:
            print("\n--- %s (%s) ---" % (typ, src))
            print(json.dumps(j, indent=2))
    if args.audit:
        from collections import Counter
        sect = {"identity": Counter(), "prerequisites": Counter(), "cost": Counter(), "art": Counter()}
        chans = Counter()
        for _, j, _ in buildings:
            for s in sect:
                for k in (j.get(s) or {}):
                    sect[s][k] += 1
            for scope, channels in (j.get("modifiers") or {}).items():
                for ch, kinds in channels.items():
                    for kind in kinds:
                        chans["%s.%s.%s" % (scope, ch, kind)] += 1
        print("\n=== modifiers (scope.channel.kind) x%d ===" % sum(chans.values()))
        for k, n in chans.most_common():
            print("  %5d  %s" % (n, k))
        for s in ("identity", "prerequisites", "cost", "art"):
            print("\n=== %s tags (%d distinct) -- identity is the UNCLASSIFIED catch-all ===" % (s, len(sect[s])))
            for k, n in sect[s].most_common():
                print("  %5d  %s" % (n, k))


if __name__ == "__main__":
    main()
