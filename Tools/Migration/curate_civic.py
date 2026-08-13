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
- BonusCommerceModifiers: NOT YET CAPTURED — dropped here and not picked up by curate_bonus either (known
  live gap; no inversion table exists for it).

Curator batch 4 (info-rebuild.md rulings 25/26/27 + the free-upkeep owner ruling):
- R26 city limits: iCityLimit -> identity.cityLimit config; iCityOverLimitUnhappy -> ONE happiness entry
  {-V, per:{CITY, above:"CITY_LIMIT"}, enabled:GAMEOPTION_EXP_OVEREXPANSION_PENALTIES} (supersedes the R21
  blocked constant-pair; see the SCALAR note for the engine math).
- R27 route yields: TradeYieldModifiers -> tradeRoutes.empire.modifier.<channel>.percent for ALL channels
  (the channel axis on the modifier kind; channel-agnostic totalTradeModifier legs stay bare).
- FREE-UPKEEP MODEL CHANGE (owner rulings, batch 4 + 4b sign normalization): the freeMilitary/freeCivilian
  kinds carry ONE FREE-AMOUNT CONVENTION -- positive = free upkeep granted, negative = free allowance
  reduced; entries sum and the engine nets max(0, upkeep - SUMfree) with the >=0 floors as family-combine
  floor metadata (modifier.md 2 min-member mechanism, C++ leg pending). iFreeUnitUpkeep*PopPercent convert
  UNCONDITIONALLY to per-population deposits {P, per:{POPULATION, each:100}} keeping P's own sign -- an
  INTENTIONAL divergence from the legacy getModifiedIntValue rounding (its asymmetric mod<0 branch is not
  chased; do NOT try to restore bit-parity with the old helper).

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

# The per-specialist count-scaler. The deposit's scope is EMPIRE (the source's reach) while the count it names
# is CITY-local -- exactly the case json §3.7's object form exists for, so the scope is authored on the scaler
# itself rather than inherited from the deposit.
_PER_SPECIALIST_IN_CITY = OrderedDict([("type", "SPECIALIST"), ("scope", "city")])

YIELDS, COMMERCES = engine.YIELDS, engine.COMMERCES

# --- scalar modifier families: tag -> (family, scope, member, unit). member "" => single-concept. ---
SCALAR = {
    # maintenance / upkeep / trade / hurry / workrate
    "iDistanceMaintenanceModifier":    ("maintenance", "empire", "distance",    "percent"),
    "iNumCitiesMaintenanceModifier":   ("maintenance", "empire", "numCities",   "percent"),
    # iHomeArea/iOtherAreaMaintenanceModifier are NOT here: home/other-area is WHEN/WHERE, not a calc component
    # (ruling 2, info-rebuild.md) -> conditioned deposits on IS_HOME_AREA in SCALAR_COND below.
    "iCorporationMaintenanceModifier": ("maintenance", "empire", "corporation", "percent"),
    "iInflation":                      ("inflation", "empire", None,       "percent"),
    "iCivilianUnitUpkeepMod":          ("upkeep", "empire", "unitCivilian","percent"),
    "iMilitaryUnitUpkeepMod":          ("upkeep", "empire", "unitMilitary","percent"),
    "iDistantUnitSupportCostModifier": ("upkeep", "empire", "supply",      "percent"),
    "iFreeUnitUpkeepMilitary":         ("upkeep", "empire", "freeMilitary","flat"),
    "iFreeUnitUpkeepCivilian":         ("upkeep", "empire", "freeCivilian","flat"),
    # iFreeUnitUpkeep{Military,Civilian}PopPercent are NOT here: converted per the batch-4 owner ruling to
    # subtractive per-population deposits on the same kinds -- handled explicitly in curate(). See the
    # docstring note "free-upkeep model change".
    # tradeRoutes is ONE family with conditions (ruling 11): the route COUNT is the MEMBERLESS scope-wide
    # amount (kind 0 IS the count -- the reconciliation micro-fix); kinds modifier/max; the foreign/sharedCivic
    # variants are CONDITIONS -> SCALAR_COND below.
    "iTradeRoutes":                    ("tradeRoutes", "empire", "",           "flat"),
    "iHurryCostModifier":              ("costs", "empire", "hurry",     "percent"),   # ruling 18: hurry.cost -> costs.hurry (CvCity::getHurryCostModifier leg)
    "iHurryInflationModifier":         ("hurry", "empire", "inflation", "percent"),
    "iWorkerSpeedModifier":            ("workRate", "empire", "", "percent"),
    "iImprovementUpgradeRateModifier": ("improvementUpgradeRate", "empire", "", "percent"),
    # military / great-people
    "iMilitaryProductionModifier":     ("buildRate", "empire", "military", "percent"),
    # L13 re-home (2026-07-05): the spec'd DEFENSE family (modifier.md §6: `amount` = the additive defense %),
    # matching the buildings' defense.empire.amount -- the old combat.empire.cityDefense had NO reader.
    "iExtraCityDefense":               ("defense", "empire", "amount",             "percent"),
    # capture is its own concept family (ruling 5, info-rebuild.md): the old combat.capture* members redistribute
    # to `capture` kinds probability/resistance (matching the unit-plane capture family), empire scope.
    "iNationalCaptureProbabilityModifier": ("capture", "empire", "probability", "percent"),
    "iNationalCaptureResistanceModifier":  ("capture", "empire", "resistance",  "percent"),
    "iFreeExperience":                 ("experience", "empire", "",        "flat"),
    "iExpInBorderModifier":            ("experience", "empire", "inBorder","percent"),
    "iGreatGeneralRateModifier":       ("greatGeneralRate", "empire", "",        "percent"),
    "iDomesticGreatGeneralRateModifier":("greatGeneralRate","empire", "domestic","percent"),
    "iGreatPeopleRateModifier":        ("greatPeopleRate", "empire", "", "percent"),
    "iMaxConscript":                   ("conscript", "empire", "", "flat"),
    "iFreeSpecialist":                 ("freeSpecialists", "empire", "", "any"),
    # wellbeing
    "iCivicHappiness":                 ("happiness", "empire", "",            "flat"),
    # iNonStateReligionHappiness / iForeignerUnhappyPercent / iTaxRateUnhappiness are NOT here: converted per
    # rulings 20/22/23 (info-rebuild.md) -- handled explicitly in curate() with their engine sites transcribed.
    # iCityLimit/iCityOverLimitUnhappy are NOT here either: converted per ruling 26 (info-rebuild.md), which
    # supersedes the blocked ruling-21 constant-pair shape with the first-class per.above scaler -- handled
    # explicitly in curate(). Engine math (CvCity.cpp:5665-5674 live, :8659-8662 what-if twin):
    #   unhappiness += V x max(0, numCities - scaledLimit)
    # where scaledLimit = m_iCityLimit x CvWorldInfo::getCityLimitsScalePercent()/100 (50..200 by world size)
    # under GAMEOPTION_EXP_OVEREXPANSION_PENALTIES, else 0 (archived CvCivicInfo::getCityLimit,
    # SourceArchive/Infos/CvCivicInfo.cpp:1015-1022). Authored as ONE entry
    #   {-V, per:{type:CITY, above:"CITY_LIMIT"}, enabled:"GAMEOPTION_EXP_OVEREXPANSION_PENALTIES"}
    # with the SOURCE-resolved CITY_LIMIT token (json.md 3.1/3.7) = the civic's own identity.cityLimit config
    # x the world-size scale (token resolution + the per-resolver `above` leg are on the C++ worklist).
    # The BASE limit itself is emitted as config data: identity.cityLimit (the anarchyLength convention --
    # a scalar rule parameter of the civic). The hard found-block when the unhappy half is 0
    # (CvPlayer.cpp:6210) is enabler data, separate -- identity.cityLimit is its datum too.
    # iCivicPercentAnger IS convertible (ruling 12): unhappiness = V×10 × pop / 1000 = V per 100 city-pop
    # (CvPlayer.cpp:8577 ×10, CvCity.cpp:5624 /PERCENT_ANGER_DIVISOR=1000) -> a per-scaler, handled in curate().
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
    # TradeYieldModifiers is NOT here: the commerce channel (the only one authored -- trade routes yield
    # COMMERCE) merges into tradeRoutes.modifier (ruling 11); handled explicitly in curate().
    "CommerceModifiers":       ("empire", "",           "percent",      COMMERCES),
}
# --- per-scaler split arrays (ruling 4, info-rebuild.md + json.md §3.7): the value deposits flat, scaled by a
# count -- {value, per:"SPECIALIST"} (bare-string sugar, each=1). Legacy getSpecialistExtraCommerce: each city
# multiplies by ITS total specialist count of ALL types (CvCity.cpp:11810; NB the engine count is CITY-local).
# tag -> (scope, unit, valueKeys, perToken). ---
SPLIT_ARRAY_PER = {
    # CITY scope, not empire: the deposit LANDS per city and is scaled by THAT city's specialists. The civic's
    # empire-wide reach comes from the SOURCE (the player's adopted civics fold into every city's package), not
    # from the address -- so the scope driver is the deposit's own scope and the bare `per` then defaults to it
    # correctly (json.md §3.7). Authored at empire it would fold into the EMPIRE package, where no city is bound,
    # ask the tally for the empire-wide specialist count, and hand every city that whole number.
    "SpecialistExtraCommerces": ("empire", "flat", COMMERCES, _PER_SPECIALIST_IN_CITY, "cities"),
}
# --- CONDITIONED split arrays: emit {family}.<scope>.<unit> as a list entry {value, enabled:<predicate>} instead of a
# bespoke sub-scope member ([DEC-conditions-are-predicates], owner 2026-06-28). Capital-only modifiers were the legacy
# `empire.capital` member; they are now empire.percent + enabled:"IS_CAPITAL" (the cascade evaluates the predicate per
# city). tag -> (scope, unit, valueKeys, predicate). ---
SPLIT_ARRAY_COND = {
    "CapitalYieldModifiers":    ("empire", "percent", YIELDS,    "IS_CAPITAL"),
    "CapitalCommerceModifiers": ("empire", "percent", COMMERCES, "IS_CAPITAL"),
}
# --- CONDITIONED scalar families: emit {family}.<scope>[.<member>].<unit> as a conditioned
# {value, enabled:<predicate>} entry ([DEC-conditions-are-predicates], owner 2026-06-28).
# tag -> (family, scope, member|None, unit, predicate). ---
SCALAR_COND = {
    # Landmark happiness: BOTH signs gated on GAMEOPTION_MAP_PERSONALIZED (engine CvCity.cpp:5718 happy + :5665-5671
    # unhappy, same option block) — signed-split handles +/-. Retires the bespoke `landmark` member.
    "iLandmarkHappiness": ("happiness", "empire", None, "flat", "GAMEOPTION_MAP_PERSONALIZED"),
    # Home/other-area maintenance (ruling 2, info-rebuild.md): IS_HOME_AREA = the city's area is the capital's
    # area (engine CvArea::getTotalAreaMaintenanceModifier gates on isHomeArea, CvArea.cpp:828-835); "other
    # areas" is the plain negation. Retires the homeArea/otherArea condition-as-member authoring.
    "iHomeAreaMaintenanceModifier":  ("maintenance", "empire", None, "percent", "IS_HOME_AREA"),
    "iOtherAreaMaintenanceModifier": ("maintenance", "empire", None, "percent", "!IS_HOME_AREA"),
    # tradeRoutes variants are CONDITIONS on the `modifier` kind (rulings 11/17): foreign = the route partner
    # is another TEAM (engine gate getTeam() != otherTeam, CvCity::totalTradeModifier CvCity.cpp:11539);
    # sharedCivic = foreign AND the partner's owner runs this same civic (CvCity.cpp:11547-11552).
    "iForeignTradeRouteModifier":     ("tradeRoutes", "empire", "modifier", "percent", "IS_FOREIGN"),
    "iSharedCivicTradeRouteModifier": ("tradeRoutes", "empire", "modifier", "percent",
                                       OrderedDict([("all", ["IS_FOREIGN", "SHARES_CIVIC"])])),
}

# --- entity-keyed (target-keyed) maps: tag -> (family, scope, targetType, unit, valueKeys|None). ---
KEYED = {
    "BuildingHappinessChanges":     ("happiness",  "empire", "buildings",   "flat",    None),
    "BuildingHealthChanges":        ("health",     "empire", "buildings",   "flat",    None),
    "BuildingProductionModifiers":  ("buildRate",  "empire", "buildings",   "percent", None),
    "BuildingCommerceModifiers":    (None,         "empire", "buildings",   "percent", COMMERCES),  # split commerce
    "FeatureHappinessChanges":      ("happiness",  "empire", "features",    "flat",    None),
    "ImprovementHappinessChanges":  ("happiness",  "empire", "improvements","flat",    None),       # civic per-improvement happiness (#430 gap fix: was bundled into legacy getFeatureGoodHappiness, never curated)
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
    "bMilitaryFoodProduction": "militaryFoodProduction",
    "bNoLandmarkAnger": "noLandmarkAnger", "bCommunism": "communism", "bCanDoElection": "canDoElection",
    "bUpgradeAnywhere": "upgradeAnywhere", "bNoNonStateReligionSpread": "noNonStateReligionSpread",
    "bAllowInquisitions": "allowInquisitions", "bDisallowInquisitions": "disallowInquisitions",
    # Gate-1 data gap closed (code-cut-map audit + owner 2026-07-02): the civic grantor half of the signed
    # AllReligionsActiveCount pair + FreedomFighterCount (CvPlayer::processCivics :18143/:18245-46). The TRAIT
    # curator already emits all three; json.md §9: both grantors must emit. ZERO civics carry these flags in
    # shipped data today (base+modules) -- a zero-delta mapping migration.
    "bAllReligionsActive": "allReligionsActive", "bBansNonStateReligions": "bansNonStateReligions",
    "bFreedomFighter": "freedomFighter",
}
# --- the CITY-scope wellbeing gates -> `amenities` (json §8), NOT `policies`. A policy is a pure empire STATE;
# these confer something on a CITY, so they are amenities the civic grants to its cities. ⚑ `abolishedAnger` is
# the mechanic named WITHOUT the WHERE: the legacy `bNoCapitalUnhappiness` baked "capital" into the key, which is
# the condition-as-member shape [DEC-conditions-are-predicates] retires. It is the SAME gate the building side
# confers, so both carriers author the SAME key -- one mechanic, one name.
CITY_AMENITIES = {
    "bNoUnhealthyPopulation": "abolishedUnhealthFromPopulation",
    "bBuildingOnlyHealthy": "abolishedUnhealthFromBuildings",
}
# CONDITIONED amenities: the value is the §3.9 entry object rather than a bare true, so the grant carries its own
# gate. `bNoCapitalUnhappiness` is `abolishedAnger` RESTRICTED TO THE CAPITAL -- the legacy key baked the WHERE
# into its NAME ([DEC-conditions-are-predicates]); the mechanic is the same gate the building side confers, and
# the capital part is a CONDITION. Evaluated per city when the fold grants AND when it repeals.
CITY_AMENITIES_COND = {
    "bNoCapitalUnhappiness": ("abolishedAnger", "IS_CAPITAL"),
}

# --- capability LIST fields -> enables.{key}; each entry is <XType> + a bool value-element. ---
ENABLE_LISTS = {"SpecialistValids": "specialists", "Hurrys": "hurries", "SpecialBuildingNotRequireds": "specialBuildingsWaived"}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help", "Strategy": "strategy"}
IDENTITY = {"Upkeep": "upkeepLevel", "CivicOptionType": "civicOption", "iAnarchyLength": "anarchyLength",
            "WeLoveTheKing": "weLoveTheKing"}
# BonusCommerceModifiers: dropped, not captured anywhere (known gap). Plus prereq + double-author + dead caches.
DROP = {"TechPrereq", "BonusCommerceModifiers", "SpecialistCommercePercentChanges", "SpecialistYieldPercentChanges",
        "Categories", "isAnyImprovementYieldChange"}
FAMILY_ORDER = ["food", "production", "commerce", "gold", "research", "culture", "espionage", "yield",
                "happiness", "health", "growth", "experience", "greatPeopleRate", "greatGeneralRate",
                "freeSpecialists", "conscript", "capture", "unitProduction", "maintenance", "upkeep",
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
    cur = node.get(unit)
    if isinstance(cur, list):            # a conditioned entry already landed here -> keep the leaf a LIST (json §3.9),
        node[unit] = [val] + cur         # unconditioned scalar FIRST (enabled-absent before conditioned, §3.9 order)
    else:
        node[unit] = val


def _put_cond(fam, family, scope, unit, value, enabled, member=None):
    """Append a CONDITIONED deposit {value, enabled:<predicate>} to a scope-wide (or member) leaf, merging with any
    unconditioned scalar already there into a list (json §3.9). The doc-covered shape for a state-gated modifier
    ([DEC-conditions-are-predicates]): a capital-only modifier is empire.percent + enabled:"IS_CAPITAL", NOT a bespoke
    empire.capital member — the cascade's normal scope walk evaluates the predicate per-city."""
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if member:
        node = node.setdefault(member, {})
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def _put_per(fam, family, scope, unit, value, per, target=None):
    """Append a PER-SCALED deposit {value, per:<count token>} (json §3.7) to a scope-wide leaf, list-merging like
    _put_cond so it coexists with a plain scalar on the same leaf.
    `target` names a PLURAL target (json §3.3) the deposit lands on -- `cities` = every city in the scope."""
    node = fam.setdefault(family, {}).setdefault(scope, {})
    if target:
        node = node.setdefault(target, {})
    entry = OrderedDict([("value", value), ("per", per)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


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
    amenities = {}
    leftover = []
    # ruling 26 pre-read: the over-limit deposit needs to know whether THIS civic carries a base limit (the
    # SOURCE-resolved CITY_LIMIT token reads the depositing civic's own limit; a V with no own limit has no
    # transcription under the per-source model -> left on the legacy member + warned).
    _cl = rec.find("iCityLimit")
    _city_limit = int(engine.text(_cl)) if _cl is not None and engine.is_int(engine.text(_cl)) else 0
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
                if tag in cc.RATIO_AS_PERCENT:
                    v = cc.ratio_to_percent(tag, v)
                _put(fam, family, scope, member, unit, v)
        elif tag == "iLargestCityHappiness":
            v = _num(t)
            if v not in (None, 0, 0.0):   # happiness in the empire's LARGEST cities -> ranked `cities` target (top-N by
                node = fam.setdefault("happiness", {}).setdefault("empire", {}).setdefault("cities", {})  # population),
                node["flat"] = v; node["max"] = "TARGET_NUM_CITIES"; node["orderedByDescending"] = "CITY_SIZE"  # json §3.3, retires the bespoke `largestCity` member ([DEC-conditions-are-predicates])
        elif tag == "TradeYieldModifiers":
            # rulings 11 + 27: ALL channels author as tradeRoutes.modifier carrying the CHANNEL axis
            # (tradeRoutes.<scope>.modifier.<channel>.<unit>, channel = the YieldTypes family word -- the
            # shrine:{gold:N} / experience.city.domains.{DOMAIN_X} keyed-axis convention). Engine: the
            # per-channel player accumulator m_aiTradeYieldModifier (civic feeder CvPlayer.cpp:18058) applied
            # at CvCity::calculateTradeYield (CvCity.cpp:11645-11648): tradeYield[ch] = profit x mod[ch]/100,
            # identity 100 carried by the base CvYieldInfo TradeModifier (commerce 100, food/production 0) --
            # the deposits ride ON TOP of the engine's incoming route yield per channel (the modifier.md 2a
            # tradeYield input fold). The channel-AGNOSTIC route modifiers (building iTradeRouteModifier,
            # the IS_FOREIGN/SHARES_CIVIC conditioned entries below -- the CvCity::totalTradeModifier stage)
            # stay channel-less on modifier.percent: they scale the route PROFIT before the channel split.
            for ident, v in engine.named_array(c, YIELDS).items():
                (fam.setdefault("tradeRoutes", {}).setdefault("empire", {}).setdefault("modifier", {})
                 .setdefault(ident, {}))["percent"] = v
        elif tag == "iCityLimit":
            v = _num(t)
            if v not in (None, 0, 0.0):
                # ruling 26: the civic's BASE LIMIT is config data -- identity.cityLimit (the anarchyLength
                # convention: a scalar rule parameter of the civic). The CITY_LIMIT token resolver reads it
                # x CvWorldInfo::getCityLimitsScalePercent()/100 (C++ worklist); the CvPlayer.cpp:6210 hard
                # found-block (V == 0 case) reads the same datum as enabler data.
                identity["cityLimit"] = int(v)
        elif tag == "iCityOverLimitUnhappy":
            v = _num(t)
            if v not in (None, 0, 0.0):
                # ruling 26: V unhappiness per city OVER the civic's (world-size-scaled) limit -- engine
                # CvCity.cpp:5665-5674: unhappiness += V x max(0, numCities - scaledLimit), option-gated
                # (see the SCALAR note above). ONE entry on the first-class per.above scaler:
                if _city_limit:
                    node = fam.setdefault("happiness", {}).setdefault("empire", {})
                    entry = OrderedDict([("value", -v),
                                         ("per", OrderedDict([("type", "CITY"), ("above", "CITY_LIMIT")])),
                                         ("enabled", "GAMEOPTION_EXP_OVEREXPANSION_PENALTIES")])
                    cur = node.get("flat")
                    node["flat"] = [entry] if cur is None else (cur + [entry] if isinstance(cur, list) else [cur, entry])
                else:
                    leftover.append("iCityOverLimitUnhappy(V without own iCityLimit -- no per-source transcription)")
                    _put(fam, "happiness", "empire", "cityOverLimit", "flat", v)
        elif tag in ("iFreeUnitUpkeepMilitaryPopPercent", "iFreeUnitUpkeepCivilianPopPercent"):
            v = _num(t)
            if v not in (None, 0, 0.0):
                # batch-4/4b owner ruling (free-upkeep model change + sign convention): the pop-scaled free
                # unit upkeep converts UNCONDITIONALLY as a per-population deposit on the freeMilitary/
                # freeCivilian kinds under the ONE FREE-AMOUNT CONVENTION -- positive = free upkeep GRANTED,
                # negative = free allowance REDUCED, entries sum, the engine nets max(0, upkeep - SUMfree)
                # with the >=0 floors as FAMILY-COMBINE FLOOR METADATA (the modifier.md 2 min-member
                # mechanism; combine-side C++ leg pending). So the entry carries the legacy P's OWN sign:
                # {P, per:{POPULATION, each:100}} (engine PER_100_POP semantics -- the source's marginal
                # free upkeep is pop x P / 100, CvPlayer.cpp:10218-10226 via
                # getModifiedIntValue(totalPopulation, P): P>0 grants, P<0 shrinks). INTENTIONAL model
                # change from the legacy getModifiedIntValue rounding (the asymmetric mod<0 branch
                # v x 100/(100-mod) is NOT chased -- additive linear is the ruled shape).
                kind = "freeMilitary" if tag == "iFreeUnitUpkeepMilitaryPopPercent" else "freeCivilian"
                node = fam.setdefault("upkeep", {}).setdefault("empire", {}).setdefault(kind, {})
                entry = OrderedDict([("value", v),
                                     ("per", OrderedDict([("type", "POPULATION"), ("each", 100)]))])
                cur = node.get("flat")
                node["flat"] = [entry] if cur is None else (cur + [entry] if isinstance(cur, list) else [cur, entry])
        elif tag == "iCivicPercentAnger":
            v = _num(t)
            if v not in (None, 0, 0.0):   # V unhappiness per 100 CITY population (engine V×10×pop/1000) ->
                # a NEGATIVE happiness per-scaler (ruling 12), on the `cities` TARGET because the count is
                # CITY-LOCAL (owner). A bare empire flat cannot express it in either direction: the empire
                # package has no city bound to count the population OF, and it rolls DOWN to every city, so one
                # city's population would scale the happiness of all of them. `cities` resolves PER CITY, which
                # is where both the count and the deposit belong -- the shape modifier.md §2b already rules for
                # the bonus case.
                _put_per(fam, "happiness", "empire", "flat", -v,
                         OrderedDict([("type", "POPULATION"), ("each", 100), ("scope", "city")]),
                         target="cities")
        elif tag == "iTaxRateUnhappiness":
            v = _num(t)
            if v not in (None, 0, 0.0):   # V unhappiness x goldRate/100 in every city (CvPlayer.cpp:26526
                # calculateTaxRateUnhappiness = getCommercePercent(COMMERCE_GOLD) * V / 100, consumed per city at
                # CvCity.cpp:5645) -> a NEGATIVE happiness deposit per GOLD_RATE, each=100 (ruling 20). Retires
                # the taxRate condition-as-member.
                _put_per(fam, "happiness", "empire", "flat", -v,
                         OrderedDict([("type", "GOLD_RATE"), ("each", 100)]))
        elif tag == "iForeignerUnhappyPercent":
            v = _num(t)
            if v not in (None, 0, 0.0):
                # V is a DIVISOR: engine anger = (100/V) * (100 - ownCulturePct) / 100 (CvCity.cpp:5650-5654 live,
                # :8664-8667 what-if twin, same math). Reciprocal precompute R = 100/V (ruling 22; exact only when
                # 100 % V == 0 -- holds for the single authored value, CIVIC_NATIONALIST V=10 -> R=10), then the
                # telescoping pair on the CULTURE_PERCENTAGE city counter: flat -R + {+R, per each 100} ==
                # -R*(100-ownPct)/100. An inexact V is NOT convertible -> kept on the legacy member + warned.
                if isinstance(v, int) and v > 0 and 100 % v == 0:
                    r = 100 // v
                    node = fam.setdefault("happiness", {}).setdefault("empire", {})
                    pair = [-r, OrderedDict([("value", r),
                                             ("per", OrderedDict([("type", "CULTURE_PERCENTAGE"), ("each", 100)]))])]
                    cur = node.get("flat")
                    node["flat"] = pair if cur is None else (cur + pair if isinstance(cur, list) else [cur] + pair)
                else:
                    leftover.append("iForeignerUnhappyPercent(100/V inexact: %s)" % v)
                    _put(fam, "happiness", "empire", "foreignerUnhappy", "percent", v)
        elif tag == "iNonStateReligionHappiness":
            v = _num(t)
            if v not in (None, 0, 0.0):   # +/-V per city religion that is NOT the state religion (engine
                # CvCity.cpp:9407-9418 getReligionHappiness per present religion; what-if twin :8689-8705) ->
                # the §3.7 predicate-filtered count in the `unit:`-qualifier pattern, religion-typed: the field
                # names the counted kind and holds the filter (ruling 23; "!IS_STATE_RELIGION" = §3.4 `!` sugar).
                node = fam.setdefault("happiness", {}).setdefault("empire", {}).setdefault("cities", {})
                entry = OrderedDict([("value", v), ("religion", "!IS_STATE_RELIGION")])
                cur = node.get("flat")
                node["flat"] = [entry] if cur is None else (cur + [entry] if isinstance(cur, list) else [cur, entry])
        elif tag == "iHappyPerMilitaryUnit":
            v = _num(t)
            if v not in (None, 0, 0.0):   # per stationed MILITARY unit -> the SPEC form: a `unit: IS_MILITARY`-
                node = fam.setdefault("happiness", {}).setdefault("empire", {}).setdefault("cities", {})  # qualified
                entry = OrderedDict([("value", v), ("unit", "IS_MILITARY")])   # entry on the `cities` target (json
                cur = node.get("flat")                                         # §3.7; retires the BANNED perMilitaryUnit
                node["flat"] = [entry] if cur is None else (cur + [entry] if isinstance(cur, list) else [cur, entry])  # member, DEC-conditions-are-predicates)
        elif tag in SCALAR_COND:
            v = _num(t)
            if v not in (None, 0, 0.0):
                family, scope, member, unit, pred = SCALAR_COND[tag]
                _put_cond(fam, family, scope, unit, v, pred, member)
        elif tag in STATE_RELIGION:
            v = _num(t)
            if v not in (None, 0, 0.0):
                member, unit = STATE_RELIGION[tag]
                _put(fam, "stateReligion", "empire", member, unit, v)
        elif tag in SPLIT_ARRAY:
            scope, member, unit, keys = SPLIT_ARRAY[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split)
                _put(fam, ident, scope, member, unit, v)
        elif tag in SPLIT_ARRAY_COND:
            scope, unit, keys, pred = SPLIT_ARRAY_COND[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split); predicate-gated deposit
                _put_cond(fam, ident, scope, unit, v, pred)
        elif tag in SPLIT_ARRAY_PER:
            scope, unit, keys, per_token, target = SPLIT_ARRAY_PER[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split); count-scaled deposit
                _put_per(fam, ident, scope, unit, v, per_token, target)
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
            # yield on LANDMARK plots (geographic landmark types — bay/forest/jungle/peak/...), gated on the
            # MAP_PERSONALIZED option AND the plot being a landmark (engine CvPlot.cpp:8420). A `plots`-target deposit
            # gated by HAS_LANDMARK + the option — retires the bespoke `landmark` yield member ([DEC-conditions-are-predicates]).
            for ident, v in engine.named_array(c, YIELDS).items():
                if v:
                    node = fam.setdefault(ident, {}).setdefault("empire", {}).setdefault("plots", {})
                    entry = OrderedDict([("value", v),
                                         ("enabled", OrderedDict([("all", ["GAMEOPTION_MAP_PERSONALIZED", "HAS_LANDMARK"])]))])
                    cur = node.get("flat")
                    if cur is None:
                        node["flat"] = [entry]
                    elif isinstance(cur, list):
                        cur.append(entry)
                    else:
                        node["flat"] = [cur, entry]
        elif tag in POLICIES:
            if t in ("1", "true", "True"):
                policies[POLICIES[tag]] = True
        elif tag in CITY_AMENITIES:
            if t in ("1", "true", "True"):
                amenities[CITY_AMENITIES[tag]] = True
        elif tag in CITY_AMENITIES_COND:
            if t in ("1", "true", "True"):
                key, cond = CITY_AMENITIES_COND[tag]
                amenities[key] = OrderedDict([("enabled", cond)])
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
    if amenities:
        out["amenities"] = OrderedDict((k, amenities[k]) for k in sorted(amenities))
    if grants:
        out["grants"] = grants
    if ai:
        out["ai"] = ai
    cc.emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    cc.fold_text_to_identity(out)   # TEXT -> identity (json.md §7)
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
                                "policies", "amenities", "grants", "ai", "ui", "world", "sound", "identity")})
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
