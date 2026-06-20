#!/usr/bin/env python3
"""Curate Civic (#428) — the heavy "decades of cruft" entity (175 civics, ~95 real top-level fields). BESPOKE.
A civic is an EMPIRE-wide source: every modifier deposits through CvPlayer::processCivics into player-level
accumulators, so nearly everything is `empire` scope (the first-pass mapping's player/city scopes were
systematically wrong — corrected against the consumers). Field dispositions verified by the classify-civic
workflow (9 field-slice agents + dead/double-author auditors + adversarial verify) against CvCivicInfo.{h,cpp}
+ the CvCity/CvPlayer/CvTeam consumers. Owner rulings (2026-06-14) drive the four structural calls:

- REVOLUTION (RevolutionDCM, ~14 iRev*/fRev*): KEPT, faithful, under a `revolution` family. The logic lives in
  Python (Assets/Python/Revolution/) only because the original modder didn't know C++ — the values are live
  data feeding logic slated to move Python->C++ (Python = presentation only). NOT cruft; carry every field
  verbatim (f* are floats).
- POLICIES: the ~17 boolean policy/capability flags (bStateReligion/bNoForeignTrade/bAllowInquisitions/...) go
  in a dedicated empire-scope `policies` section (policies.{name}: true). They toggle player STATE, not an
  additive number and not entity-availability. Allow/Disallow inquisition are a signed pair (both kept; the
  +1/-1 empire-count semantics is a consumer rule).
- STATE-RELIGION: the 5 iStateReligion* effects -> a grouped `stateReligion` family (empire), gated on having a
  state religion. Civic-OWNED; NOT inverted onto any ReligionInfo.
- BonusCommerceModifiers: INVERTED onto the bonus (the bonus is the conditioner) via a new curate_bonus
  BONUS_BOOSTS row -> dropped here, mirroring building/unit/project BonusProductionModifiers.

Other: TechPrereq -> DROP (store inverts to tech.enables.civics). SpecialistYield/CommercePercentChanges ->
DROP (already folded onto the specialist by curate_specialist — double-author). Entity-keyed maps
(Building*/Feature*/Improvement*/Terrain*/Unit*/UnitCombat*) STAY target-keyed on the civic. Capability LISTS
(SpecialistValids/Hurrys/SpecialBuildingNotRequireds) -> enables.{specialists,hurries,specialBuildingsWaived}.
FreeSpecialistCounts -> grants. PropertyManipulators -> empire gated property family. CivicAttitudeChanges ->
diplomacy keyed by civic. iAnarchyLength/Upkeep/CivicOptionType/WeLoveTheKing -> identity. x100/floats faithful.

  python3 curate_civic.py --sample CIVIC_DESPOTISM CIVIC_THEOCRACY
  python3 curate_civic.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
import curate_common as cc
from curate_common import de_i
from store import Store, REPO

YIELDS, COMMERCES = engine.YIELDS, engine.COMMERCES

# --- scalar modifier families: tag -> (family, scope, member, unit). member "" => single-concept. ---
SCALAR = {
    # maintenance / upkeep / trade / hurry / workrate
    "iDistanceMaintenanceModifier":    ("maintenance", "empire", "distance",    "percent"),
    "iNumCitiesMaintenanceModifier":   ("maintenance", "empire", "numCities",   "percent"),
    "iHomeAreaMaintenanceModifier":    ("maintenance", "empire", "homeArea",    "percent"),
    "iOtherAreaMaintenanceModifier":   ("maintenance", "empire", "otherArea",   "percent"),
    "iCorporationMaintenanceModifier": ("maintenance", "empire", "corporation", "percent"),
    "iInflation":                      ("upkeep", "empire", "inflation",   "percent"),
    "iCivilianUnitUpkeepMod":          ("upkeep", "empire", "unitCivilian","percent"),
    "iMilitaryUnitUpkeepMod":          ("upkeep", "empire", "unitMilitary","percent"),
    "iDistantUnitSupportCostModifier": ("upkeep", "empire", "supply",      "percent"),
    "iFreeUnitUpkeepMilitary":         ("upkeep", "empire", "freeMilitary","flat"),
    "iFreeUnitUpkeepMilitaryPopPercent":("upkeep","empire", "freeMilitary","perPopulation"),
    "iFreeUnitUpkeepCivilian":         ("upkeep", "empire", "freeCivilian","flat"),
    "iFreeUnitUpkeepCivilianPopPercent":("upkeep","empire", "freeCivilian","perPopulation"),
    "iTradeRoutes":                    ("tradeRoutes", "empire", "",           "flat"),
    "iForeignTradeRouteModifier":      ("tradeRoutes", "empire", "foreign",    "percent"),
    "iSharedCivicTradeRouteModifier":  ("tradeRoutes", "empire", "sharedCivic","percent"),
    "iHurryCostModifier":              ("hurry", "empire", "cost",      "percent"),
    "iHurryInflationModifier":         ("hurry", "empire", "inflation", "percent"),
    "iWorkerSpeedModifier":            ("workRate", "empire", "", "percent"),
    "iImprovementUpgradeRateModifier": ("improvementUpgradeRate", "empire", "", "percent"),
    # military / great-people
    "iMilitaryProductionModifier":     ("buildRate", "empire", "military", "percent"),
    "iExtraCityDefense":               ("combat", "empire", "cityDefense",        "percent"),
    "iNationalCaptureProbabilityModifier": ("combat", "empire", "captureProbability", "percent"),
    "iNationalCaptureResistanceModifier":  ("combat", "empire", "captureResistance",  "percent"),
    "iFreeExperience":                 ("experience", "empire", "",        "flat"),
    "iExpInBorderModifier":            ("experience", "empire", "inBorder","percent"),
    "iGreatGeneralRateModifier":       ("greatGeneralRate", "empire", "",        "percent"),
    "iDomesticGreatGeneralRateModifier":("greatGeneralRate","empire", "domestic","percent"),
    "iGreatPeopleRateModifier":        ("greatPeopleRate", "empire", "", "percent"),
    "iMaxConscript":                   ("conscript", "empire", "", "flat"),
    "iFreeSpecialist":                 ("freeSpecialists", "empire", "", "any"),
    # wellbeing
    "iCivicHappiness":                 ("happiness", "empire", "",            "flat"),
    "iLargestCityHappiness":           ("happiness", "empire", "largestCity", "flat"),
    "iLandmarkHappiness":              ("happiness", "empire", "landmark",    "flat"),
    "iNonStateReligionHappiness":      ("happiness", "empire", "nonStateReligion", "flat"),
    "iCityLimit":                      ("happiness", "empire", "cityLimit",    "flat"),
    "iCityOverLimitUnhappy":           ("happiness", "empire", "cityOverLimit","flat"),
    "iForeignerUnhappyPercent":        ("happiness", "empire", "foreignerUnhappy","percent"),
    "iTaxRateUnhappiness":             ("happiness", "empire", "taxRate",      "percent"),
    "iCivicPercentAnger":              ("happiness", "empire", "civicAnger",   "percent"),
    "iHappyPerMilitaryUnit":           ("happiness", "empire", "perMilitaryUnit", "perMilitaryUnit"),
    "iExtraHealth":                    ("health", "empire", "", "flat"),
    "iPopulationgrowthratepercentage": ("growth", "empire", "", "percent"),
    "iWarWearinessModifier":           ("diplomacy", "empire", "warWeariness", "percent"),
    "iAttitudeChange":                 ("diplomacy", "empire", "attitude",     "flat"),
    "iAttitudeShareMod":               ("diplomacy", "empire", "attitudeShare","flat"),
    # revolution (RevolutionDCM — kept faithful; f* are floats)
    "iRevIdxLocal":                    ("revolution", "empire", "local",       "flat"),
    "iRevIdxNational":                 ("revolution", "empire", "national",    "flat"),
    "iRevIdxDistanceModifier":         ("revolution", "empire", "distanceModifier", "percent"),
    "fRevIdxDistanceMod":              ("revolution", "empire", "distanceMod",       "flat"),
    "iRevIdxHolyCityGood":             ("revolution", "empire", "holyCityGood","flat"),
    "iRevIdxHolyCityBad":              ("revolution", "empire", "holyCityBad", "flat"),
    # iRevIdxSwitchTo is NOT here — it's a one-time BURST on civic-switch, not a continuous modifier, so it goes
    # to `grants` (owner 2026-06-15: revolting to a civic raises future revolution chance; grants is the home for
    # one-time pulses, the same shape the outcomes system will use for one-time yields). Handled in curate().
    "iRevLaborFreedom":                ("revolution", "empire", "laborFreedom","flat"),
    "iRevReligiousFreedom":            ("revolution", "empire", "religiousFreedom", "flat"),
    "iRevEnvironmentalProtection":     ("revolution", "empire", "environmentalProtection", "flat"),
    "iRevDemocracyLevel":              ("revolution", "empire", "democracyLevel", "flat"),
    "fRevIdxNationalityMod":           ("revolution", "empire", "nationalityMod",  "percent"),
    "fRevIdxBadReligionMod":           ("revolution", "empire", "badReligionMod",  "percent"),
    "fRevIdxGoodReligionMod":          ("revolution", "empire", "goodReligionMod", "percent"),
    "fRevViolentMod":                  ("revolution", "empire", "violentMod",      "percent"),
}

# --- state-religion grouped family: tag -> (member, unit). All empire, gated on having a state religion. ---
STATE_RELIGION = {
    "iStateReligionHappiness":                ("happiness",          "flat"),
    "iStateReligionGreatPeopleRateModifier":  ("greatPeopleRate",    "percent"),
    "iStateReligionUnitProductionModifier":   ("unitProduction",     "percent"),
    "iStateReligionBuildingProductionModifier":("buildingProduction","percent"),
    "iStateReligionFreeExperience":           ("freeExperience",     "flat"),
}

# --- flat (no-key) split arrays: tag -> (scope, member, unit, valueKeys). member "" => no condition. ---
SPLIT_ARRAY = {
    "YieldModifiers":          ("empire", "",           "percent",      YIELDS),
    "CapitalYieldModifiers":   ("empire", "capital",    "percent",      YIELDS),
    "TradeYieldModifiers":     ("empire", "tradeRoute", "percent",      YIELDS),
    "CommerceModifiers":       ("empire", "",           "percent",      COMMERCES),
    "CapitalCommerceModifiers":("empire", "capital",    "percent",      COMMERCES),
    "SpecialistExtraCommerces":("empire", "specialist", "perSpecialist",COMMERCES),
}

# --- entity-keyed (target-keyed) maps: tag -> (family, scope, targetType, unit, valueKeys|None). ---
KEYED = {
    "BuildingHappinessChanges":     ("happiness",  "empire", "buildings",   "flat",    None),
    "BuildingHealthChanges":        ("health",     "empire", "buildings",   "flat",    None),
    "BuildingProductionModifiers":  ("buildRate",  "empire", "buildings",   "percent", None),
    "BuildingCommerceModifiers":    (None,         "empire", "buildings",   "percent", COMMERCES),  # split commerce
    "FeatureHappinessChanges":      ("happiness",  "empire", "features",    "flat",    None),
    "ImprovementYieldChanges":      (None,         "empire", "improvements","flat",    YIELDS),      # split yield
    "TerrainYieldChanges":          (None,         "empire", "terrains",    "flat",    YIELDS),      # split yield
    "UnitProductionModifiers":      ("buildRate", "empire", "units",      "percent", None),
    "UnitCombatProductionModifiers":("buildRate", "empire", "unitCombats","percent", None),
    "CivicAttitudeChanges":         ("diplomacy",  "empire", "civics",      "flat",    None),
}
LANDMARK_YIELD = "LandmarkYieldChanges"   # flat split yield gated on landmark improvements (condition member)

# --- boolean policy/capability flags -> policies.{name}: true. ---
POLICIES = {
    "bStateReligion": "stateReligion", "bNoForeignTrade": "noForeignTrade", "bNoCorporations": "noCorporations",
    "bNoForeignCorporations": "noForeignCorporations", "bFreeSpeech": "freeSpeech", "bFixedBorders": "fixedBorders",
    "bMilitaryFoodProduction": "militaryFoodProduction", "bBuildingOnlyHealthy": "buildingOnlyHealthy",
    "bNoUnhealthyPopulation": "noUnhealthyPopulation", "bNoCapitalUnhappiness": "noCapitalUnhappiness",
    "bNoLandmarkAnger": "noLandmarkAnger", "bCommunism": "communism", "bCanDoElection": "canDoElection",
    "bUpgradeAnywhere": "upgradeAnywhere", "bNoNonStateReligionSpread": "noNonStateReligionSpread",
    "bAllowInquisitions": "allowInquisitions", "bDisallowInquisitions": "disallowInquisitions",
}
# --- capability LIST fields -> enables.{key}; each entry is <XType> + a bool value-element. ---
ENABLE_LISTS = {"SpecialistValids": "specialists", "Hurrys": "hurries", "SpecialBuildingNotRequireds": "specialBuildingsWaived"}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help", "Strategy": "strategy"}
IDENTITY = {"Upkeep": "upkeepLevel", "CivicOptionType": "civicOption", "iAnarchyLength": "anarchyLength",
            "WeLoveTheKing": "weLoveTheKing"}
# inverts onto the bonus (curate_bonus BONUS_BOOSTS); dropped here. Plus prereq + double-author + dead caches.
DROP = {"TechPrereq", "BonusCommerceModifiers", "SpecialistCommercePercentChanges", "SpecialistYieldPercentChanges",
        "Categories", "isAnyImprovementYieldChange"}
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "espionage", "yield",
                "happiness", "health", "growth", "experience", "greatPeopleRate", "greatGeneralRate",
                "freeSpecialists", "conscript", "combat", "unitProduction", "maintenance", "upkeep",
                "tradeRoutes", "hurry", "workRate", "improvementUpgradeRate", "diplomacy", "stateReligion",
                "revolution"]


def _category(rec):
    """Folder = the civic's CivicOption category, short form (CIVICOPTION_GOVERNMENT -> 'government'). The full
    Type stays the identity.civicOption reference; the folder is the short name (mirrors the era-folder split)."""
    co = engine.text(rec.find("CivicOptionType"))
    return co.replace("CIVICOPTION_", "").lower() if co else ""


def _num(t):
    """faithful scalar: int if integral, else float (the fRev* fields), else None."""
    if engine.is_int(t):
        return int(t)
    try:
        return float(t)
    except (TypeError, ValueError):
        return None


def _put(fam, family, scope, member, unit, val):
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    node[unit] = val


def _keyed_entries(node, keys):
    """<Foo><FooChange><XType>K</XType><value...></FooChange>...> -> {K: int|float|named_array}."""
    out = OrderedDict()
    for entry in list(node):
        k, vals = None, []
        for c in entry:
            if k is None and c.tag.endswith("Type"):
                k = engine.text(c)
            else:
                vals.append(c)
        if not k:
            continue
        if len(vals) == 1 and list(vals[0]):
            val = engine.named_array(vals[0], keys) if keys else engine.generic(vals[0])
        elif len(vals) == 1:
            val = _num(engine.text(vals[0]))
        else:                                              # multiple children: take the numeric value, drop
            val = next((n for n in (_num(engine.text(c)) for c in vals) if n is not None), None)
            # cosmetic label siblings (e.g. CivicAttitudeChange's <Description> reason text)
        if val not in (None, {}, [], ""):
            out[k] = val
    return out


def _enable_list(node):
    """capability list: <Foo><FooEntry><XType>K</XType><bFlag>1</bFlag></FooEntry>...> -> [K where bFlag set]."""
    out = []
    for entry in list(node):
        key, on = None, True
        for c in entry:
            if key is None and c.tag.endswith("Type"):
                key = engine.text(c)
            elif c.tag[:1] == "b":
                on = engine.text(c) in ("1", "true", "True")
        if key and on:
            out.append(key)
    return out


def _properties(node, props):
    """Civic PropertySources -> v3 property deposits via the shared converter (engine.property_source_v3 — the
    STANDARD, owner 2026-06-15). Uniform with the Property pass: <PROPERTY>.<scope>.<unit> where unit is flat/
    percent (+ `per` when attribute-scaled). scope from GameObjectType (city/plot), NOT a hardcoded empire;
    RELATION_ASSOCIATED (the civic's effect on its own cities) is the cascade default, dropped by the converter."""
    for src in node.findall("PropertySource"):
        conv = engine.property_source_v3(src)
        if conv is None:
            continue
        prop, scope, unit, value = conv
        props.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())[unit] = value


def curate(typ, rec, store):
    text, fam, props, policies, enables, grants, art_blocks, identity, ai = {}, {}, {}, {}, {}, {}, {}, {}, {}
    leftover = []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag == "Type" or tag in DROP:
            continue
        elif tag in TEXT:
            if t:
                text[TEXT[tag]] = t
        elif tag in SCALAR:
            v = _num(t)
            if v not in (None, 0, 0.0):
                family, scope, member, unit = SCALAR[tag]
                _put(fam, family, scope, member, unit, v)
        elif tag in STATE_RELIGION:
            v = _num(t)
            if v not in (None, 0, 0.0):
                member, unit = STATE_RELIGION[tag]
                _put(fam, "stateReligion", "empire", member, unit, v)
        elif tag in SPLIT_ARRAY:
            scope, member, unit, keys = SPLIT_ARRAY[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split)
                _put(fam, ident, scope, member, unit, v)
        elif tag in KEYED:
            family, scope, tt, unit, keys = KEYED[tag]
            for target, val in _keyed_entries(c, keys).items():
                if keys:                                           # split: each named member is its own family
                    for ident, v in (val.items() if isinstance(val, dict) else []):
                        (fam.setdefault(ident, {}).setdefault(scope, {}).setdefault(tt, {})
                         .setdefault(target, {}))[unit] = v
                else:
                    (fam.setdefault(family, {}).setdefault(scope, {}).setdefault(tt, {})
                     .setdefault(target, {}))[unit] = val
        elif tag == LANDMARK_YIELD:
            for ident, v in engine.named_array(c, YIELDS).items():
                _put(fam, ident, "empire", "landmark", "flat", v)
        elif tag in POLICIES:
            if t in ("1", "true", "True"):
                policies[POLICIES[tag]] = True
        elif tag in ENABLE_LISTS:
            lst = _enable_list(c)
            if lst:
                enables[ENABLE_LISTS[tag]] = sorted(lst)
        elif tag == "iRevIdxSwitchTo":
            v = _num(t)                                    # one-time revolution-index BURST on switching to this
            if v not in (None, 0, 0.0):                    # civic (signed) -> grants (owner 2026-06-15), not a
                grants["revolution"] = v                   # continuous revolution modifier
        elif tag == "FreeSpecialistCounts":
            # freeSpecialists COUNT family, keyed by specialist type, empire scope (keep-on-civic; active while the
            # civic is adopted -- inherent, no extra `enabled`). modifier.md §6.7 (A).
            m = _keyed_entries(c, None)
            for k in sorted(m):
                if m[k]:
                    fam.setdefault("freeSpecialists", {}).setdefault("empire", {})[k] = m[k]
        elif tag == "PropertyManipulators":
            _properties(c, props)
        elif tag == "Flavors":
            v = engine.generic(c)
            if v:
                ai["flavours"] = v
        elif tag == "iAIWeight":
            if engine.is_int(t) and int(t) != 0:
                ai.setdefault("behaviour", {})["weight"] = int(t)
        elif tag == "Button":
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui.art.icon via ART_BLOCK
        elif tag in IDENTITY:
            if engine.is_int(t):
                if int(t) != 0:
                    identity[IDENTITY[tag]] = int(t)
            elif (t or list(c)) and t != "NONE":
                identity[IDENTITY[tag]] = engine.generic(c)
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    # the civic enables its capability lists; plus any store-derived edge (e.g. civic-gated buildings)
    derived = store.enabled_by(typ)
    for k in sorted(derived):
        enables.setdefault(k, derived[k])

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia", "help", "strategy"):
        if k in text:
            out[k] = text[k]
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:
        if family not in out:
            out[family] = fam[family]
    for prop in sorted(props):
        out[prop] = props[prop]
    if policies:
        out["policies"] = OrderedDict((k, policies[k]) for k in sorted(policies))
    if grants:
        out["grants"] = grants
    if ai:
        out["ai"] = ai
    cc.emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("CivicInfo")
    results, cats, all_leftover = OrderedDict(), {}, set()
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store)
        results[typ] = obj
        cats[typ] = _category(rec)
        all_leftover.update(leftover)
    print("CivicInfo curated: %d" % len(results))
    fams = sorted({k for o in results.values() for k in o
                   if k not in ("type", "description", "civilopedia", "help", "strategy", "enables",
                                "policies", "grants", "ai", "ui", "world", "sound", "identity")})
    print("  families/props seen: %s" % ", ".join(fams))
    if all_leftover:
        print("  !! leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "civics")
        for typ, obj in results.items():
            folder = os.path.join(out_dir, cats[typ]) if cats[typ] else out_dir
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d CivicInfo JSON files under Assets/Data/civics/<category>/" % len(results))


if __name__ == "__main__":
    main()
