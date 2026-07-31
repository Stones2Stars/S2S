#!/usr/bin/env python3
"""Curate Trait (#428) — "Mount Doom": ONE CvTraitInfo class serving BOTH trait systems. BESPOKE.

There is a SINGLE trait class. The "developing/complex leaders" system is the SAME CvTraitInfo, just
assigned differently: CvLeaderHeadInfo carries DefaultTraits AND DefaultComplexTraits (both lists of
TraitTypes into this one table), and CvPlayer picks the list by GAMEOPTION_LEADER_COMPLEX_TRAITS
(CvPlayer.cpp:439, :1583). So assignment (fixed vs accumulating) is CONSUMER-side; the data shape is one
uniform `trait` surface. A trait is an EMPIRE-wide SOURCE/ENABLER (deposits via CvPlayer::processTrait into
player-level accumulators) — so virtually every modifier is `empire` scope, and nothing ever TARGETS a trait.

ROOT CAUSE + intended shape (owner 2026-06-21): the complex/replacement system exists only because the original
author reached for a generic RUNTIME SWAP (`CvInfoReplacements`: a base trait carries a `ReplacementID` +
`ReplacementCondition`, and the engine OVERWRITES the whole Info when a game option flips) instead of defining a
complex trait as its OWN object on the same base abstraction. With no composition/inheritance/contract, "which
trait set is active" became a condition buried in the data rather than a composition-root choice. Splitting
simple/ + complex/ into two COMPLETE, INDEPENDENT sets (the complex set self-contains its effective values via
the merge below) is the prerequisite for the proper end state: complex traits become their own Info type behind
a shared trait CONTRACT, chosen at the composition root (`if(complexTraits)` inject the complex impl), NOT a
roundabout replacement. The two systems must live fully separate before that code untangle is possible.

Field dispositions verified by the classify-trait workflow (wf_cc8659b5: 3 ground-truth agents + 6 field
slices, each adversarially verified, + a coverage/conflict/conditioned-on-source/dev-leader audit) against CvTraitInfo.{h,cpp}
+ CvPlayer::processTrait + CvCity/CvGameTextMgr consumers. Full analysis:
Tools/Migration/classifications/trait-classification.json. Conventions MIRROR curate_civic.py (the first heavy
entity owns them: production vs unitProduction split, the grouped `stateReligion` family, PropertyManipulators
parsed into per-PROPERTY_* families). OWNER RULINGS (2026-06-14) drive the structural calls:

- DEV-LEADERS relations -> top-down/derived, NOT trait-side prereqs:
  * TraitPrereq / TraitPrereqOr1 / TraitPrereqOr2 (trait->trait) and PrereqTech (tech->trait) INVERT via the
    store into `enables.traits` on the prereq trait / tech (registered in store.PREREQ_FIELDS) -> DROPPED here.
  * DisallowedTraitTypes -> a same-tier `excludes` set (author one end; the symmetric reverse is derived into
    the cold-path reverse index). NOT the #429 SPATIAL sideways — a structural mutual-exclusion.
  * OnGameOptions / NotOnGameOptions -> DROPPED (owner ruling 2026-07-08): every authoring was
    GAMEOPTION_LEADER_COMPLEX_TRAITS restating the simple/complex FOLDER split, which IS the selection
    mechanism (traits/simple/ vs traits/complex/, option-selected at the composition root). The `loadPrune`
    section it fed was a curator-era invention, retired whole (superseded-ideas.md).
  * PromotionLine + LinePriority -> a `succession` block (the developing-leaders line ordering); the line's own
    PrereqTech is a DERIVED tech enabler owned by PromotionLineInfo (store), not re-authored here.
  * Categories -> DROP (dead: zero C++ readers; not authored in either trait XML; civic drops it too).
  * the culture-requirement progression (GAMEOPTION_NEXT_TRAIT_CULTURE_REQ_PERCENT) is a GAME OPTION, not a
    trait field -> not authored on the trait.
- conditioned-on-source: BonusHappinessChanges (the only fresh bonus-conditioner) is authored ON THE TRAIT (keep-on-source,
  modifier-spec §6 — supersedes the old "fold onto the bonus" rule; a resource is never a target): the trait
  grants +N happiness while a specific bonus is present. The deposit is `happiness.empire.flat` (scope = the
  MODIFIER's: the trait benefits the player's cities); the condition is the agreed full+explicit enabler atom
  (enabler-spec §6.1/§13.7) `enabled:{type:BONUS_X, scope:empire, min:1}` ("we have ≥1", a tally count read).
  TechResearchModifiers -> `researchRate.empire.techs.{TECH}.percent` (TARGET-KEYED by tech). researchRate is the
  research-RATE analogue of buildRate (owner 2026-06-28): "+% beakers when researching tech X", a research-RATE/beaker
  modifier (getNationalTechResearchModifier -> calculateBaseNetResearch, CvPlayer.cpp:8214) — DISTINCT from the
  commerce `research` family (the commerce cascade reads commerce `research`, never `researchRate`). Keyed exactly like
  buildRate.empire.buildings.{X}. (A 2026-06-28 mis-retire into commerce research.empire.percent inflated the commerce
  modifier ~10x and was reverted.)
- GreatPeopleUnitType + GreatPeopleRateChange FOLD into greatPeopleRate.empire.units.{UNIT}.flat (the change is
  keyed by the GP unit; CvPlayer.cpp:28606-28610, only when >0).
- CityStartCulture + BonusPopulationinNewCities -> a `cityFounding` family (standing empire accumulators applied
  at every city founding, NOT one-shot grants).
- SpecialistYield/CommerceChanges -> KEEP trait-side, keyed by specialist (yield/commerce.empire.specialists.{SPEC}.flat,
  governing-deliverer) -- NOT inverted onto the shared specialist, because the simple/complex sets carry different
  per-set values for the same specialist (Option-B ruling 2026-06-25; see modifier.md trait callout). curate_specialist
  no longer folds them in.
- MaxAnarchy/MinAnarchy -> clean identity keys (maxAnarchy default -1 carried verbatim; min default 0).
- SCALE (verified 2026-06-22 at the consumption site, scale-registry method): NO trait field is x100. Every
  SCALAR deposits via CvPlayer::processTrait as `change<X>(iChange * get<X>())` -- multiplied ONLY by iChange (+/-1),
  never x100; CvTraitInfo exposes ZERO `...100()` accessors; the lone x100 in processTrait is
  `changeExtraCommerce100(100*iChange*getCommerceChange)`, whose explicit human->x100 scale-up PROVES those inputs
  are human. So all SCALARs are emitted RAW (human) via _num(t) -- CORRECT, no PER100 descale (the #432 de-scale
  does not apply to traits). fRev* are floats carried verbatim.

  python3 curate_trait.py --sample TRAIT_PHILOSOPHICAL TRAIT_FINANCIAL
  python3 curate_trait.py --write
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
    # wellbeing
    "iHealth":                         ("health", "empire", "", "flat"),
    "iHappiness":                      ("happiness", "empire", "", "flat"),
    "iNonStateReligionHappiness":      ("happiness", "empire", "nonStateReligion", "flat"),
    "iGlobalPopulationgrowthratepercentage": ("growth", "empire", "", "percent"),
    # great people / generals
    "iGreatPeopleRateModifier":        ("greatPeopleRate", "empire", "", "percent"),
    "iGreatGeneralRateModifier":       ("greatGeneralRate", "empire", "", "percent"),
    "iDomesticGreatGeneralRateModifier":("greatGeneralRate", "empire", "domestic", "percent"),
    "iFreeSpecialist":                 ("freeSpecialists", "empire", "", "any"),
    # build-rate (owner 2026-06-16: production=total city OUTPUT; buildRate=faster to build a target/category).
    "iMilitaryProductionModifier":     ("buildRate", "empire", "military", "percent"),
    "iMaxGlobalBuildingProductionModifier": ("buildRate", "empire", "worldWonder", "percent"),
    "iMaxTeamBuildingProductionModifier":   ("buildRate", "empire", "teamWonder", "percent"),
    "iMaxPlayerBuildingProductionModifier": ("buildRate", "empire", "nationalWonder", "percent"),
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
    # combat / defense
    # L13 re-home (2026-07-05): the spec'd DEFENSE family (modifier.md §6), matching buildings'
    # defense.empire.amount -- the old combat.empire.cityDefense had NO reader.
    "iCityDefenseBonus":               ("defense", "empire", "amount", "percent"),
    # L13 re-home (2026-07-05): trait iBombardDefense feeds m_iNationalBombardDefenseModifier
    # (processTrait, CvPlayer.cpp:28613) -> the DEFENSE family's bombardDefense member; the old
    # combat.empire.bombardDefense had NO reader (the getBuildingBombardDefense national leg was dropped).
    "iBombardDefense":                 ("defense", "empire", "bombardDefense", "percent"),
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
    # SeaPlotYieldChanges -> a PLOTS-TARGET fold (owner 2026-06-22): empire.plots.flat {IS_WATER}; handled in the apply loop.
    "GoldenAgeYieldChanges":    ("empire", "goldenAge", "flat",         YIELDS),
    "CommerceChanges":          ("empire", "",          "flat",         COMMERCES),
    "CommerceModifiers":        ("empire", "",          "percent",      COMMERCES),
    "GoldenAgeCommerceChanges": ("empire", "goldenAge", "flat",         COMMERCES),
}
# --- per-scaler split arrays (json §3.7): the value deposits FLAT, scaled by a count -- {value, per:"SPECIALIST"}
# (bare-string sugar, each=1). CITY scope, not empire: the deposit LANDS per city and scales by THAT city's
# specialists, which is the count the engine takes. The trait's empire-wide reach comes from the SOURCE (a held
# trait folds into every city's package), never from the address -- so the deposit's own scope is the scope driver
# and the bare `per` defaults to it correctly. tag -> (scope, unit, valueKeys, perToken). ---
SPLIT_ARRAY_PER = {
    "SpecialistExtraYields":    ("empire", "flat", YIELDS,    _PER_SPECIALIST_IN_CITY, "cities"),
    "SpecialistExtraCommerces": ("empire", "flat", COMMERCES, _PER_SPECIALIST_IN_CITY, "cities"),
}
# --- CONDITIONED arrays/scalars: emit {family}.<scope>.<unit> as a list entry {value, enabled:<predicate>} instead of
# a bespoke sub-scope member ([DEC-conditions-are-predicates], owner 2026-06-28). Capital-only modifiers were the
# legacy `empire.capital` member -> now empire.percent + enabled:"IS_CAPITAL" (cascade evaluates the predicate per
# city). (GoldenAge* stays a `goldenAge` member — deferred engine-core exception, golden-age.md.) ---
SPLIT_ARRAY_COND = {
    "CapitalYieldModifiers":    ("empire", "percent", YIELDS,    "IS_CAPITAL"),
    "CapitalCommerceModifiers": ("empire", "percent", COMMERCES, "IS_CAPITAL"),
}
SCALAR_COND = {
    "iCapitalXPModifier": ("experience", "empire", "percent", "IS_CAPITAL"),
    # holy city of a NON-state religion (engine CvCity.cpp:3250 = isHolyCity() && !isHolyCity(stateReligion)) ->
    # composed predicate via the `!` NOT operator (owner 2026-06-28), retiring the bespoke `nonStateHolyCityXP` member.
    "iHolyCityofNonStateReligionXPModifier": ("experience", "empire", "percent",
        OrderedDict([("all", ["IS_HOLY_CITY", "!IS_STATE_RELIGION_HOLY_CITY"])])),
}
# --- positional yield arrays kept GROUPED under their own family (yield is the member, not the family). ---
GROUPED_YIELD_ARRAY = {
    "ExtraYieldThresholds": ("extraYieldThreshold", "empire", "flat", YIELDS),
    "LessYieldThresholds":  ("lessYieldThreshold",  "empire", "flat", YIELDS),
}

# --- entity-keyed (target-keyed) maps: tag -> (family, scope, targetType, unit, valueKeys|None). ---
#   keys != None  => SPLIT (each named yield/commerce becomes its own top-level family).
#   unit "enabler" => the entry value is a bool flag -> emit true.
#   TechResearchModifiers -> researchRate.empire.techs.{TECH}.percent (target-keyed by tech). researchRate is the
#   research-RATE analogue of buildRate (owner 2026-06-28): it feeds the beaker rate via getNationalTechResearchModifier
#   -> calculateBaseNetResearch (CvPlayer.cpp:8214), DISTINCT from the commerce `research` family — so the commerce
#   cascade never reads it (no regression; a 2026-06-28 mis-retire into commerce research.empire.percent inflated the
#   modifier ~10x and was reverted). Target-keyed exactly as buildRate.empire.buildings.{X} — not a bespoke member.
KEYED = {
    "ImprovementYieldChanges":            (None,             "empire", "improvements",   "flat",    YIELDS),
    "ImprovementUpgradeModifierTypes":    ("improvementUpgradeRate", "empire", "improvements", "percent", None),
    "BuildWorkerSpeedModifierTypes":      ("workRate",       "empire", "builds",         "percent", None),
    "DomainFreeExperiences":              ("experience",     "empire", "domains",        "flat",    None),
    "DomainProductionModifiers":          ("buildRate",      "empire", "domains",        "percent", None),
    "BuildingProductionModifierTypes":    ("buildRate",      "empire", "buildings",      "percent", None),
    "SpecialBuildingProductionModifierTypes": ("buildRate", "empire", "specialBuildings","percent", None),
    "BuildingHappinessModifierTypes":     ("happiness",      "empire", "buildings",      "flat",    None),
    "UnitProductionModifierTypes":        ("buildRate",      "empire", "units",          "percent", None),
    "SpecialUnitProductionModifierTypes": ("buildRate",      "empire", "specialUnits",   "percent", None),
    "UnitCombatFreeExperiences":          ("experience",     "empire", "unitCombats",    "flat",    None),
    "UnitCombatProductionModifiers":      ("buildRate",      "empire", "unitCombats",    "percent", None),
    # A civic option whose upkeep this trait WAIVES. Emitted as the magnitude that says so (-100%), never a
    # bare `enabler` bool: the modifier plane carries magnitudes, and a boolean keyed by a target is neither a
    # kind nor a `policy` (json.md §9: a policy is a PURE STATE, never parameterized by a target).
    "CivicOptionNoUpkeepTypes":           ("upkeep",         "empire", "civicOptions",   "percent", None),
    "TechResearchModifiers":              ("researchRate",   "empire", "techs",          "percent", None),  # research-RATE "+% to research tech X" — researchRate is the research analogue of buildRate (owner 2026-06-28), TARGET-KEYED by tech (researchRate.empire.techs.{TECH}.percent) exactly as buildRate.empire.buildings.{X}; DISTINCT from commerce `research` (cascade never reads researchRate). Retires the `byTech` member invention.
    # Specialist yield/commerce boosts STAY on the trait, keyed by the specialist (governing-deliverer), NOT inverted
    # onto the shared specialist -- the simple/complex sets carry DIFFERENT per-set values, so inverting onto the ONE
    # specialist file would break the clean separation (modifier.md trait callout, owner ruling 2026-06-25). The
    # simple folder gets the base value, complex the merged-effective; the cascade reads the active set x count.
    "SpecialistYieldChanges":             (None,             "empire", "specialists",    "flat",    YIELDS),
    "SpecialistCommerceChanges":          (None,             "empire", "specialists",    "flat",    COMMERCES),
}

# --- boolean policy flags -> policies.{name}: true. PURE empire STATES only (json.md §9): a policy is a state a civic/
# trait enacts, NEVER an effect/parameterized rule. (nonStateReligionCommerce is a Free-Church permission STATE -- it
# stays; the FreeSpecialistPer* keys are EFFECTS -> FREE_SPEC_PER_WONDER below, not here.) ---
POLICIES = {
    "bNonStateReligionCommerce": "nonStateReligionCommerce", "bUpgradeAnywhere": "upgradeAnywhere",
    "bMilitaryFoodProduction": "militaryFoodProduction", "bAllowsInquisitions": "allowInquisitions",
    "bCitiesStartwithStateReligion": "citiesStartWithStateReligion", "bDraftsOnCityCapture": "draftsOnCityCapture",
    "bExtraGoody": "extraGoody",
    "bAllReligionsActive": "allReligionsActive", "bBansNonStateReligions": "bansNonStateReligions",
    "bFreedomFighter": "freedomFighter",
}
# --- FreeSpecialistPer{Wonder,Project}: EFFECTS, not policies (owner 2026-07-01) -> a `freeSpecialists` MODIFIER scaled
# per wonder count (CvCity:5764: +1 free specialist of any type per world/national/team wonder in each city). Emitted as
# freeSpecialists.empire.any list entries {value:1, per:{type:<WONDER_TOKEN>, scope:city}} -- the ordinary §3.7 per-count
# scaler. The count tokens are WORLD_WONDER / NATIONAL_WONDER / TEAM_WONDER -- the existing engine terms (json.md §3.1
# count registry, UPPER_SNAKE like POPULATION/ERA; pedia display names can be aligned later). Coexists with iFreeSpecialist. ---
FREE_SPEC_PER_WONDER = {
    "bFreeSpecialistperWorldWonder":    "WORLD_WONDER",
    "bFreeSpecialistperNationalWonder": "NATIONAL_WONDER",
    "bFreeSpecialistperTeamProject":    "TEAM_WONDER",
}
# --- boolean flags -> identity (intrinsic "what am I", not a player-state policy). ---
IDENTITY_FLAGS = {
    "bNegativeTrait": "negativeTrait", "bImpurePropertyManipulators": "impurePropertyManipulators",
    "bImpurePromotions": "impurePromotions", "bCivilizationTrait": "civilizationTrait",
    "bBarbarianSelectionOnly": "barbarianSelectionOnly",
}
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help", "Strategy": "strategy"}
# DROPs: prereqs (-> store enables), dead. (BonusHappinessChanges is NO LONGER dropped — it is authored on the trait,
# gated by bonus presence; handled in the loop + merge below.) SpecialistYield/CommerceChanges are NO LONGER dropped
# either — they now STAY trait-side keyed by specialist (KEYED above), per the Option-B ruling (the simple/complex
# per-set value cannot be inverted onto the one shared specialist file).
DROP = {"Type", "TraitPrereq", "TraitPrereqOr1", "TraitPrereqOr2", "PrereqTech", "Categories"}
FAMILY_ORDER = ["food", "production", "buildRate", "researchRate", "commerce", "gold", "research", "culture", "espionage",
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
    cur = node.get(unit)
    if isinstance(cur, list):            # a conditioned entry already landed here -> keep the leaf a LIST (json §3.9),
        node[unit] = [val] + cur         # unconditioned scalar FIRST (enabled-absent before conditioned, §3.9 order)
    else:
        node[unit] = val


def _put_per(fam, family, scope, unit, value, per, target=None):
    """Append a PER-SCALED deposit {value, per:<count token>} (json §3.7) to a scope-wide leaf, list-merging like
    _put_cond so it coexists with a plain scalar on the same leaf. Mirrors curate_civic/_curate_building.
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


def _put_cond(fam, family, scope, unit, value, enabled):
    """Append a CONDITIONED deposit {value, enabled:<predicate>} to a scope-wide leaf, merging with any unconditioned
    scalar already there into a list (json §3.9) — the same shape as the SeaPlotYieldChanges IS_WATER fold below. The
    doc-covered shape for a state-gated modifier ([DEC-conditions-are-predicates]): a capital-only modifier is
    empire.percent + enabled:"IS_CAPITAL", NOT a bespoke empire.capital member."""
    node = fam.setdefault(family, {}).setdefault(scope, {})
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


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
    text, fam, props, policies, grants, art_blocks, identity, ai = {}, {}, {}, {}, {}, {}, {}, {}
    excludes, succession = [], {}
    gp_unit, gp_change = None, None
    bonus_happy = OrderedDict()
    free_spec_wonder = []            # FreeSpecialistPer* -> freeSpecialists.empire.any per-wonder deposits (below)
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
        elif tag == "iLargestCityHappiness":
            v = _num(t)
            if v not in (None, 0, 0.0):   # happiness in the empire's LARGEST cities -> ranked `cities` target (top-N by
                node = fam.setdefault("happiness", {}).setdefault("empire", {}).setdefault("cities", {})  # population),
                node["flat"] = v; node["max"] = "TARGET_NUM_CITIES"; node["orderedByDescending"] = "CITY_SIZE"  # json §3.3, retires the bespoke `largestCity` member ([DEC-conditions-are-predicates])
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
                family, scope, unit, pred = SCALAR_COND[tag]
                _put_cond(fam, family, scope, unit, v, pred)
        elif tag in STATE_RELIGION:
            v = _num(t)
            if v not in (None, 0, 0.0):
                member, unit = STATE_RELIGION[tag]
                _put(fam, "stateReligion", "empire", member, unit, v)
        elif tag in SPLIT_ARRAY:
            scope, member, unit, keys = SPLIT_ARRAY[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split)
                _put(fam, ident, scope, member, unit, v)
        elif tag in SPLIT_ARRAY_PER:
            scope, unit, keys, per_token, target = SPLIT_ARRAY_PER[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split); count-scaled deposit
                _put_per(fam, ident, scope, unit, v, per_token, target)
        elif tag in SPLIT_ARRAY_COND:
            scope, unit, keys, pred = SPLIT_ARRAY_COND[tag]
            for ident, v in engine.named_array(c, keys).items():   # ident IS the family (split); predicate-gated deposit
                _put_cond(fam, ident, scope, unit, v, pred)
        elif tag == "SeaPlotYieldChanges":
            # PLOTS-TARGET fold (owner 2026-06-22): the empire sea-plot yield -> the explicit `plots` target {IS_WATER}
            # (data-model §4.1/§6). Deposits onto every water plot worked in the empire; retires getSeaPlotYield + seaPlot.
            for ident, v in engine.named_array(c, YIELDS).items():
                if v:
                    node = fam.setdefault(ident, {}).setdefault("empire", {}).setdefault("plots", {})
                    entry = OrderedDict([("value", v), ("enabled", "IS_WATER")])
                    cur = node.get("flat")
                    if cur is None:
                        node["flat"] = [entry]
                    elif isinstance(cur, list):
                        cur.append(entry)
                    else:
                        node["flat"] = [cur, entry]
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
                    # A membership-list tag carries no magnitude of its own -- the LIST is the assertion ("this
                    # target is waived"). Emit the magnitude that states it, so the entry is an ordinary modifier
                    # deposit rather than a bool the modifier plane cannot read.
                    if tag == "CivicOptionNoUpkeepTypes":
                        out_val = -100
                    (fam.setdefault(family, {}).setdefault(scope, {}).setdefault(tt, {})
                     .setdefault(target, {}))[unit] = out_val
        elif tag in POLICIES:
            if t in ("1", "true", "True"):
                policies[POLICIES[tag]] = True
        elif tag in FREE_SPEC_PER_WONDER:                 # EFFECT (not a policy) -> a freeSpecialists per-wonder modifier
            if t in ("1", "true", "True"):
                free_spec_wonder.append(OrderedDict([
                    ("value", 1),
                    ("per", OrderedDict([("type", FREE_SPEC_PER_WONDER[tag]), ("scope", "city")]))]))
        elif tag in IDENTITY_FLAGS:
            if t in ("1", "true", "True"):
                identity[IDENTITY_FLAGS[tag]] = True
        elif tag == "DisallowedTraitTypes":
            excludes = _type_list(c)
        elif tag in ("OnGameOptions", "NotOnGameOptions"):
            pass   # DROPPED (owner 2026-07-08): restates the simple/complex folder split -- see the docstring
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
            cc.put_art(art_blocks, tag, engine.generic(c))   # -> ui.art.icon via ART_BLOCK
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

    # GP-rate change (CvPlayer.cpp:~28698, legacy gate `if (iGPRateChange > 0)`): a type-specified change keys
    # by its GP unit; a NO_UNIT change is the UNTYPED national rate -- the legacy NO_UNIT path still adds it and
    # the GP engine assigns it to a pool (owner 2026-07-05: "not-type-specified GP rate must go through, the GP
    # engine deals with placement"). Emit unkeyed `greatPeopleRate.empire.flat` so the cascade national fold
    # picks it up -- was being DROPPED (the `if gp_unit` gate), the L6 residual.
    if gp_change and gp_change > 0:
        gpr = fam.setdefault("greatPeopleRate", {}).setdefault("empire", {})
        if gp_unit:
            gpr.setdefault("units", {}).setdefault(gp_unit, {})["flat"] = gp_change
        else:
            gpr["flat"] = gpr.get("flat", 0) + gp_change   # NO_UNIT -> the untyped national pool

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

    # FreeSpecialistPer* -> freeSpecialists.empire.any per-wonder deposits (owner 2026-07-01; EFFECTS, not policies).
    # +1 free specialist of ANY type per world/national/team wonder in each city (CvCity:5764). Coexists in the `any`
    # leaf with a bare iFreeSpecialist count (merged into a list, json §3.9).
    if free_spec_wonder:
        node = fam.setdefault("freeSpecialists", {}).setdefault("empire", {})
        existing = node.get("any")
        entries = (existing if isinstance(existing, list) else [existing]) if existing is not None else []
        entries.extend(free_spec_wonder)
        node["any"] = entries

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
    table = store.table("TraitInfo")
    repl_map = store.replacements.get("TraitInfo", {})        # {baseType: {"replacement": rid, "condition": elem}}
    complex_ids = set(v["replacement"] for v in repl_map.values())
    rid_to_base = {v["replacement"]: b for b, v in repl_map.items()}   # replacement type -> the base it overwrites
    results, all_leftover, folders = OrderedDict(), set(), {}
    for typ, rec in table.items():
        obj, leftover = curate(typ, rec, store)
        results[typ] = obj
        folders[typ] = "complex" if is_complex(typ, rec, complex_ids) else "simple"
        all_leftover.update(leftover)
    nc = sum(1 for f in folders.values() if f == "complex")
    print("TraitInfo curated: %d  (simple=%d, complex=%d)" % (len(results), len(results) - nc, nc))
    STRUCT = {"type", "description", "civilopedia", "help", "strategy", "enables", "replacedBy", "excludes",
              "policies", "grants", "succession", "ai", "ui", "world", "sound", "identity"}
    fams = sorted({k for o in results.values() for k in o if k not in STRUCT and not k.startswith("PROPERTY_")})
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("  families seen: %s" % ", ".join(fams))
    for k in ("enables", "replacedBy", "excludes", "succession", "grants", "policies", "ai"):
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

        def _write(folder, typ, obj):
            d = os.path.join(out_dir, folder)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)

        # Fresh dirs so types that are no longer emitted standalone (the replacement records, now MERGED into
        # complex/<base>) don't linger as stale files.
        # ⛔ But NEVER when the curator produced nothing: traits are content-LOCKED and hand-maintained
        # (modifier.md), so a zero result means there is no input to rewrite from -- clearing would destroy the
        # authored set rather than refresh it.
        if not results:
            print("REFUSED to clear the trait folders: the curator produced 0 entities.")
            return
        for sub in ("simple", "complex"):
            d = os.path.join(out_dir, sub)
            if os.path.isdir(d):
                for fn in os.listdir(d):
                    if fn.endswith(".json"):
                        os.remove(os.path.join(d, fn))
        nwritten = 0
        for typ, obj in results.items():
            if typ in rid_to_base:
                continue   # standalone replacement record -> subsumed into complex/<base> below; don't emit alone
            _write(folders[typ], typ, obj)                    # base/plain -> simple/ (or complex-only -> complex/)
            nwritten += 1
            if typ in repl_map:
                # base trait WITH a complex replacement: emit the complex def keyed by the BASE type (the type the
                # player actually holds) so the calc picks it by GAMEOPTION. WHOLE-SWAP, NO base-fill (owner ruling
                # 2026-06-25, SUPERSEDING the 2026-06-21 fill-from-base): the engine's CvInfoReplacements swaps the
                # WHOLE CvTraitInfo, so a field the replacement OMITS is 0/absent in the engine -- it is NOT inherited
                # from base. Matching that literally is required for parity (base SPIRITUAL gives PRIEST a specialist
                # yield the complex replacement drops -> engine perType 0; fill-from-base wrongly kept it). So the
                # complex/ def IS the replacement's curated Info ENTIRELY, merely re-keyed to the base type. The
                # complex/ set stays SELF-COMPLETE + INDEPENDENT of simple/ (the replacement Info is itself complete).
                rid = repl_map[typ]["replacement"]
                merged = OrderedDict(results.get(rid, OrderedDict()))
                merged["type"] = typ
                _write("complex", typ, merged)
                nwritten += 1
            elif folders[typ] == "simple":
                # SELF-COMPLETE COMPLEX (owner ruling 2026-07-21): a simple trait with NO complex replacement is ALSO
                # base-filled into complex/ (its whole def, identical) so the complex folder is a SUPERSET of simple.
                # The option-gated active-set read (getTraitInfo / MMKernel::traitData) then NEVER falls back to a
                # simple record under GAMEOPTION_LEADER_COMPLEX_TRAITS -- ② can make that read fail-loud with nothing
                # to fall back to. Distinct from the whole-swap above: there is no replacement to swap to, so the base
                # IS the complex version (e.g. TRAIT_BARBARIAN, the NPC-civ trait -- the only such case today).
                _write("complex", typ, obj)
                nwritten += 1
        print("\nwrote %d TraitInfo JSON files under Assets/Data/traits/{simple,complex}/" % nwritten)


if __name__ == "__main__":
    main()
