#!/usr/bin/env python3
"""Curate Trait (#428) — "Mount Doom": ONE CvTraitInfo class serving BOTH trait systems. BESPOKE.

There is a SINGLE trait class. The "developing/complex leaders" system is the SAME CvTraitInfo, just
assigned differently: CvLeaderHeadInfo carries DefaultTraits AND DefaultComplexTraits (both lists of
TraitTypes into this one table), and CvPlayer picks the list by GAMEOPTION_LEADER_COMPLEX_TRAITS
(CvPlayer.cpp:439, :1583). So assignment (fixed vs accumulating) is CONSUMER-side; the data shape is one
uniform `trait` surface. A trait is an EMPIRE-wide SOURCE/ENABLER (deposits via CvPlayer::processTrait into
player-level accumulators) — so virtually every modifier is `empire` scope, and nothing ever TARGETS a trait.

Field dispositions verified by the classify-trait workflow (wf_cc8659b5: 3 ground-truth agents + 6 field
slices, each adversarially verified, + a coverage/conflict/CREST/dev-leader audit) against CvTraitInfo.{h,cpp}
+ CvPlayer::processTrait + CvCity/CvGameTextMgr consumers. Full analysis:
Tools/Migration/classifications/trait-classification.json. Conventions MIRROR curate_civic.py (the first heavy
entity owns them: production vs unitProduction split, the grouped `stateReligion` family, PropertyManipulators
parsed into per-PROPERTY_* families). OWNER RULINGS (2026-06-14) drive the structural calls:

- DEV-LEADERS relations -> top-down/derived, NOT trait-side prereqs:
  * TraitPrereq / TraitPrereqOr1 / TraitPrereqOr2 (trait->trait) and PrereqTech (tech->trait) INVERT via the
    store into `enables.traits` on the prereq trait / tech (registered in store.PREREQ_FIELDS) -> DROPPED here.
  * DisallowedTraitTypes -> a same-tier `excludes` set (author one end; the symmetric reverse is derived into
    the cold-path reverse index). NOT the #429 SPATIAL sideways — a structural mutual-exclusion.
  * OnGameOptions / NotOnGameOptions -> a `loadPrune` gate (the reader prunes at load; bulk are
    GAMEOPTION_LEADER_COMPLEX_TRAITS — "this trait only exists when complex traits are on").
  * PromotionLine + LinePriority -> a `succession` block (the developing-leaders line ordering); the line's own
    PrereqTech is a DERIVED tech enabler owned by PromotionLineInfo (store), not re-authored here.
  * Categories -> DROP (dead: zero C++ readers; not authored in either trait XML; civic drops it too).
  * the culture-requirement progression (GAMEOPTION_NEXT_TRAIT_CULTURE_REQ_PERCENT) is a GAME OPTION, not a
    trait field -> not authored on the trait.
- CREST: BonusHappinessChanges (the only fresh bonus-conditioner) is authored ON THE TRAIT (keep-on-source,
  modifier-spec §6 — supersedes the old "fold onto the bonus" rule; a resource is never a target): the trait
  grants +N happiness while a specific bonus is present. The deposit is `happiness.empire.flat` (scope = the
  MODIFIER's: the trait benefits the player's cities); the condition is the agreed full+explicit enabler atom
  (enabler-spec §6.1/§13.7) `enabled:{type:BONUS_X, scope:empire, min:1}` ("we have ≥1", a tally count read).
  TechResearchModifiers is crest-adjacent but the plan HANDOFF rules it STAYS trait-side (it surfaces on a tech
  only as a derived boostedBy) -> kept as research.empire.byTech.{TECH}.percent.
- GreatPeopleUnitType + GreatPeopleRateChange FOLD into greatPeopleRate.empire.units.{UNIT}.flat (the change is
  keyed by the GP unit; CvPlayer.cpp:28606-28610, only when >0).
- CityStartCulture + BonusPopulationinNewCities -> a `cityFounding` family (standing empire accumulators applied
  at every city founding, NOT one-shot grants).
- Double-author DROPs (specialist migrated first, curate_specialist.py:80-81): SpecialistYieldChanges +
  SpecialistCommerceChanges.
- MaxAnarchy/MinAnarchy -> clean identity keys (maxAnarchy default -1 carried verbatim; min default 0).
- x100 carried faithfully (#432); fRev* are floats carried verbatim.

  python3 curate_trait.py --sample TRAIT_PHILOSOPHICAL TRAIT_FINANCIAL
  python3 curate_trait.py --write
"""
import argparse
import json
import os
from collections import OrderedDict

import engine
from curate_common import de_i
from store import Store, REPO

YIELDS, COMMERCES = engine.YIELDS, engine.COMMERCES

# --- scalar modifier families: tag -> (family, scope, member, unit). member "" => single-concept. ---
SCALAR = {
    # wellbeing
    "iHealth":                         ("health", "empire", "", "flat"),
    "iHappiness":                      ("happiness", "empire", "", "flat"),
    "iLargestCityHappiness":           ("happiness", "empire", "largestCity", "flat"),
    "iNonStateReligionHappiness":      ("happiness", "empire", "nonStateReligion", "flat"),
    "iHappyPerMilitaryUnit":           ("happiness", "empire", "perMilitaryUnit", "perMilitaryUnit"),
    "iGlobalPopulationgrowthratepercentage": ("growth", "empire", "", "percent"),
    # great people / generals
    "iGreatPeopleRateModifier":        ("greatPeopleRate", "empire", "", "percent"),
    "iGreatGeneralRateModifier":       ("greatGeneralRate", "empire", "", "percent"),
    "iDomesticGreatGeneralRateModifier":("greatGeneralRate", "empire", "domestic", "percent"),
    "iFreeSpecialist":                 ("freeSpecialists", "empire", "", "flat"),
    # production (build-rate; mirrors civic: production=military/building, unitProduction=unit) — NOT the yield
    "iMilitaryProductionModifier":     ("production", "empire", "military", "percent"),
    "iMaxGlobalBuildingProductionModifier": ("production", "empire", "worldWonder", "percent"),
    "iMaxTeamBuildingProductionModifier":   ("production", "empire", "teamWonder", "percent"),
    "iMaxPlayerBuildingProductionModifier": ("production", "empire", "nationalWonder", "percent"),
    # maintenance / upkeep
    "iDistanceMaintenanceModifier":    ("maintenance", "empire", "distance", "percent"),
    "iNumCitiesMaintenanceModifier":   ("maintenance", "empire", "numCities", "percent"),
    "iCorporationMaintenanceModifier": ("maintenance", "empire", "corporation", "percent"),
    "iUpkeepModifier":                 ("upkeep", "empire", "civic", "percent"),
    "iFreeUnitUpkeepCivilian":         ("upkeep", "empire", "freeCivilian", "flat"),
    "iFreeUnitUpkeepMilitary":         ("upkeep", "empire", "freeMilitary", "flat"),
    "iFreeUnitUpkeepCivilianPopPercent":("upkeep", "empire", "freeCivilian", "perPopulation"),
    "iFreeUnitUpkeepMilitaryPopPercent":("upkeep", "empire", "freeMilitary", "perPopulation"),
    "iCivilianUnitUpkeepMod":          ("upkeep", "empire", "unitCivilian", "percent"),
    "iMilitaryUnitUpkeepMod":          ("upkeep", "empire", "unitMilitary", "percent"),
    "iUnitUpgradePriceModifier":       ("upkeep", "empire", "upgradePrice", "percent"),
    # work / improvement / conscript / hurry / trade
    "iWorkerSpeedModifier":            ("workRate", "empire", "", "percent"),
    "iImprovementUpgradeRateModifier": ("improvementUpgradeRate", "empire", "", "percent"),
    "iMaxConscript":                   ("conscript", "empire", "", "flat"),
    "iHurryAngerModifier":             ("hurry", "empire", "anger", "percent"),
    "iHurryCostModifier":              ("hurry", "empire", "cost", "percent"),
    "iTradeRoutes":                    ("tradeRoutes", "empire", "", "flat"),
    "iCoastalTradeRoutes":             ("tradeRoutes", "empire", "coastal", "flat"),
    "iMaxTradeRoutesChange":           ("tradeRoutes", "empire", "max", "flat"),
    "iForeignTradeRouteModifier":      ("tradeRoutes", "empire", "foreign", "percent"),
    "iGoldenAgeDurationModifier":      ("goldenAge", "empire", "", "percent"),
    "iGlobalAirUnitCapacity":          ("unitCapability", "empire", "airUnitCapacity", "flat"),
    # experience
    "iFreeExperience":                 ("experience", "empire", "", "flat"),
    "iLevelExperienceModifier":        ("experience", "empire", "levelModifier", "percent"),
    "iExpInBorderModifier":            ("experience", "empire", "inBorder", "percent"),
    "iCapitalXPModifier":              ("experience", "empire", "capital", "percent"),
    "iHolyCityofNonStateReligionXPModifier": ("experience", "empire", "nonStateHolyCityXP", "percent"),
    # combat / defense
    "iCityDefenseBonus":               ("combat", "empire", "cityDefense", "percent"),
    "iBombardDefense":                 ("combat", "empire", "bombardDefense", "percent"),
    "iEspionageDefense":               ("combat", "empire", "espionageDefense", "percent"),
    "iNationalCaptureProbabilityModifier": ("combat", "empire", "captureProbability", "percent"),
    "iNationalCaptureResistanceModifier":  ("combat", "empire", "captureResistance", "percent"),
    "iMissileRange":                   ("combat", "empire", "missileRange", "flat"),
    "iFlightOperationRange":           ("combat", "empire", "flightRange", "flat"),
    "iNavalCargoSpace":                ("combat", "empire", "navalCargo", "flat"),
    "iMissileCargoSpace":              ("combat", "empire", "missileCargo", "flat"),
    # diplomacy
    "iAttitudeModifier":               ("diplomacy", "empire", "attitude", "flat"),
    "iWarWearinessAccumulationModifier":("diplomacy", "empire", "warWeariness", "percent"),
    "iEnemyWarWearinessModifier":      ("diplomacy", "empire", "enemyWarWeariness", "percent"),
    # anarchy durations
    "iCivicAnarchyTimeModifier":       ("durations", "empire", "civicAnarchy", "percent"),
    "iReligiousAnarchyTimeModifier":   ("durations", "empire", "religiousAnarchy", "percent"),
    # revolution (RevolutionDCM — kept faithful; f* are floats)
    "iRevIdxLocal":                    ("revolution", "empire", "local", "flat"),
    "iRevIdxNational":                 ("revolution", "empire", "national", "flat"),
    "iRevIdxDistanceModifier":         ("revolution", "empire", "distanceModifier", "percent"),
    "iRevIdxHolyCityGood":             ("revolution", "empire", "holyCityGood", "flat"),
    "iRevIdxHolyCityBad":              ("revolution", "empire", "holyCityBad", "flat"),
    "iFreedomFighterChange":           ("revolution", "empire", "freedomFighter", "flat"),
    "fRevIdxNationalityMod":           ("revolution", "empire", "nationalityMod", "percent"),
    "fRevIdxGoodReligionMod":          ("revolution", "empire", "goodReligionMod", "percent"),
    "fRevIdxBadReligionMod":           ("revolution", "empire", "badReligionMod", "percent"),
    # founding (standing empire accumulators applied at every city founding — owner: cityFounding modifier)
    "iCityStartCulture":               ("cityFounding", "empire", "startCulture", "flat"),
    "iBonusPopulationinNewCities":     ("cityFounding", "empire", "startPopulation", "flat"),
}

# --- state-religion grouped family: tag -> (member, unit). All empire, gated on having a state religion. ---
STATE_RELIGION = {
    "iStateReligionHappiness":                 ("happiness",         "flat"),
    "iStateReligionGreatPeopleRateModifier":   ("greatPeopleRate",   "percent"),
    "iStateReligionUnitProductionModifier":    ("unitProduction",    "percent"),
    "iStateReligionBuildingProductionModifier":("buildingProduction","percent"),
    "iStateReligionFreeExperience":            ("freeExperience",    "flat"),
    "iHolyCityofStateReligionXPModifier":      ("holyCityXP",        "percent"),
    "iStateReligionSpreadProbabilityModifier": ("spreadProbability", "percent"),
    "iNonStateReligionSpreadProbabilityModifier":("nonStateSpreadProbability", "percent"),
}

# --- positional yield/commerce arrays -> SPLIT into per-identifier families: tag -> (scope, member, unit, keys). ---
SPLIT_ARRAY = {
    "YieldChanges":             ("empire", "",          "flat",         YIELDS),
    "YieldModifiers":           ("empire", "",          "percent",      YIELDS),
    "TradeYieldModifiers":      ("empire", "tradeRoute","percent",      YIELDS),
    "CapitalYieldModifiers":    ("empire", "capital",   "percent",      YIELDS),
    "SeaPlotYieldChanges":      ("empire", "seaPlot",   "flat",         YIELDS),
    "SpecialistExtraYields":    ("empire", "specialist","perSpecialist",YIELDS),
    "GoldenAgeYieldChanges":    ("empire", "goldenAge", "flat",         YIELDS),
    "CommerceChanges":          ("empire", "",          "flat",         COMMERCES),
    "CommerceModifiers":        ("empire", "",          "percent",      COMMERCES),
    "CapitalCommerceModifiers": ("empire", "capital",   "percent",      COMMERCES),
    "SpecialistExtraCommerces": ("empire", "specialist","perSpecialist",COMMERCES),
    "GoldenAgeCommerceChanges": ("empire", "goldenAge", "flat",         COMMERCES),
}
# --- positional yield arrays kept GROUPED under their own family (yield is the member, not the family). ---
GROUPED_YIELD_ARRAY = {
    "ExtraYieldThresholds": ("extraYieldThreshold", "empire", "flat", YIELDS),
    "LessYieldThresholds":  ("lessYieldThreshold",  "empire", "flat", YIELDS),
}

# --- entity-keyed (target-keyed) maps: tag -> (family, scope, targetType, unit, valueKeys|None). ---
#   keys != None  => SPLIT (each named yield/commerce becomes its own top-level family).
#   unit "enabler" => the entry value is a bool flag -> emit true.
#   TechResearchModifiers: tech is a CONDITIONER (byTech), kept trait-side per the HANDOFF.
KEYED = {
    "ImprovementYieldChanges":            (None,             "empire", "improvements",   "flat",    YIELDS),
    "ImprovementUpgradeModifierTypes":    ("improvementUpgradeRate", "empire", "improvements", "percent", None),
    "BuildWorkerSpeedModifierTypes":      ("workRate",       "empire", "builds",         "percent", None),
    "DomainFreeExperiences":              ("experience",     "empire", "domains",        "flat",    None),
    "DomainProductionModifiers":          ("unitProduction", "empire", "domains",        "percent", None),
    "BuildingProductionModifierTypes":    ("production",     "empire", "buildings",      "percent", None),
    "SpecialBuildingProductionModifierTypes": ("production", "empire", "specialBuildings","percent", None),
    "BuildingHappinessModifierTypes":     ("happiness",      "empire", "buildings",      "flat",    None),
    "UnitProductionModifierTypes":        ("unitProduction", "empire", "units",          "percent", None),
    "SpecialUnitProductionModifierTypes": ("unitProduction", "empire", "specialUnits",   "percent", None),
    "UnitCombatFreeExperiences":          ("experience",     "empire", "unitCombats",    "flat",    None),
    "UnitCombatProductionModifiers":      ("unitProduction", "empire", "unitCombats",    "percent", None),
    "CivicOptionNoUpkeepTypes":           ("upkeep",         "empire", "civicOptions",   "enabler", None),
    "TechResearchModifiers":              ("research",       "empire", "byTech",         "percent", None),  # CREST: stays (HANDOFF)
}

# --- boolean policy/capability flags -> policies.{name}: true. ---
POLICIES = {
    "bNonStateReligionCommerce": "nonStateReligionCommerce", "bUpgradeAnywhere": "upgradeAnywhere",
    "bMilitaryFoodProduction": "militaryFoodProduction", "bAllowsInquisitions": "allowInquisitions",
    "bCitiesStartwithStateReligion": "citiesStartWithStateReligion", "bDraftsOnCityCapture": "draftsOnCityCapture",
    "bFreeSpecialistperWorldWonder": "freeSpecialistPerWorldWonder",
    "bFreeSpecialistperNationalWonder": "freeSpecialistPerNationalWonder",
    "bFreeSpecialistperTeamProject": "freeSpecialistPerTeamProject", "bExtraGoody": "extraGoody",
    "bAllReligionsActive": "allReligionsActive", "bBansNonStateReligions": "bansNonStateReligions",
    "bFreedomFighter": "freedomFighter",
}
# --- boolean flags -> identity (intrinsic "what am I", not a player-state policy). ---
IDENTITY_FLAGS = {
    "bNegativeTrait": "negativeTrait", "bImpurePropertyManipulators": "impurePropertyManipulators",
    "bImpurePromotions": "impurePromotions", "bCivilizationTrait": "civilizationTrait",
    "bBarbarianSelectionOnly": "barbarianSelectionOnly",
}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help", "Strategy": "strategy"}
# DROPs: prereqs (-> store enables), double-author (-> curate_specialist), dead. (BonusHappinessChanges is NO
# LONGER dropped — it is authored on the trait, gated by bonus presence; handled in the loop + merge below.)
DROP = {"Type", "TraitPrereq", "TraitPrereqOr1", "TraitPrereqOr2", "PrereqTech",
        "SpecialistYieldChanges", "SpecialistCommerceChanges", "Categories"}
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "espionage",
                "extraYieldThreshold", "lessYieldThreshold", "happiness", "health", "growth",
                "greatPeopleRate", "greatGeneralRate", "freeSpecialists", "experience", "conscript",
                "combat", "unitProduction", "maintenance", "upkeep", "tradeRoutes", "hurry", "workRate",
                "improvementUpgradeRate", "goldenAge", "cityFounding", "unitCapability", "durations",
                "diplomacy", "stateReligion", "revolution"]


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
    """<Foo><FooEntry><XType>K</XType><value...></FooEntry>...> -> {K: int|float|named_array}."""
    out = OrderedDict()
    for entry in list(node):
        k, vals = None, []
        for c in entry:
            if k is None and c.tag.endswith("Type"):
                k = engine.text(c)
            else:
                vals.append(c)
        if not k or k == "NONE":
            continue
        if len(vals) == 1 and list(vals[0]):
            val = engine.named_array(vals[0], keys) if keys else engine.generic(vals[0])
        elif len(vals) == 1:
            val = _num(engine.text(vals[0]))
        else:
            val = next((n for n in (_num(engine.text(c)) for c in vals) if n is not None), None)
        if val not in (None, {}, [], ""):
            out[k] = val
    return out


def _type_list(node):
    """list of referenced Types (DisallowedTraitTypes / On|NotOnGameOptions). Entries are either a bare
    <Tag>TYPE</Tag> or a wrapper <Entry><XType>TYPE</XType></Entry>."""
    out = []
    for entry in list(node):
        t = engine.text(entry)
        if not t:  # wrapper: take the first child text
            for c in entry:
                if engine.text(c):
                    t = engine.text(c)
                    break
        if t and t != "NONE":
            out.append(t)
    return out


def _free_promotions(node):
    """<FreePromotionUnitCombatType><PromotionType>P</><UnitCombatTypes><UnitCombatType>UC</>...></> ->
    {PROMOTION: [UNITCOMBAT, ...]} (a free promotion granted to those unit-combat classes)."""
    out = OrderedDict()
    for entry in list(node):
        promo, ucs = None, []
        for c in entry:
            if c.tag == "PromotionType":
                promo = engine.text(c)
            elif c.tag == "UnitCombatTypes":
                ucs = [engine.text(u) for u in c if engine.text(u)]
        if promo and ucs:
            out[promo] = sorted(ucs)
    return out


def _properties(node, props):
    """PropertyManipulators -> v3 deposits via the shared converter (engine.property_source_v3 — the standard,
    uniform with Property/Civic/Religion). Trait sources are all CONSTANT/RELATION_ASSOCIATED."""
    for src in node.findall("PropertySource"):
        conv = engine.property_source_v3(src)
        if conv is None:
            continue
        prop, scope, unit, value = conv
        props.setdefault(prop, OrderedDict()).setdefault(scope, OrderedDict())[unit] = value


def is_complex(typ, rec, complex_ids):
    """SIMPLE vs COMPLEX trait split (owner 2026-06-15) — they are two separate systems hacked into one
    CvTraitInfo (TB); complex traits become their own Info type (coding pass). complex iff: the trait is a
    CvInfoReplacements complex variant (typ in complex_ids) OR it is gated by GAMEOPTION_LEADER_COMPLEX_TRAITS
    (its OnGameOptions). Everything else (incl. vanilla bases that HAVE a complex counterpart) is simple."""
    if typ in complex_ids:
        return True
    og = rec.find("OnGameOptions")
    if og is not None:
        for x in og:
            for s in [engine.text(x)] + [engine.text(cc) for cc in x]:
                if s and "COMPLEX" in s:
                    return True
    return False


def curate(typ, rec, store):
    text, fam, props, policies, grants, art, identity, ai = {}, {}, {}, {}, {}, {}, {}, {}
    excludes, load_on, load_not, succession = [], [], [], {}
    gp_unit, gp_change = None, None
    bonus_happy = OrderedDict()
    leftover = []
    for c in rec:
        tag, t = c.tag, engine.text(c)
        if tag in DROP:
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
        elif tag in GROUPED_YIELD_ARRAY:
            family, scope, unit, keys = GROUPED_YIELD_ARRAY[tag]
            for ident, v in engine.named_array(c, keys).items():   # yield is the MEMBER under the family
                _put(fam, family, scope, ident, unit, v)
        elif tag in KEYED:
            family, scope, tt, unit, keys = KEYED[tag]
            for target, val in _keyed_entries(c, keys).items():
                if keys:                                           # split: each named member is its own family
                    for ident, v in (val.items() if isinstance(val, dict) else []):
                        (fam.setdefault(ident, {}).setdefault(scope, {}).setdefault(tt, {})
                         .setdefault(target, {}))[unit] = v
                else:
                    out_val = True if unit == "enabler" else val
                    if unit == "enabler" and not val:
                        continue
                    (fam.setdefault(family, {}).setdefault(scope, {}).setdefault(tt, {})
                     .setdefault(target, {}))[unit] = out_val
        elif tag in POLICIES:
            if t in ("1", "true", "True"):
                policies[POLICIES[tag]] = True
        elif tag in IDENTITY_FLAGS:
            if t in ("1", "true", "True"):
                identity[IDENTITY_FLAGS[tag]] = True
        elif tag == "DisallowedTraitTypes":
            excludes = _type_list(c)
        elif tag == "OnGameOptions":
            load_on = _type_list(c)
        elif tag == "NotOnGameOptions":
            load_not = _type_list(c)
        elif tag == "PromotionLine":
            if t and t != "NONE":
                succession["promotionLine"] = t
        elif tag == "iLinePriority":
            if engine.is_int(t) and int(t) != 0:
                succession["priority"] = int(t)
        elif tag == "iGreatPeopleRateChange":
            gp_change = int(t) if engine.is_int(t) else None
        elif tag == "GreatPeopleUnitType":
            gp_unit = t if (t and t != "NONE") else None
        elif tag == "EraAdvanceFreeSpecialistType":
            if t and t != "NONE":
                grants["eraAdvanceFreeSpecialist"] = t
        elif tag == "GoldenAgeonBirthofGreatPersonType":
            if t and t != "NONE":
                grants["goldenAgeOnBirthOfGreatPerson"] = t
        elif tag == "FreePromotionUnitCombatTypes":
            fp = _free_promotions(c)
            if fp:
                grants["freePromotions"] = fp
        elif tag == "BonusHappinessChanges":              # per-bonus conditional happiness, authored on the trait
            bonus_happy = _keyed_entries(c, None)          # {BONUS_X: +N happy while that bonus is present}
        elif tag == "PropertyManipulators":
            _properties(c, props)
        elif tag == "Flavors":
            v = engine.generic(c)
            if v:
                ai["flavours"] = v
        elif tag == "bCoastalAIInfluence":
            if t in ("1", "true", "True"):
                ai.setdefault("behaviour", {})["coastalAIInfluence"] = True
        elif tag == "Button":
            v = engine.generic(c)
            if v not in (None, "", [], {}, "NONE"):
                art["icon"] = v
        elif tag == "ShortDescription":
            if t:
                identity["shortDescription"] = t
        elif tag == "iMaxAnarchy":
            if engine.is_int(t) and int(t) != -1:        # -1 is the "no limit" default
                identity["maxAnarchy"] = int(t)
        elif tag == "iMinAnarchy":
            if engine.is_int(t) and int(t) != 0:
                identity["minAnarchy"] = int(t)
        else:
            if list(c) or t:
                leftover.append(tag)
                identity[engine.FIELD_RENAME.get(tag, de_i(tag))] = engine.generic(c)

    # GP-rate change folds onto the GP unit (CvPlayer.cpp:28606-28610; only meaningful with a unit + >0)
    if gp_unit and gp_change and gp_change != 0:
        (fam.setdefault("greatPeopleRate", {}).setdefault("empire", {})
         .setdefault("units", {}).setdefault(gp_unit, {}))["flat"] = gp_change

    # BonusHappinessChanges -> conditional happiness deposits on the TRAIT (modifier-spec §6 keep-on-source).
    # Deposit = `happiness.empire.flat` (scope is the MODIFIER's: the trait benefits the player's own cities).
    # Condition = the agreed full+explicit enabler atom (enabler-spec §6.1/§13.7): `{type:BONUS_X, scope:empire,
    # min:1}` = "we have at least 1 of this bonus" (a tally count read, presence). Coexists with an unconditional
    # iHappiness via the §1.5 mixed list (a bare value + condition-bearing entries).
    if bonus_happy:
        node = fam.setdefault("happiness", {}).setdefault("empire", {})
        existing = node.get("flat")
        entries = (existing if isinstance(existing, list) else [existing]) if existing is not None else []
        for b, v in bonus_happy.items():
            entries.append(OrderedDict([("value", v),
                                        ("enabled", OrderedDict([("type", b), ("scope", "empire"), ("min", 1)]))]))
        node["flat"] = entries

    out = OrderedDict()
    out["type"] = typ
    for k in ("description", "civilopedia", "help", "strategy"):
        if k in text:
            out[k] = text[k]
    # top-down enables: traits this trait is a prereq for, + techs->traits surface on the tech, not here
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    # NB: the CvInfoReplacements base->complex link is DROPPED (owner 2026-06-15): simple and complex traits are
    # "2 completely different traits hacked on top of each other" (TB), NOT a base+variant. They become TWO SEPARATE
    # Info types behind a shared interface (coded in the coding pass, #430); the migration splits them into
    # simple/ + complex/ folders and authors each as an independent, full trait. No `replacedBy` cross-link.
    if excludes:
        out["excludes"] = sorted(excludes)
    for family in FAMILY_ORDER:
        if family in fam:
            out[family] = fam[family]
    for family in fam:                                   # any family outside the ordering (safety)
        if family not in out:
            out[family] = fam[family]
    for prop in sorted(props):                           # PROPERTY_* families
        out[prop] = props[prop]
    if policies:
        out["policies"] = OrderedDict((k, policies[k]) for k in sorted(policies))
    if grants:
        out["grants"] = grants
    if succession:
        out["succession"] = succession
    if load_on or load_not:
        lp = OrderedDict()
        if load_on:
            lp["onGameOptions"] = sorted(load_on)
        if load_not:
            lp["notOnGameOptions"] = sorted(load_not)
        out["loadPrune"] = lp
    if ai:
        out["ai"] = ai
    if art:
        out["art"] = art
    if identity:
        out["identity"] = identity
    return out, leftover


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*", help="print these types (default: first 1)")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("TraitInfo")
    complex_ids = set(v["replacement"] for v in store.replacements.get("TraitInfo", {}).values())
    results, all_leftover, folders = OrderedDict(), set(), {}
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store)
        results[typ] = obj
        folders[typ] = "complex" if is_complex(typ, rec, complex_ids) else "simple"
        all_leftover.update(leftover)
    nc = sum(1 for f in folders.values() if f == "complex")
    print("TraitInfo curated: %d  (simple=%d, complex=%d)" % (len(results), len(results) - nc, nc))
    STRUCT = {"type", "description", "civilopedia", "help", "strategy", "enables", "replacedBy", "excludes",
              "policies", "grants", "succession", "loadPrune", "ai", "art", "identity"}
    fams = sorted({k for o in results.values() for k in o if k not in STRUCT and not k.startswith("PROPERTY_")})
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("  families seen: %s" % ", ".join(fams))
    for k in ("enables", "replacedBy", "excludes", "succession", "loadPrune", "grants", "policies", "ai"):
        print("  with %-11s: %d" % (k, has(k)))
    if all_leftover:
        print("  !! leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "traits")
        for typ, obj in results.items():                       # simple/ + complex/ — two separate sets (folders)
            folder = os.path.join(out_dir, folders[typ])
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)
        print("\nwrote %d TraitInfo JSON files under Assets/Data/traits/{simple,complex}/" % len(results))


if __name__ == "__main__":
    main()
