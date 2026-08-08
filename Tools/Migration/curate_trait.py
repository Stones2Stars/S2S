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
  * PromotionLine + LinePriority -> `enables.traits`: a rung ENABLES the next rung. That edge IS the ladder --
    a rung enters CAN GET only once the one beneath it is held (enabler.md par.1) -- so there is no ordering
    block, no rank, and no gate beside it. The `succession` block these used to emit never existed in the spec.
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
- CityStartCulture + BonusPopulationinNewCities -> DROPPED here. They are conditional GRANTS living on the
  FOUNDER -- `grants.culture` / `grants.population` on the settler, gated by the trait (json.md par.5). The
  `cityFounding` family they used to emit was an INVENTION and never existed in the spec.
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
from curate_common import TAG_BY_UNITCOMBAT   # the ONE unitcombat->tag map (a free-promotion condition keys on it)
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
    # The unit UPGRADE PRICE is a COST, not upkeep -- json.md §6's cost cluster puts "what CHANGES a cost" in the
    # ONE `costs` family, and `upgrade` is already a live kind there (COSTS_UPGRADE, authored by 17 entities).
    # `upkeep.upgradePrice` matched no kind row at all, so all 58 trait authorings parsed, reported
    # `[READJSON] unkinded-member upkeep.upgradePrice` and produced NOTHING. Same fix as ruling 18 below.
    "iUnitUpgradePriceModifier":       ("costs", "empire", "upgrade", "percent"),
    # work / improvement / conscript / hurry / trade
    "iWorkerSpeedModifier":            ("workRate", "empire", "", "percent"),
    "iImprovementUpgradeRateModifier": ("improvementUpgradeRate", "empire", "", "percent"),
    "iMaxConscript":                   ("conscript", "empire", "", "flat"),
    "iHurryAngerModifier":             ("hurry", "empire", "anger", "percent"),
    # ruling 18: hurry.cost -> costs.hurry. The ENGINE reads this leg as costs[COSTS_HURRY]
    # (CvCity::getHurryCostModifier -> CvPlayer::getCostKinds), and `hurry.cost` matches no kind row at all, so
    # the deposit was dropped at load as `unkinded-member hurry.cost`. curate_civic has always had it right.
    "iHurryCostModifier":              ("costs", "empire", "hurry",  "percent"),
    "iTradeRoutes":                    ("tradeRoutes", "empire", "", "flat"),
    "iMaxTradeRoutesChange":           ("tradeRoutes", "empire", "max", "flat"),
    # iCoastalTradeRoutes / iForeignTradeRouteModifier are NOT rows here: the route-KIND variants are CONDITIONS,
    # never members (rulings 11/17, json.md §2). They emitted `coastal` / `foreign` members no kind table carries,
    # so every one was dropped at load as `[READJSON] unkinded-member tradeRoutes.coastal|foreign` -- silently,
    # which is how the whole seafaring line came to grant no trade routes at all. Handled in the apply loop below,
    # matching curate_civic / curate_building, which have carried the correct shape all along.
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
    # its OWN family (memberless kind 0) -- the engine reads espionageDefense, never a combat member;
    # curate_building has always had it right. Under `combat` it matched no kind row and was dropped.
    "iEspionageDefense":               ("espionageDefense", "empire", "", "percent"),
    # the CAPTURE family owns these, not combat: the engine reads capture[CAPTURE_PROBABILITY] /
    # [CAPTURE_RESISTANCE] via CvPlayer::getCaptureKinds. Under `combat` they matched no kind and were dropped.
    "iNationalCaptureProbabilityModifier": ("capture", "empire", "probability", "percent"),
    "iNationalCaptureResistanceModifier": ("capture", "empire", "resistance", "percent"),
    # iMissileRange is NOT a row here: a missile's range works EXACTLY like an air unit's operation range (owner),
    # so it is the SAME air.range kind gated on IS_MISSILE -- a CONDITION, never a kind of its own
    # ([DEC-conditions-are-predicates]). Under `combat` it matched no kind row and every one was dropped at load
    # as `unkinded-member combat.missileRange`. Handled in the apply loop below.
    # AIR_RANGE -- the player leg CvUnit::airRange() folds in beside the unit's own resolved value.
    "iFlightOperationRange":           ("air", "empire", "range", "flat"),
    "iNavalCargoSpace":                ("cargo", "empire", "navalCargo", "flat"),
    "iMissileCargoSpace":              ("cargo", "empire", "missileCargo", "flat"),
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
    # TradeYieldModifiers is NOT a SPLIT_ARRAY row: that table makes the YIELD the FAMILY, which addressed a
    # per-channel ROUTE modifier as a member of food/commerce/production. Ruling 27 puts it under tradeRoutes
    # (tradeRoutes.<scope>.modifier.<channel>.<unit>) -- handled in the apply loop, as curate_civic already does.
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
    # ⚠ The option is a CONDITION, so this row is intercepted below and emitted as a conditioned deposit on the
    # `civic` kind rather than keyed by the option -- the member here names the KIND, not a target container.
    "CivicOptionNoUpkeepTypes":           ("upkeep",         "empire", "civic",          "percent", None),
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
# iCityStartCulture / iBonusPopulationinNewCities: conditional GRANTS on the founder (json.md par.5), never a
# trait-side family -- dropped here so the mapping cannot quietly re-emit the invented `cityFounding` block.
DROP = {"Type", "TraitPrereq", "TraitPrereqOr1", "TraitPrereqOr2", "PrereqTech", "Categories",
        "iCityStartCulture", "iBonusPopulationinNewCities"}
FAMILY_ORDER = ["food", "production", "buildRate", "researchRate", "commerce", "gold", "research", "culture", "espionage",
                "extraYieldThreshold", "lessYieldThreshold", "happiness", "health", "growth",
                "greatPeopleRate", "greatGeneralRate", "freeSpecialists", "experience", "conscript",
                "combat", "unitProduction", "maintenance", "upkeep", "tradeRoutes", "hurry", "workRate",
                "improvementUpgradeRate", "goldenAge", "unitCapability", "durations",
                "diplomacy", "stateReligion", "revolution"]


# Free-promotion entries whose unit filter is INEXPRESSIBLE -- a referenced unit-combat class carries no tag, so
# the entry is skipped rather than emitted unfiltered (which would arm every unit in the city). A drop ANNOUNCES
# (curators/README): data that vanishes silently is invisible on both axes at once.
FREE_PROMO_UNFILTERED = []


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


def _put_cond(fam, family, scope, unit, value, enabled, member=None):
    """Append a CONDITIONED deposit {value, enabled:<predicate>} to a scope-wide leaf, merging with any unconditioned
    scalar already there into a list (json §3.9) — the same shape as the SeaPlotYieldChanges IS_WATER fold below. The
    doc-covered shape for a state-gated modifier ([DEC-conditions-are-predicates]): a capital-only modifier is
    empire.percent + enabled:"IS_CAPITAL", NOT a bespoke empire.capital member."""
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
    the `promote` entry list a `triggers` end-turn-presence action takes.

    Free promotions EVOLVED from repeatable grants into TRIGGERS (owner; json.md §5: the `repeatable` wrapper and
    its `interval` dissolve into the trigger, and a free promotion is a `triggers` entry whose action promotes the
    units PRESENT at end-turn). The trait leg rides the SAME shape curate_building emits for FreePromoTypes -- one
    mechanism, not a trait variant.

    The unit-combat list is the trait declaring WHICH UNITS it arms, so it maps through TAG_BY_UNITCOMBAT onto the
    unit's TAG and rides the entry's own `enabled` predicate (engine.md: a promotion grant keys off the TAG, never
    a UNITCOMBAT id). ⛔ That is NOT the banned trait-side promotion x unitcombat MAP -- the map was the legacy
    mechanism; this is the ordinary conditioned-entry shape every other grantor uses.
    ⚠ Dropping the filter would arm EVERY unit in the city, so an entry whose classes carry no tag is SKIPPED and
    reported by the caller rather than emitted unfiltered.
    """
    out, unfiltered = [], []
    for entry in list(node):
        promo, ucs = None, []
        for c in entry:
            if c.tag == "PromotionType":
                promo = engine.text(c)
            elif c.tag == "UnitCombatTypes":
                ucs = [engine.text(u) for u in c if engine.text(u)]
        if not promo or not ucs:
            continue
        tags, missing = [], []
        for uc in sorted(ucs):
            hit = TAG_BY_UNITCOMBAT.get(uc)
            if not hit:
                missing.append(uc)
                continue
            for t in hit:
                if t not in tags:
                    tags.append(t)
        if missing:
            unfiltered.append((promo, missing))
            continue
        if len(tags) == 1:
            item = OrderedDict([("promotion", promo), ("enabled", "IS_" + tags[0].upper())])
        else:
            item = OrderedDict([("promotion", promo),
                                ("enabled", OrderedDict([("any", ["IS_" + t.upper() for t in sorted(tags)])]))])
        # DEDUPE: several unitcombat VARIANTS collapse onto one tag predicate (the curate_building precedent),
        # so identical entries would only make the apply do the same work twice.
        if item not in out:
            out.append(item)
    return out, unfiltered


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



def ladder_edges(lineOf, rankOf, emittedIds):
    """The developing-ladder `enables.traits` edges for ONE output folder: {typ: [nextTyp, ...]}.

    A rung ENABLES the rung above it, and that edge IS the ladder (json.md par.9 -- ordering needs no section of
    its own). Membership comes from the legacy PromotionLine + iLinePriority, NEVER from the id spelling: a line
    may RENAME mid-chain (TRAIT_NOMAD1 -> TRAIT_NOMADIC2) or SKIP a rank (TRAIT_GLORIOUS1 -> TRAIT_GLORIOUS3),
    and deriving the successor by string arithmetic on the id silently loses both -- it fabricates a
    TRAIT_NOMAD2 that no record defines.

    Scoped to ONE FOLDER's emitted ids so the two trait sets stay COMPLETELY SEPARATE: the chain simply ends
    where that set ends (simple/ tops out at rung 1), and a rung is never linked to one the active set has no
    entity for. Priority 0/absent is the BASE rung; the two arms (+1,+2,+3 and -1,-2,-3) each chain outward from
    it, so a line carrying both forks from the base."""
    members = {}
    for typ in emittedIds:
        line = lineOf.get(typ)
        if line:
            members.setdefault(line, []).append((rankOf.get(typ, 0), typ))
    edges = {}
    for line, entries in members.items():
        byRank = {}
        for rank, typ in entries:
            byRank.setdefault(rank, []).append(typ)
        for arm in (sorted(r for r in byRank if r >= 0),
                    sorted((r for r in byRank if r <= 0), reverse=True)):
            for i in range(len(arm) - 1):
                for lower in byRank[arm[i]]:
                    for upper in byRank[arm[i + 1]]:
                        if upper != lower and upper not in edges.setdefault(lower, []):
                            edges[lower].append(upper)
    return edges

def curate(typ, rec, store):
    text, fam, props, policies, grants, art_blocks, identity, ai = {}, {}, {}, {}, {}, {}, {}, {}
    excludes = []
    line_name, line_rank = None, None
    triggers = []
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
        elif tag == "iMissileRange":
            # A missile's range works EXACTLY like an air unit's operation range (owner) -- the missile simply dies
            # at the end of it -- so it is the SAME air.range kind gated on IS_MISSILE, never a kind of its own
            # ([DEC-conditions-are-predicates]). Under `combat` it matched no kind row and every authoring was
            # dropped at load as `unkinded-member combat.missileRange`.
            # ⚑ Only the DIFFERENCE against iFlightOperationRange is gated, and that is what makes the conversion
            # EXACT rather than approximate: where the two tags are EQUAL the author plainly meant "air and missile
            # both get this range" (owner), and the unconditioned entry already reaches missiles -- so a gated entry
            # there would double it for no authored reason. Where they differ the delta reproduces both values,
            # including the NEGATIVE case (SCIENTIFIC2 authors flight 1 / missile 0, i.e. a missile gets nothing).
            # ⚠ A missile therefore double-dips air.range + the delta by construction, which is the expected and
            # owner-accepted shape; changing it is a BALANCE decision, not a curation one.
            vMissile = _num(t) or 0
            eFlight = rec.find("iFlightOperationRange")
            vFlight = (_num(engine.text(eFlight)) or 0) if eFlight is not None else 0
            vDelta = vMissile - vFlight
            if vDelta not in (None, 0, 0.0):
                _put_cond(fam, "air", "empire", "flat", vDelta, "IS_MISSILE", "range")
        elif tag == "iCoastalTradeRoutes":
            # +N routes, but only in a COASTAL city -- a CITY verdict, so the predicate is HAS_COAST (the shape
            # curate_building.py already uses for this same tag).
            v = _num(t)
            if v not in (None, 0, 0.0):
                _put_cond(fam, "tradeRoutes", "empire", "flat", v, "HAS_COAST")
        elif tag == "iForeignTradeRouteModifier":
            # A ROUTE verdict, not a city one: IS_FOREIGN is evaluated against the route's PARTNER city inside the
            # profit stage (CvCity::totalTradeModifier). It rides the channel-AGNOSTIC `modifier`, which scales the
            # route profit before the per-channel split.
            v = _num(t)
            if v not in (None, 0, 0.0):
                _put_cond(fam, "tradeRoutes", "empire", "percent", v, "IS_FOREIGN", "modifier")
        elif tag == "TradeYieldModifiers":
            # Ruling 27: the per-CHANNEL route-yield %, carried by tradeRoutes under the channel axis. It scales
            # what a route DELIVERS (CvCity::calculateTradeYield), which is a property of the route -- not a
            # member of the food family, which is where the SPLIT_ARRAY row used to put it.
            for ident, v in engine.named_array(c, YIELDS).items():
                (fam.setdefault("tradeRoutes", {}).setdefault("empire", {}).setdefault("modifier", {})
                 .setdefault(ident, {}))["percent"] = v
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
                    # category is waived"), so the emitted magnitude is the -100% that states it.
                    # ⛔ And the OPTION is a CONDITION, never a keyed target: `upkeep.civicOptions` matched no kind
                    # row at all, so the authoring parsed, reported `[READJSON] unkinded-member` and produced
                    # NOTHING -- while `upkeep.civic` is the live kind 150 entities already author. The category
                    # rides the entry's own `enabled` predicate ([DEC-conditions-are-predicates]: a member that
                    # answers WHICH is the rollerskate). The predicate carries the FULL `CIVICOPTION_` id so it can
                    # never be confused with a `RELIGION_` type.
                    if tag == "CivicOptionNoUpkeepTypes":
                        _put_cond(fam, family, scope, unit, -100,
                                  OrderedDict([("CIVIC_CATEGORY", target)]), member=tt)
                        continue
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
                line_name = t
        elif tag == "iLinePriority":
            if engine.is_int(t) and int(t) != 0:
                line_rank = int(t)
        elif tag == "iGreatPeopleRateChange":
            gp_change = int(t) if engine.is_int(t) else None
        elif tag == "GreatPeopleUnitType":
            gp_unit = t if (t and t != "NONE") else None
        elif tag == "EraAdvanceFreeSpecialistType":
            # Fires when the ERA ADVANCES, not on the trait's own considered action, so it is a TRIGGERS entry
            # rather than a grant (json.md §5). The era advance is tech-driven (a tech whose era exceeds the
            # player's raises it), and the spine already carries that fact as SEVT_ERA_CHANGED -> `onEraChanged`.
            # `specialists` IS a grant payload here, the carve-out json.md §5 reserved: this one is a persisted
            # PULSE that outlives the trait (the apply lands it in the city's UNATTRIBUTED typed-free ledger),
            # never the alive-with-source freeSpecialists modifier family.
            if t and t != "NONE":
                triggers.append(OrderedDict([
                    ("trigger", "onEraChanged"),
                    # The LIST form its siblings use (units/techs): one element per granted specialist. An
                    # OBJECT here would parse as a SCOPED PULSE (CvGrants: "population": {"city": 3}) and grant
                    # nothing, silently.
                    ("action", OrderedDict([
                        ("grant", OrderedDict([("specialists", [t])])),
                    ])),
                ]))
        elif tag == "GoldenAgeonBirthofGreatPersonType":
            if t and t != "NONE":
                grants["goldenAgeOnBirthOfGreatPerson"] = t
        elif tag == "FreePromotionUnitCombatTypes":
            # A free promotion is a TRIGGER, not a grant (json.md §5) -- `grants.freePromotions` matched no
            # consumer, so all 130 authoring traits reported `unconsumed-section` and armed nobody. Same
            # end-turn-presence entry the BUILDING leg emits; one mechanism.
            fp, fp_unfiltered = _free_promotions(c)
            if fp:
                triggers.append(OrderedDict([
                    # The UNIT ENTERING is the happening, and the applier is targeted propagation off it (see
                    # curate_building's note) -- there is no per-turn sweep on this plane.
                    ("trigger", "onUnitEnteredCity"),
                    ("action", OrderedDict([("promote", OrderedDict([("promotions", fp), ("units", "present")]))])),
                ]))
            for promo, missing in fp_unfiltered:
                FREE_PROMO_UNFILTERED.append((typ, promo, missing))
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
    if triggers:
        out["triggers"] = triggers
    # NB the developing-ladder `enables.traits` edge is NOT emitted here: it is a property of the LINE and of the
    # emitting FOLDER, neither of which this per-record pass can see. It is applied per folder in main().
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
              "policies", "grants", "ai", "ui", "world", "sound", "identity"}
    fams = sorted({k for o in results.values() for k in o if k not in STRUCT and not k.startswith("PROPERTY_")})
    has = lambda k: sum(1 for o in results.values() if k in o)
    print("  families seen: %s" % ", ".join(fams))
    for k in ("enables", "replacedBy", "excludes", "grants", "policies", "ai"):
        print("  with %-11s: %d" % (k, has(k)))
    if all_leftover:
        print("  !! leftover-to-identity (review): %s" % ", ".join(sorted(all_leftover)))
    else:
        print("  (every XML tag classified — no leftovers)")
    if FREE_PROMO_UNFILTERED:
        # ANNOUNCE the skips: each is a free promotion whose unit filter cannot be expressed because a referenced
        # unit-combat class carries no tag. Emitting it unfiltered would arm EVERY unit in the city, so it is
        # dropped -- loudly. Closing one is a TAG_BY_UNITCOMBAT entry, not a curator change.
        missing_classes = sorted({uc for _, _, ms in FREE_PROMO_UNFILTERED for uc in ms})
        print("  ⚠ free promotions SKIPPED (untagged unit-combat class, filter inexpressible): %d entr(y/ies)"
              % len(FREE_PROMO_UNFILTERED))
        print("     untagged classes: %s" % ", ".join(missing_classes))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            print("\n=== %s ===" % nm)
            print(json.dumps(results.get(nm, {"(not found)": nm}), indent=1, ensure_ascii=False))
    if args.write:
        out_dir = os.path.join(REPO, "Assets", "Data", "traits")

        # The developing ladder, per FOLDER (see ladder_edges): membership from PromotionLine + iLinePriority,
        # scoped to the ids that folder actually emits, so a chain never reaches into the other set.
        lineOf, rankOf = {}, {}
        for typ, rec in table.items():
            line = engine.text(rec.find("PromotionLine"))
            rank = engine.text(rec.find("iLinePriority"))
            if line:
                lineOf[typ] = line
            rankOf[typ] = int(rank) if rank not in (None, "") else 0
        # ⚖ A COMPLEX-ONLY RUNG TAKES THE `TRAIT_COMPLEX_` PREFIX WHEN ITS **LINE** HAS A SIMPLE COUNTERPART
        # (owner). The replacement variants already carry it, but a rung the simple set never had (the simple
        # ladder tops out early) fell through keeping its authored `TRAIT_` id -- which left a chain reading
        # TRAIT_COMPLEX_SEAFARING -> TRAIT_COMPLEX_SEAFARING1 -> TRAIT_SEAFARING2. The LINE is the complex
        # variant, so every rung of it is, whether or not that particular rung has a simple twin.
        # ⚠ This is a Type RENAME and therefore a Type removal ([save.md] par.7): trait reads are allow-missing,
        # so a player holding the old id loses that rung on load. Accepted (owner) -- the cost only grows.
        simpleLines = set(lineOf[t] for t in results if folders.get(t) == "simple" and t in lineOf)
        rename = {}
        for typ in results:
            if typ in rid_to_base or folders.get(typ) != "complex":
                continue          # a replacement variant already carries the prefix; simple/ keeps its id
            if typ.startswith("TRAIT_COMPLEX_") or not typ.startswith("TRAIT_"):
                continue
            if lineOf.get(typ) in simpleLines:
                rename[typ] = "TRAIT_COMPLEX_" + typ[len("TRAIT_"):]
        if rename:
            # re-key the maps IN PLACE OF ORDER (the manifest order is positional -- curate_order.py)
            results = OrderedDict((rename.get(k, k), v) for k, v in results.items())
            for old, new in rename.items():
                results[new]["type"] = new
                folders[new] = folders.pop(old)
                if old in lineOf:
                    lineOf[new] = lineOf.pop(old)
                if old in rankOf:
                    rankOf[new] = rankOf.pop(old)
            # every CROSS-REFERENCE moves with the id (enables.traits ladders, excludes, replacedBy): a dangling
            # old id resolves to nothing and severs the edge silently
            def _rekey(node):
                if isinstance(node, dict):
                    return OrderedDict((k, _rekey(v)) for k, v in node.items())
                if isinstance(node, list):
                    return [_rekey(v) for v in node]
                return rename.get(node, node) if isinstance(node, str) else node
            for k in list(results):
                results[k] = _rekey(results[k])
            print("  re-keyed %d complex-only rung(s) onto TRAIT_COMPLEX_: %s"
                  % (len(rename), ", ".join(sorted(rename))))

        emitted = {"simple": set(), "complex": set()}
        for typ in results:
            if typ in rid_to_base:
                emitted["complex"].add(typ)          # the complex variant, under its OWN TRAIT_COMPLEX_ id
            else:
                emitted[folders[typ]].add(typ)
                if folders[typ] == "simple" and typ not in repl_map:
                    emitted["complex"].add(typ)      # base-fill: no complex variant exists for this one
        ladders = dict((f, ladder_edges(lineOf, rankOf, ids)) for f, ids in emitted.items())

        def _write(folder, typ, obj):
            d = os.path.join(out_dir, folder)
            os.makedirs(d, exist_ok=True)
            edges = ladders[folder].get(typ)
            if edges:
                # copy first -- a base-filled object is written to BOTH folders, whose chains differ
                obj = OrderedDict(obj)
                enables = OrderedDict(obj.get("enables") or OrderedDict())
                enables["traits"] = edges
                obj["enables"] = enables
            with open(os.path.join(d, typ.lower() + ".json"), "w") as f:
                json.dump(obj, f, indent=1, ensure_ascii=False)

        # Drop-before-rewrite through the SHARED clear, so a type no longer emitted (a re-keyed or merged-away
        # record) cannot linger as a stale file. It must be this one and not a hand-rolled loop: clearing a folder
        # is what REGISTERS it for the `_additions` overlay re-apply at process exit (curators/README.md), so a
        # curator that clears by hand silently drops its own overlay. It also carries the refuse-on-zero guard --
        # a curator that produced nothing has no input to rewrite from, and clearing would destroy the set.
        cc.wipe_entity_json(out_dir, recurse=True, expected=len(results))
        if not results:
            return
        nwritten = 0
        for typ, obj in results.items():
            if typ in rid_to_base:
                # THE COMPLEX VARIANT KEEPS ITS OWN `TRAIT_COMPLEX_` IDENTITY (naming.md: TRAIT_ is a simple
                # trait, TRAIT_COMPLEX_ a complex one). Re-keying it onto the base id is what manufactured the
                # colliding-id problem -- two different entities answering to one name, which then forced every
                # reader to disambiguate by game option. The two sets are separated by FOLDER and by ID, so
                # nothing has to be resolved at read time.
                _write("complex", typ, obj)
                nwritten += 1
                continue
            _write(folders[typ], typ, obj)                    # base/plain -> simple/ (or complex-only -> complex/)
            nwritten += 1
            if typ in repl_map:
                pass   # its complex variant is emitted above under its own TRAIT_COMPLEX_ id
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
