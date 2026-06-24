#!/usr/bin/env python3
"""Curate Building (#428, Tier E #32) + SpecialBuilding (#31, rides this pass) — THE deepest modifier surface.

Curated from `classifications/building-classification.json` (the adversarial classify-building workflow, 288 fields)
+ the owner rulings (handover-2026-06-16-6). Building is the most-targeted entity; the SOURCE->building enabler edges
(tech/bonus/civic/religion/corp/cultureLevel `enables`, ObsoleteTech, ReplacementBuildings) are ALREADY store-wired,
so on the building side they DROP — the building authors only its OWN: ~70 modifier families, a `requires` MEANS gate,
`grants` (incl. the NEW `grants.repeatable`), cost, properties, identity. EXE-link: 0 DllExport -> UNCONSTRAINED.

OWNER RULINGS folded in (handover #6):
- §6.1 DELIVERYGUY: the 22 "inversions" KEEP-ON-BUILDING keyed by target (NOT inverted). Tech/Bonus/Building gated via
  `enabled`; Improvement/Terrain/Plot yields target-keyed (food.city.improvements.{IMP}.flat). Tech ones PROVISIONAL (Phase F).
- `grants.repeatable[]` + `interval` (modifier-spec §4.1): PropertySpawn (chance via `per`) + iNumUnitFullHeal/HealUnitCombat.
- shrine (GlobalReligionCommerce) -> `per`-scaled commerce MODIFIER (per city holding the religion), not a block.
- CommerceChangeDoubleTimes -> 2nd age-gated deposit `enabled:{existedFor:{min:N}}`.
- cityCapture = its OWN family (capturing CITIES, distinct from the §5 unit `capture` gradient).
- capability bools -> identity (revisit Phase F); EXCEPTIONS to enables-family: bForceTeamVoteEligible->enables.votes,
  Hurrys->enables.hurries, FoundsCorporation->enables.corporations.
- DROP (dead §8-i): iMaxPopulationAllowed, iMaxPopulationChange, iDCMNukesOkay/bDCMNukesOkay.
- iNukeExplosionRand: NOT emitted (live code, but only the EXCLUDED Bad_Karma/Building_Meltdown module populates it).

⚠ THIS IS PASS 1 (the bulk): scalar/percent families + yield/commerce + requires + store edges + cost + caps + art/ai +
era foldering + the COVERAGE CHECK. The keyed inversions, CvProperties, repeatable grants, building-on-building, and the
one-shot grants/pulses land in PASS 2 (they show as UNHANDLED in the coverage report meanwhile).

  python3 curate_building.py --sample BUILDING_FORGE BUILDING_PALACE BUILDING_WONDER_PYRAMIDS
  python3 curate_building.py --write
"""
import argparse
import json
import os
import xml.etree.ElementTree as ET
from collections import OrderedDict

import engine
import boolexpr
from curate_common import put_art, emit_art, FAMILY_ORDER, de_i, descale100
from store import Store, REPO

# ---- scalar/percent modifier families: tag -> (family, scope, member|None, unit). Corrected scopes from the
# classification (the mapping's were often wrong). Names PROVISIONAL (reader-pass refines). ----
SCALAR_FAMILIES = {
    # health / happiness / healing
    "iHealth": ("health", "city", None, "flat"),
    "iAreaHealth": ("health", "area", None, "flat"),
    "iGlobalHealth": ("health", "empire", None, "flat"),
    "iHealthPercentPerPopulation": ("health", "city", "perPopulation", "percent"),
    "iHappiness": ("happiness", "city", None, "flat"),
    "iAreaHappiness": ("happiness", "area", None, "flat"),
    "iGlobalHappiness": ("happiness", "empire", None, "flat"),
    "iHappinessPercentPerPopulation": ("happiness", "city", "perPopulation", "percent"),
    "iHealRateChange": ("healing", "city", None, "flat"),
    "iFoodKept": ("foodKept", "city", None, "percent"),
    # great people / great general
    "iGreatPeopleRateChange": ("greatPeopleRate", "city", None, "flat"),
    "iGreatPeopleRateModifier": ("greatPeopleRate", "city", None, "percent"),
    "iGlobalGreatPeopleRateModifier": ("greatPeopleRate", "empire", None, "percent"),
    "iGreatGeneralRateModifier": ("greatGeneralRate", "city", None, "percent"),
    "iDomesticGreatGeneralRateModifier": ("greatGeneralRate", "city", "domestic", "percent"),
    # maintenance (grouped, cost-style)
    "iMaintenanceModifier": ("maintenance", "city", None, "percent"),
    "iGlobalMaintenanceModifier": ("maintenance", "empire", None, "percent"),
    "iAreaMaintenanceModifier": ("maintenance", "area", None, "percent"),
    "iOtherAreaMaintenanceModifier": ("maintenance", "area", "otherArea", "percent"),
    "iDistanceMaintenanceModifier": ("maintenance", "empire", "distance", "percent"),
    "iNumCitiesMaintenanceModifier": ("maintenance", "empire", "numCities", "percent"),
    "iCoastalDistanceMaintenanceModifier": ("maintenance", "empire", "coastalDistance", "percent"),
    "iConnectedCityMaintenanceModifier": ("maintenance", "empire", "connectedCity", "percent"),
    "iInflationModifier": ("inflation", "empire", None, "percent"),
    # war weariness
    "iWarWearinessModifier": ("warWeariness", "city", None, "percent"),
    "iGlobalWarWearinessModifier": ("warWeariness", "empire", None, "percent"),
    "iEnemyWarWearinessModifier": ("warWeariness", "city", "enemy", "percent"),
    # hurry
    "iHurryCostModifier": ("hurryCost", "city", None, "percent"),
    "iGlobalHurryModifier": ("hurryCost", "empire", None, "percent"),
    "iHurryAngerModifier": ("hurryAnger", "city", None, "percent"),
    # production specials
    "iMilitaryProductionModifier": ("buildRate", "city", "military", "percent"),
    "iSpaceProductionModifier": ("buildRate", "city", "space", "percent"),
    "iGlobalSpaceProductionModifier": ("buildRate", "empire", "space", "percent"),
    "iWorkerSpeedModifier": ("workRate", "empire", None, "percent"),
    # trade routes
    "iTradeRoutes": ("tradeRoutes", "city", None, "flat"),
    "iCoastalTradeRoutes": ("tradeRoutes", "empire", "coastal", "flat"),
    "iGlobalTradeRoutes": ("tradeRoutes", "empire", None, "flat"),
    "iWorldTradeRoutes": ("tradeRoutes", "world", None, "flat"),
    "iTradeRouteModifier": ("tradeRoutes", "city", "modifier", "percent"),
    "iForeignTradeRouteModifier": ("tradeRoutes", "city", "foreignModifier", "percent"),
    # experience / free specialists
    "iExperience": ("experience", "city", None, "flat"),
    "iGlobalExperience": ("experience", "empire", None, "flat"),
    "iFreeSpecialist": ("freeSpecialists", "city", None, "any"),
    "iAreaFreeSpecialist": ("freeSpecialists", "area", None, "any"),
    "iGlobalFreeSpecialist": ("freeSpecialists", "empire", None, "any"),
    # misc city
    "iAnarchyModifier": ("anarchy", "city", None, "percent"),
    "iGoldenAgeModifier": ("goldenAge", "empire", None, "percent"),
    "iOccupationTimeModifier": ("occupationTime", "city", None, "percent"),
    "iPopulationgrowthratepercentage": ("populationGrowthRate", "city", None, "percent"),
    "iGlobalPopulationgrowthratepercentage": ("populationGrowthRate", "empire", None, "percent"),
    # revolution
    "iRevIdxLocal": ("revolution", "city", None, "flat"),
    "iRevIdxNational": ("revolution", "empire", None, "flat"),
    "iRevIdxDistanceModifier": ("revolution", "city", "distanceModifier", "percent"),
    # cops-and-robbers (owner ruling 2026-06-20): criminal stealth (insidiousness) vs city catch (investigation)
    # -> the makeWanted/arrest mechanic (verified CvUnit::doInsidiousnessVSInvestigationCheck). Dedicated block,
    # NOT espionage (one stat, one name -- espionage never "becomes" insidiousness). iPillageGoldModifier dropped (dead).
    "iInsidiousness": ("copsAndRobbers", "city", "insidiousness", "flat"),
    "iInvestigation": ("copsAndRobbers", "city", "investigation", "flat"),
    "iEspionageDefense": ("espionageDefense", "city", None, "flat"),
    # cityCapture (NEW family — capturing CITIES, distinct from §5 unit capture)
    "iNationalCaptureProbabilityModifier": ("cityCapture", "empire", "probability", "percent"),
    "iNationalCaptureResistanceModifier": ("cityCapture", "empire", "resistance", "percent"),
    "iLocalCaptureProbabilityModifier": ("cityCapture", "city", "probability", "percent"),
    "iLocalCaptureResistanceModifier": ("cityCapture", "city", "resistance", "percent"),
    "iUnitUpgradePriceModifier": ("unitUpgradePrice", "empire", None, "percent"),
    # defense family (grouped; `min` floor clamp lives here, modifier-spec §7)
    "iDefense": ("defense", "city", "amount", "percent"),
    "iBombardDefense": ("defense", "city", "bombardDefense", "percent"),
    "iAllCityDefense": ("defense", "empire", "amount", "percent"),
    "iNukeModifier": ("defense", "city", "nukeDefense", "percent"),
    "iAirModifier": ("defense", "city", "airDefense", "percent"),
    "iMinDefense": ("defense", "city", "min", "flat"),
    "iNoEntryDefenseLevel": ("defense", "city", "noEntryLevel", "flat"),
    "iLocalDynamicDefense": ("defense", "city", "dynamicDefense", "flat"),
    "iRiverDefensePenalty": ("defense", "city", "riverDefensePenalty", "flat"),
    "iBuildingDefenseRecoverySpeedModifier": ("defense", "city", "buildingDefenseRecovery", "percent"),
    "iCityDefenseRecoverySpeedModifier": ("defense", "city", "cityDefenseRecovery", "percent"),
    "iDamageAttackerChance": ("defense", "city", "damageAttackerChance", "flat"),
    "iDamageToAttacker": ("defense", "city", "damageToAttacker", "flat"),
    "iAdjacentDamagePercent": ("defense", "city", "adjacentDamage", "percent"),
}
# scope-wide yield/commerce families: tag -> (scope, keys, kind). SPLIT into per-identifier families (food/gold/…).
YIELD_FAMILIES = {
    "YieldChanges": ("city", engine.YIELDS, "flat"),
    "YieldModifiers": ("city", engine.YIELDS, "percent"),
    "YieldPerPopChanges": ("city", engine.YIELDS, "perPopulation"),
    "AreaYieldModifiers": ("area", engine.YIELDS, "percent"),
    "GlobalYieldModifiers": ("empire", engine.YIELDS, "percent"),
    # GlobalSeaPlotYieldChanges + RiverPlotYieldChanges are PLOTS-TARGET folds (owner 2026-06-22): a scope-wide
    # source depositing onto every matching plot in scope. Handled in pass2 via _inject_plots (the "PLOTS-TARGET
    # folds" block), NOT the scope-wide flat path -- so `plotTypes`/`empire.flat`+post_process/`.river` are retired.
    "CommerceChanges": ("city", engine.COMMERCES, "flat"),
    "CommerceModifiers": ("city", engine.COMMERCES, "percent"),
    "CommercePerPopChanges": ("city", engine.COMMERCES, "perPopulation"),
    "GlobalCommerceModifiers": ("empire", engine.COMMERCES, "percent"),
    # +commerce per specialist (ALL types) -> <c>.empire.specialist.perSpecialist, UNIFORM with civic/trait
    # (legacy getSpecialistExtraCommerce, scaled x total specialist count -- a flat would mis-count by ~count-1).
    "SpecialistExtraCommerces": ("empire", engine.COMMERCES, "perSpecialist", "specialist"),
}

# capability bools -> identity (owner: revisit Phase F). Plain b-flag -> clean name: true (false omitted).
CAP_IDENTITY = {
    "bNukeImmune": "nukeImmune", "bNeverCapture": "neverCapture", "bZoneOfControl": "zoneOfControl",
    "bProtectedCulture": "protectedCulture", "bBorderObstacle": "borderObstacle", "bNoUnhappiness": "noUnhappiness",
    "bNoUnhealthyPopulation": "noUnhealthyPopulation", "bBuildingOnlyHealthy": "buildingOnlyHealthy",
    "bForceAllTradeRoutes": "forceAllTradeRoutes",
    "bQuarantine": "quarantine", "bMapCentering": "mapCentering", "bCenterInCity": "centerInCity",
    "bTeamShare": "teamShare", "bOrbital": "orbital", "bOrbitalInfrastructure": "orbitalInfrastructure",
    "bGovernmentCenter": "governmentCenter", "bCapital": "capital", "bAllowsNukes": "allowsNukes",
    "bProvidesFreshWater": "providesFreshWater", "bNoHolyCity": "noHolyCity", "bAutoBuild": "autoBuild",
    "bNoLimit": "noInstanceLimit", "bForceNoPrereqScaling": "forceNoPrereqScaling",
    "bApplyFreePromotionOnMove": "applyFreePromotionOnMove",
    "bDamageAllAttackers": "damageAllAttackers",  # derives m_bDamageAttackerCapable (recompute-on-load)
}
# identity scalars: tag -> key (non-zero int OR non-empty string).
ID_SCALAR = {
    "iAsset": "worth", "iPower": "militaryWorth", "iConquestProb": "conquestProbability",
    "iAirlift": "airlift", "iAirUnitCapacity": "airUnitCapacity", "iLineOfSight": "sightRange",
    "iWorkableRadius": "workableRadius", "iNumPopulationEmployed": "populationEmployed",
    "iExtraPlayerInstances": "maxPlayerInstancesExtra", "iDCMAirbombMission": "dcmAirbombMission",
    "ExtendsBuilding": "extends", "ProductionContinueBuilding": "productionContinue",
    "GreatPeopleUnitType": "greatPeopleUnitType", "DiploVoteType": "diploVoteType",
    "SpecialBuildingType": "specialBuildingType",  # FK to the per-player-capped SpecialBuilding GROUP (#31)
    "ReligionType": "religion", "Advisor": "advisor", "fVisibilityPriority": "visibilityPriority",
    "FreeStartEra": "freeStartEra", "MaxStartEra": "maxStartEra", "PromotionLineType": "promotionLineType",
    "iLinePriority": "linePriority",
}
ID_LIST = {"MapCategoryTypes": "mapCategories", "UnitCombatRetrainTypes": "unitCombatRetrainTypes",
           "Categories": "categories", "VictoryThresholds": "victoryThresholds"}
# cost (intrinsic base + the C2C real-cost scaling members). tag -> key.
COST = {"iCost": "production", "iCostSizeModifier": "sizeModifier", "iCostCountModifier": "countModifier",
        "iCostMaterialsModifier": "materialsModifier", "iCostComplexityModifier": "complexityModifier"}

# ---- DROP / DEFER tables ----
# DEAD (§8-i, confirmed zero consumers) + meltdown (excluded-module-only data, not emitted).
DROP_DEAD = {"iMaxPopulationAllowed", "iMaxPopulationChange", "iDCMNukesOkay", "bDCMNukesOkay", "iNukeExplosionRand",
    # dead, re-verified 2026-06-20 (zero consumers): building field unwired -- live pillage-gold is the promotion PillageChange
    "iPillageGoldModifier",
    # dead, re-verified 2026-06-20: the repel combat mechanic was REMOVED from the engine (CvCombatModel.cpp:294); field is a vestige (only debug TestCode.py reads getLocalRepel)
    "iLocalRepel",
    # dead, re-verified 2026-06-20: old wonder item never wired (only the loader/getter decl, zero consumers)
    "bNoEnemyPillagingIncome"}
# Module-LOADER directives (not gameplay): bForceOverwrite is a modular-merge control, never a building property.
DROP_MODULE = {"bForceOverwrite"}
# Handled by requires_fn (read off rec). Most are in the mapping prereqs; listed here for the coverage check.
REQUIRES_TAGS = {
    "PrereqTech", "TechTypes", "Bonus", "PrereqBonuses", "VicinityBonus", "RawVicinityBonus",
    "PrereqVicinityBonuses", "PrereqRawVicinityBonuses", "PrereqReligion", "PrereqCorporation", "PrereqCultureLevel",
    "PrereqCivic", "PrereqAndCivics", "PrereqOrCivics", "bRequiresActiveCivics", "StateReligion",
    "bNeedStateReligionInCity", "bWater", "bRiver", "bFreshWater", "bPower", "bPrereqPower", "PowerBonus",
    "iMinAreaSize", "iMinLatitude", "iMaxLatitude", "iPrereqPopulation", "bPrereqWar", "ConstructCondition",
    "PrereqInCityBuildings", "PrereqNotInCityBuildings", "PrereqOrBuildings", "PrereqAmountBuildings",
    "PrereqAnyoneBuilding", "PrereqOrTerrain", "PrereqAndTerrain", "PrereqOrFeature", "PrereqOrImprovement",
    "PrereqOrHeritage", "VictoryPrereq", "iCitiesPrereq", "iTeamsPrereq", "iLevelPrereq",
    "iMaxGlobalInstances", "iMaxTeamInstances", "iMaxPlayerInstances", "EnabledCivilizationTypes",
    "PrereqGameOption", "NotGameOption",
}
# store-handled enabler edges (DROP building-side: they invert onto the SOURCE) + obsolete/replace.
STORE_TAGS = {"ObsoleteTech", "ObsoletesToBuilding", "ReplacementBuildings", "FreeBuilding", "FreeAreaBuilding"}
# PASS-2 tags (keyed inversions / properties / repeatable grants / building-on-building / one-shot grants/pulses).
# Listed so the coverage check distinguishes "deferred to pass 2" from "genuinely unhandled".
PASS2_TAGS = {
    # keyed inversions (keep-on-building):
    "TechYieldChanges", "TechYieldModifiers", "TechCommerceChanges", "TechCommerceModifiers", "TechHappinessChanges",
    "TechHealthChanges", "TechSpecialistChanges", "BonusHealthChanges", "BonusHappinessChanges", "BonusYieldChanges",
    "BonusYieldModifiers", "BonusCommercePercentChanges", "VicinityBonusYieldChanges", "BonusProductionModifiers",
    "ImprovementYieldChanges", "GlobalImprovementYieldChanges", "TerrainYieldChanges", "ReligionChanges",
    "PlotYieldChanges", "GlobalSeaPlotYieldChanges", "RiverPlotYieldChanges", "BonusDefenseChanges", "PowerYieldModifiers",
    # building-on-building:
    "BuildingHappinessChanges", "BuildingProductionModifiers", "GlobalBuildingProductionModifiers",
    "GlobalBuildingCostModifiers", "GlobalBuildingExtraCommerces",
    # specialist-keyed:
    "SpecialistYieldChanges", "SpecialistCommerceChanges", "LocalSpecialistCommerceChanges",
    # properties:
    "Properties", "PropertiesAllCities", "PropertyManipulators",
    # repeatable grants:
    "PropertySpawnUnit", "PropertySpawnProperty", "iNumUnitFullHeal", "HealUnitCombatTypes",
    # one-shot grants / pulses:
    "ExtraFreeBonuses", "FreeTraitTypes", "FreeSpecialTech", "iFreeTechs", "NewCityFree", "HolyCity",
    "iGlobalPopulationChange", "iPopulationChange", "bGoldenAge", "FreeSpecialistCounts", "SpecialistCounts",
    # enables-family from XML:
    "FoundsCorporation", "Hurrys", "bForceTeamVoteEligible",
    # conditional / temporal:
    "iStateReligionHappiness", "CommerceChangeDoubleTimes",
    # aid (creative, pass 2):
    "BonusAidModifiers", "AidRateChanges", "DomainFreeExperiences", "UnitCombatFreeExperiences",
    "UnitCombatExtraStrengths", "UnitProductionModifiers", "UnitCombatProdModifiers", "DomainProductionModifiers",
    "UnitCombatDefenseAgainstModifiers", "MayDamageAttackingUnitCombatTypes", "FreePromoTypes",
    "CommerceHappinesses", "CommerceFlexibles", "GlobalReligionCommerce",
    "StateReligionCommerces", "ImprovementFreeSpecialists", "GlobalCorporationCommerce", "PropertySource",
    "ObsoletesToBuilding",  # building->building obsolescence (own obsoletes edge; wire in pass 2)
}
# text + always-drop.
TEXT = {"Description": "description", "Civilopedia": "civilopedia", "Help": "help"}
ART = ("ArtDefineTag", "Button", "MovieDefineTag", "ConstructSound")


# ---- PASS 2 tables (keep-on-building, §6.1) ----
# CONDITION-gated deposits (the keyed entity is a CONDITIONER you possess): tag -> (family|None, scope, valuekeys|None, unit, ref_kind).
# family None + valuekeys => SPLIT (the yield/commerce member IS the family). ref_kind builds the `enabled` atom.
COND_KEYED = {
    "TechYieldChanges":            (None, "city", engine.YIELDS, "flat", "tech"),
    "TechYieldModifiers":          (None, "city", engine.YIELDS, "percent", "tech"),
    "TechCommerceChanges":         (None, "city", engine.COMMERCES, "flat", "tech"),     # FLAT change (changeBuildingCommerceTechChange -> getBaseCommerceRate100, CvCity.cpp:12136) -- NOT a percent (XML sub-tag "CommercePercents" is a misnomer; verify-then-fix, cascade-fixed-point.md §2)
    "TechCommerceModifiers":       (None, "city", engine.COMMERCES, "percent", "tech"),
    "TechHappinessChanges":        ("happiness", "city", None, "flat", "tech"),
    "TechHealthChanges":           ("health", "city", None, "flat", "tech"),
    "BonusHealthChanges":          ("health", "city", None, "flat", "bonus"),
    "BonusHappinessChanges":       ("happiness", "city", None, "flat", "bonus"),
    "BonusYieldChanges":           (None, "city", engine.YIELDS, "flat", "bonus"),
    "BonusYieldModifiers":         (None, "city", engine.YIELDS, "percent", "bonus"),
    "BonusCommercePercentChanges": (None, "city", engine.COMMERCES, "flat", "bonus"),   # FLAT x100 (getBonusCommercePercentChanges -> getBaseCommerceRate100, CvCity.cpp:12135) -- NOT a percent (the "Percent" XML name is a misnomer, like TechCommerceChanges); de-scaled via PER100_TAGS
    "BonusProductionModifiers":    ("buildRate", "self", None, "percent", "bonus"),
    "VicinityBonusYieldChanges":   (None, "city", engine.YIELDS, "flat", "vicinityBonus"),
    "BuildingHappinessChanges":    ("happiness", "city", None, "flat", "building"),
    "GlobalBuildingCostModifiers": ("costs", "empire", None, "percent", "building"),
}
# TARGET-keyed deposits (the effect lands ON the keyed entity): tag -> (family|None, scope, targetType|None, valuekeys|None, unit).
# targetType None => key DIRECTLY under scope (religion influence). family None + valuekeys => split member is family.
TARGET_KEYED = {
    "ImprovementYieldChanges":       (None, "city", "improvements", engine.YIELDS, "flat"),
    "GlobalImprovementYieldChanges": (None, "empire", "improvements", engine.YIELDS, "flat"),
    "TerrainYieldChanges":           (None, "city", "terrains", engine.YIELDS, "flat"),
    # PlotYieldChanges -> a PLOTS-TARGET fold (owner 2026-06-22): per-plot-TYPE map folds into the `plots` target
    # filtered by a plot predicate (IS_WATER/IS_LAND/HAS_HILLS/HAS_PEAK). Handled in pass2 (_inject_plots), NOT plotTypes.
    "GlobalBuildingExtraCommerces":  (None, "empire", "buildings", engine.COMMERCES, "flat"),
    # Specialist{Yield,Commerce}Changes + Local* -> NOT here: a building boosting a specialist is the SPECIALIST's
    # OWN output conditioned by the building's presence (own-output home, modifier.md §6.5, owner 2026-06-20), so it
    # lives ON THE SPECIALIST -- emitted by curate_specialist's SPECIALIST_BOOSTS, dropped at the building.
    "BonusDefenseChanges":           ("defense", "city", "bonuses", None, "flat"),
    "ReligionChanges":               ("religion", "city", None, None, "flat"),
    "UnitCombatFreeExperiences":     ("experience", "city", "unitCombats", None, "flat"),
    "DomainFreeExperiences":         ("experience", "city", "domains", None, "flat"),
    "UnitCombatExtraStrengths":      ("strength", "city", "unitCombats", None, "flat"),
    "UnitProductionModifiers":       ("buildRate", "city", "units", None, "percent"),
    "UnitCombatProdModifiers":       ("buildRate", "city", "unitCombats", None, "percent"),
    "DomainProductionModifiers":     ("buildRate", "city", "domains", None, "percent"),
    "BuildingProductionModifiers":   ("buildRate", "city", "buildings", None, "percent"),
    "GlobalBuildingProductionModifiers": ("buildRate", "empire", "buildings", None, "percent"),
    "UnitCombatDefenseAgainstModifiers": ("defense", "city", "unitCombats", None, "flat"),
    # ImprovementFreeSpecialists -> NOT keyed here: it's a conditioner (improvement presence), handled as a
    # conditioned `freeSpecialists.city.any` deposit in the body (modifier.md §6.7).
}
_KEY_TAGS = ("PrereqTech", "TechType", "BuildingType", "BonusType", "ImprovementType", "TerrainType",
             "SpecialistType", "ReligionType", "UnitType", "UnitCombatType", "DomainType", "PlotType")

# PER-100 legacy fields -- the curator's ONE-TIME de-scale to HUMAN-READABLE numbers (docs/dev/reference/
# cascade-fixed-point.md §0.1/§2). These XML fields are stored x100 (their C++ accessor is `get...100()`; the value
# flows straight into the x100 `m_buildingExtraYield100` bucket, CvCity.cpp:4951 -- verified from the math, not the
# field name alone). The JSON layer must be human ("+7", not 700); the human->x100 conversion is readJson's sole job.
# After this single XML->JSON conversion no per-100/normal mix survives anywhere downstream (owner 2026-06-19).
PER100_TAGS = frozenset(("TechYieldChanges", "TechCommerceChanges", "BonusCommercePercentChanges"))


def _descale100(v):
    """x100 legacy value -> human-readable: int when divisible by 100 (700 -> 7), else a 2-decimal float (150 -> 1.5).
    Recurses into the split/member dict ({food: 700, production: 100} -> {food: 7, production: 1})."""
    if isinstance(v, dict):
        return OrderedDict((k, _descale100(x)) for k, x in v.items())
    if isinstance(v, int):
        return v // 100 if v % 100 == 0 else round(v / 100.0, 2)
    return v


def _keyed(node, valuekeys):
    """[(ref, value), ...] for a C2C keyed/paired container. ref = the first known key-tag child; value = a
    named_array(rest, valuekeys) dict if valuekeys else the scalar int. Handles the inconsistent key tags."""
    out = []
    for entry in list(node):
        ref, valnode = None, None
        for c in entry:
            if ref is None and (c.tag in _KEY_TAGS or c.tag.endswith("Type")):
                ref = engine.text(c)
            else:
                valnode = c
        if not ref or ref == "NONE":
            continue
        if valuekeys is not None:
            vn = valnode if (valnode is not None and len(valnode)) else entry
            val = engine.named_array(vn, valuekeys)
        elif valnode is not None and len(valnode):
            val = engine.named_array(valnode, None) if False else None
        else:
            tx = engine.text(valnode) if valnode is not None else None
            val = int(tx) if engine.is_int(tx) else None
        if val not in (None, {}, [], 0):
            out.append((ref, val))
    return out


def _enabled(ref_kind, ref, scope):
    if ref_kind == "tech":
        return _atom(ref, "team")
    if ref_kind == "bonus":
        return _atom(ref, "city", min=1)
    if ref_kind == "vicinityBonus":
        return _atom(ref, "city", connection="vicinity", min=1)
    if ref_kind == "building":
        return _atom(ref, "empire" if scope == "empire" else "city")
    if ref_kind == "power":
        return "HAS_POWER"
    return _atom(ref, scope)


def _inject_cond(fams, family, scope, unit, value, enabled):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def _inject_keyed(fams, family, scope, target_type, key, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    if target_type:
        node = node.setdefault(target_type, OrderedDict())
    node = node.setdefault(key, OrderedDict())
    if unit in node and isinstance(node[unit], int) and isinstance(value, int):
        node[unit] += value
    else:
        node[unit] = value


# PlotType key -> the plots-target predicate (the plotTypes fold, owner 2026-06-22).
PLOT_PRED = {"PLOT_OCEAN": "IS_WATER", "PLOT_LAND": "IS_LAND", "PLOT_HILLS": "HAS_HILLS", "PLOT_PEAK": "HAS_PEAK"}


def _inject_plots(fams, family, scope, unit, value, enabled):
    """A `plots`-TARGET deposit (owner 2026-06-22): <family>.<scope>.plots.<unit> = [{value, enabled:<predicate>}, ...]
    (data-model.md §4.1/§6). A scope-wide source deposits onto EVERY plot in scope matching the predicate -- ONE
    uniform mechanism retiring getYieldChangeAt / getSeaPlotYield / getRiverPlotYield + the per-plot-type accumulators.
    Always a LIST of entries (matches _inject_cond), so multiple predicates accumulate under one (family, scope)."""
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict()).setdefault("plots", OrderedDict())
    entry = OrderedDict([("value", value), ("enabled", enabled)])
    cur = node.get(unit)
    if cur is None:
        node[unit] = [entry]
    elif isinstance(cur, list):
        cur.append(entry)
    else:
        node[unit] = [cur, entry]


def pass2(typ, rec, store, fams, grants, repeatable, identity, enables):
    """The custom-shape layer: keyed inversions (§6.1), properties, repeatable grants, one-shot grants/pulses,
    enables-from-XML, the conditional/temporal deposits. Mutates the passed-in collections."""
    # --- CONDITION-gated keyed deposits (Tech/Bonus/Building/Power conditioners) ---
    for tag, (family, scope, vkeys, unit, kind) in COND_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for ref, val in _keyed(node, vkeys):
            if tag in PER100_TAGS:                       # one-time de-scale x100 -> human (cascade-fixed-point.md §2)
                val = _descale100(val)
            enabled = _enabled(kind, ref, scope)
            if isinstance(val, dict):                    # split: member IS the family
                for member, v in val.items():
                    _inject_cond(fams, member, scope, unit, v, enabled)
            else:
                _inject_cond(fams, family, scope, unit, val, enabled)
    # --- TARGET-keyed deposits (effect lands on the improvement/terrain/specialist/unit/...) ---
    for tag, (family, scope, ttype, vkeys, unit) in TARGET_KEYED.items():
        node = rec.find(tag)
        if node is None:
            continue
        for ref, val in _keyed(node, vkeys):
            if isinstance(val, dict):
                for member, v in val.items():
                    _inject_keyed(fams, member, scope, ttype, ref, unit, v)
            else:
                _inject_keyed(fams, family, scope, ttype, ref, unit, val)
    # --- PLOTS-TARGET folds (owner 2026-06-22): plotTypes/seaPlot/.river -> the explicit `plots` target filtered by a
    # plot predicate. A scope-wide source deposits onto EVERY matching plot in scope (data-model §4.1/§6); ONE uniform
    # mechanism retiring getYieldChangeAt / getSeaPlotYield / getRiverPlotYield + the per-plot-type accumulators. ---
    pyc = rec.find("PlotYieldChanges")           # per-plot-TYPE (OCEAN/LAND/HILLS/PEAK) -> <yield>.city.plots.flat {pred}
    if pyc is not None:
        for ref, val in _keyed(pyc, engine.YIELDS):
            pred = PLOT_PRED.get(ref)
            if pred is None or not isinstance(val, dict):
                continue                          # unknown PlotType -> skip (never invent a predicate)
            for member, v in val.items():
                _inject_plots(fams, member, "city", "flat", v, pred)
    gsp = rec.find("GlobalSeaPlotYieldChanges")  # empire-wide flat to water tiles -> <yield>.empire.plots.flat {IS_WATER}
    if gsp is not None:
        for member, v in engine.named_array(gsp, engine.YIELDS).items():
            if v:
                _inject_plots(fams, member, "empire", "flat", v, "IS_WATER")
    rpy = rec.find("RiverPlotYieldChanges")      # yield to the city's worked river plots -> <yield>.city.plots.flat {HAS_RIVER}
    if rpy is not None:
        for member, v in engine.named_array(rpy, engine.YIELDS).items():
            if v:
                _inject_plots(fams, member, "city", "flat", v, "HAS_RIVER")
    # --- PowerYieldModifiers: a DIRECT per-yield array (<iYield>..</iYield>), NOT entity-keyed -> emit each yield as a
    # city-scope `percent` deposit gated `enabled: HAS_POWER` (legacy getPowerYieldRateModifier, summed only when
    # isPower(), CvCity.cpp:11228). Was mis-classified in COND_KEYED (which expects a key-tag) -> silently dropped. ---
    pym = rec.find("PowerYieldModifiers")
    if pym is not None:
        for member, v in engine.named_array(pym, engine.YIELDS).items():
            if v:
                _inject_cond(fams, member, "city", "percent", v, "HAS_POWER")
    # --- StateReligionCommerces: per-commerce, gated on the building's religion being the state religion ---
    src = rec.find("StateReligionCommerces")
    if src is not None:
        relig = _txt(rec, "ReligionType")
        pred = OrderedDict([("STATE_RELIGION", relig)]) if relig else "HAS_STATE_RELIGION"
        for member, v in engine.named_array(src, engine.COMMERCES).items():
            _inject_cond(fams, member, "city", "flat", v, pred)
    # --- iStateReligionHappiness: happiness while the city follows the state religion ---
    srh = _int(rec, "iStateReligionHappiness")
    if srh:
        _inject_cond(fams, "happiness", "city", "flat", srh, "HAS_STATE_RELIGION")
    # --- CommerceChangeDoubleTimes: a SECOND age-gated commerce deposit (existedFor), pairing the base CommerceChanges ---
    cdt = rec.find("CommerceChangeDoubleTimes")
    if cdt is not None:
        base = engine.named_array(rec.find("CommerceChanges"), engine.COMMERCES) if rec.find("CommerceChanges") is not None else {}
        for member, turns in engine.named_array(cdt, engine.COMMERCES).items():
            if member in base:
                _inject_cond(fams, member, "city", "flat", base[member],
                             OrderedDict([("existedFor", OrderedDict([("min", turns)]))]))
    # --- CommerceHappinesses: happiness GAINED per unit of each commerce produced -> a grouped commerceHappiness family ---
    ch = rec.find("CommerceHappinesses")
    if ch is not None:
        for member, v in engine.named_array(ch, engine.COMMERCES).items():
            fams.setdefault("commerceHappiness", OrderedDict()).setdefault("city", OrderedDict()).setdefault(member, OrderedDict())["flat"] = v
    # --- shrine (GlobalReligionCommerce = a single RELIGION FK, getDataMembers enumAsInt): the building is the
    # SHRINE for that religion. The per-commerce VALUES live on the Religion (parked religion.shrine, #15); the
    # full modifier = religion.shrine.{commerce} x countReligionLevels(religion) at world scope is assembled at #430.
    # The building just declares the shrine relationship -> `shrine: RELIGION` (owner: shrine is a `per`-scaled
    # commerce modifier; the building provides the religion ref, the religion provides the values). ---
    shrine = _txt(rec, "GlobalReligionCommerce")
    if shrine:
        identity["shrine"] = shrine

    # --- CvProperties: Properties (city) / PropertiesAllCities (empire) -> per-PROPERTY family deposits ---
    for tag, scope in (("Properties", "city"), ("PropertiesAllCities", "empire")):
        node = rec.find(tag)
        if node is not None:
            for c in node:
                p = engine.text(c.find("PropertyType")) if c.find("PropertyType") is not None else None
                amt = _intval(c)
                if p and p != "NONE" and amt:
                    fams.setdefault(p, OrderedDict()).setdefault(scope, OrderedDict())["flat"] = amt
    pm = rec.find("PropertyManipulators")
    if pm is not None:
        for s in pm.findall("PropertySource"):
            res = engine.property_source_v3(s)
            if res:
                prop, pscope, unit, value = res
                node = fams.setdefault(prop, OrderedDict()).setdefault(pscope, OrderedDict())
                if unit in node and isinstance(node[unit], int) and isinstance(value, int):
                    node[unit] += value
                else:
                    node[unit] = value
    # --- repeatable grants (modifier-spec §4.1): PropertySpawn + the per-turn heal generalization ---
    sp_prop = _txt(rec, "PropertySpawnProperty")
    sp_unit = _txt(rec, "PropertySpawnUnit")
    if sp_prop and sp_unit:
        repeatable.append(OrderedDict([("unit", sp_unit), ("interval", "perTurn"),
                                       ("chance", OrderedDict([("per", _atom(sp_prop, "city"))]))]))
    fh = _int(rec, "iNumUnitFullHeal")
    if fh:
        repeatable.append(OrderedDict([("heal", "full"), ("count", fh), ("interval", "perTurn")]))
    hnode = rec.find("HealUnitCombatTypes")
    if hnode is not None:
        for item in list(hnode):
            uc = _txt(item, "UnitCombatType")
            heal, adj = _int(item, "iHeal"), _int(item, "iAdjacentHeal")
            if uc and (heal or adj):
                g = OrderedDict([("unitCombat", uc), ("interval", "perTurn")])
                if heal:
                    g["heal"] = heal
                if adj:
                    g["adjacentHeal"] = adj
                repeatable.append(g)
    # --- one-shot grants / pulses --- (ExtraFreeBonuses is NOT a one-shot grant: it's a continuous while-active
    # bonus supply -> provides.bonuses, handled in curate(). FreeTraitTypes stays a grant.)
    for tag, key in (("FreeTraitTypes", "traits"),):
        lst = _typelist(rec, tag)
        if lst:
            grants[key] = lst
    st = _txt(rec, "FreeSpecialTech")
    if st:
        grants.setdefault("techs", []).append(st)
    hc = _txt(rec, "HolyCity")
    if hc:
        grants["holyCity"] = hc
    ft = _int(rec, "iFreeTechs")
    if ft:
        grants["freeTechs"] = ft
    for tag in ("iPopulationChange", "iGlobalPopulationChange"):
        v = _int(rec, tag)
        if v:
            sc = "city" if tag == "iPopulationChange" else "empire"
            grants.setdefault("population", OrderedDict())[sc] = v
    if _bool(rec, "bGoldenAge"):
        grants["goldenAge"] = True
    # FreeSpecialistCounts -> freeSpecialists COUNT family, keyed by specialist type (modifier.md §6.7 (A)).
    fsc = rec.find("FreeSpecialistCounts")
    if fsc is not None:
        node = fams.setdefault("freeSpecialists", OrderedDict()).setdefault("city", OrderedDict())
        for k, v in _pairs_generic(fsc):
            if v:
                node[k] = v
    promos = _typelist(rec, "FreePromoTypes")
    if promos:
        grants["promotions"] = promos
    # SpecialistCounts (capacity/slots) -> allowedSpecialists COUNT family, keyed by specialist type (the cap on
    # manual assignment, modifier.md §6.7 (A)).
    scn = rec.find("SpecialistCounts")
    if scn is not None:
        node = fams.setdefault("allowedSpecialists", OrderedDict()).setdefault("city", OrderedDict())
        for k, v in _pairs_generic(scn):
            if v:
                node[k] = v
    # TechSpecialistChanges = the SAME capacity, tech-conditioned (inner XML is <SpecialistCounts>, verified 2026-06-20)
    # -> allowedSpecialists.city.SPECIALIST_X enabled by the team-tech. NOT freeSpecialists (the old label was wrong).
    tsc = rec.find("TechSpecialistChanges")
    if tsc is not None:
        for entry in list(tsc):
            tech = _txt(entry, "PrereqTech")
            counts = entry.find("SpecialistCounts")
            if tech and counts is not None:
                pred = OrderedDict([("type", tech), ("scope", "team")])
                for k, v in _pairs_generic(counts):
                    if v:
                        _inject_cond(fams, "allowedSpecialists", "city", k, v, pred)
    # ImprovementFreeSpecialists -> free ANY specialist, CONDITIONED on the improvement's presence (modifier.md §6.7).
    # FLAG: per-improvement-instance vs presence semantics unverified -> modeled as `enabled` presence for now.
    ifs = rec.find("ImprovementFreeSpecialists")
    if ifs is not None:
        for entry in list(ifs):
            imp = _txt(entry, "ImprovementType")
            n = _int(entry, "iFreeSpecialistCount")
            if imp and n:
                _inject_cond(fams, "freeSpecialists", "city", "any", n, OrderedDict([("type", imp), ("scope", "city")]))
    # --- enables-family authored on the building (FoundsCorporation / Hurrys / vote eligibility) ---
    fc = _txt(rec, "FoundsCorporation")
    if fc:
        enables.setdefault("corporations", []).append(fc)
    hurries = _typelist(rec, "Hurrys")
    if hurries:
        enables.setdefault("hurries", []).extend(hurries)
    if _bool(rec, "bForceTeamVoteEligible"):
        enables.setdefault("votes", []).append("FORCE_TEAM_ELIGIBLE")
    # ObsoletesToBuilding is authored TARGET-side as `obsoletedBy` in the main emit (not here) — owner 2026-06-22.
    # --- NewCityFree: RELOCATED off the building onto the FOUNDER units as grants.foundBuildings (owner 2026-06-16:
    # the settler "carries buildings into settling"; gated by each building's NewCityFree BoolExpr -> a tech-gated
    # building unavailable at settle time is not pre-built). curate_unit.found_buildings() reads NewCityFree off the
    # store's BuildingInfo table + the boolexpr converter; nothing is emitted building-side now. (renames §Unit.) ---
    # --- CommerceFlexibles -> identity.commerceFlexible (capability: which commerce SLIDERS this building unlocks;
    # CvPlayer::changeCommerceFlexibleCount on build -> isCommerceFlexible gates slider-setting — owner 2026-06-16). ---
    cfn = rec.find("CommerceFlexibles")
    if cfn is not None:
        flex = [engine.COMMERCES[i] for i, c in enumerate(list(cfn))
                if i < len(engine.COMMERCES) and engine.text(c) in ("1", "true", "True")]
        if flex:
            identity["commerceFlexible"] = flex
    # --- GlobalCorporationCommerce: park the corp FK (the building is corp X's HQ; the per-commerce
    # HeadquarterCommerce VALUES live on the corporation #16, x world countCorporationLevels assembled at #430 — the
    # corp-HQ ANALOG of the shrine, owner 2026-06-16). ---
    corphq = _txt(rec, "GlobalCorporationCommerce")
    if corphq:
        identity["corporationHQ"] = corphq
    # --- MayDamageAttackingUnitCombatTypes: the selective counter-damage list (pairs with the defense family +
    # bDamageAllAttackers/iDamageToAttacker) -> identity capability list. ---
    md = _typelist(rec, "MayDamageAttackingUnitCombatTypes")
    if md:
        identity["damageAttackingUnitCombats"] = md
    # AidRateChanges / BonusAidModifiers: DROPPED (owner 2026-06-16) — an UNWIRED property "aid" mechanic with NO
    # gameplay effect (city arrays m_paiAidRate/m_ppaaiExtraBonusAidModifier allocated+saved but never written-from-
    # building or read-for-effect); only AI building-valuation (CvCityAI /3) + pedia read the raw values. Not emitted.


def _intval(node):
    for ch in node:
        if engine.is_int(engine.text(ch)) and ch.tag != "PropertyType":
            return int(engine.text(ch))
    return None


def _pairs_generic(node):
    if node is None:
        return
    for item in list(node):
        k, v = None, None
        for c in item:
            if k is None and (c.tag.endswith("Type") or c.tag in _KEY_TAGS):
                k = engine.text(c)
            elif engine.is_int(engine.text(c)):
                v = int(engine.text(c))
        if k and k != "NONE" and v not in (None, 0):
            yield k, v


def _txt(rec, tag):
    t = engine.text(rec.find(tag))
    return t if (t and t != "NONE") else None


def _int(rec, tag):
    t = engine.text(rec.find(tag))
    return int(t) if engine.is_int(t) else None


def _bool(rec, tag):
    return engine.text(rec.find(tag)) in ("1", "true", "True")


def _typelist(rec, wrapper):
    node = rec.find(wrapper)
    if node is None:
        return []
    return [t for t in (engine.text(c).strip() for c in node) if t and t != "NONE"]


def _extra_free_bonuses(rec):
    # ExtraFreeBonuses is NESTED: <ExtraFreeBonuses><ExtraFreeBonus><FreeBonus>BONUS_X</FreeBonus>
    # <iNumFreeBonuses>N</iNumFreeBonuses></ExtraFreeBonus>...</ExtraFreeBonuses>. The engine's getFreeBonuses()
    # makes the building supply these bonuses to its city while ACTIVE (bonusAvailableFromBuildings -> hasVicinityBonus),
    # so they become a vicinity-bonus SOURCE -> provides.bonuses (uniform with a map bonus that provides itself).
    node = rec.find("ExtraFreeBonuses")
    if node is None:
        return []
    out = []
    for efb in node.findall("ExtraFreeBonus"):
        b = _txt(efb, "FreeBonus")
        if b:
            out.append(b)
    return out


def _default_scope(typ):
    # Mirrors the parser's rjDefaultScope (CvCascadeReadJson.cpp): TECH->team, civic/heritage->empire,
    # everything else (building/bonus/religion/corporation/population/...)->city.
    if typ.startswith("TECH_"):
        return "team"
    if typ.startswith(("CIVIC_", "HERITAGE_")):
        return "empire"
    return "city"


def _atom(typ, scope, **kw):
    # Collapse a plain presence to a BARE STRING -- the parser implies scope from the ID's domain, so emitting a
    # redundant {type, scope} only invites authoring bugs (owner 2026-06-23: forcing type+scope guarantees bugs).
    # Object form ONLY for a special case: any kw (a `connection`, a `role`, a `min`/`max` count), a non-default
    # scope, or a plot-substrate predicate type (TERRAIN_/FEATURE_/IMPROVEMENT_/MAPCATEGORY_) -- the parser routes
    # those to a plot predicate by the object's `type` key, so they must stay objects, never a bare string.
    is_plot_pred = isinstance(typ, str) and typ.startswith(("TERRAIN_", "FEATURE_", "IMPROVEMENT_", "MAPCATEGORY_"))
    if not kw and not is_plot_pred and scope == _default_scope(typ):
        return typ
    a = OrderedDict([("type", typ), ("scope", scope)])
    a.update(kw)
    return a


def requires_building(rec, store):
    """The TARGET-side reversible MEANS gate (enabler-spec §3/§5): build (greying) vs operate (dormancy).
    Most SOURCE->building enabler edges are store-wired onto the source; here we author the building's OWN means."""
    build_all, build_any, build_none, op_all, op_any, op_dormant = [], [], [], [], [], []
    # --- resources: a bonus prereq is REVERSIBLE, but WHERE it lands depends on whether the building NEEDS the
    # resource to FUNCTION or merely to APPEAR (owner ruling 2026-06-22; live-verified):
    #   * MANUALLY-built buildings that need the resource to operate (the gatherer line -- RICE_GATHERER &c.) go
    #     DORMANT when the resource is lost -> requires.OPERATE. (Corrects the 2026-06-21 build-side classification,
    #     which leaned on an INCOMPLETE "isDisabledBuilding == replacement/religion/corp only" grounding; the live
    #     enabler verify proved the engine DOES disable these on bonus loss -- the ~1,193 missed-dormant gatherers.)
    # ⚑ PESTS/CRIME disease pseudobuildings (bAutoBuild) are a KNOWN GAP: the engine dormants them by their
    #   DISEASE/CRIME PROPERTY band (owner 2026-06-22), NOT the bonus -- but they are not in CIV4PropertyInfos, so
    #   the curator doesn't author that band. Routing their bonus to BUILD (no operate) over-permits (the engine
    #   dormants most PESTS -> +misses); routing to OPERATE over-dormants the persisters (e.g. TERMITES -> false-
    #   dormants) but matches MORE cities (28 misses / 61% vs 101 / 51%). Until the PESTS property band is migrated,
    #   operate is the closer approximation, so bonus -> operate uniformly (the gatherer line is the real win here).
    bonus_all, bonus_any = op_all, op_any
    b = _txt(rec, "Bonus")
    if b:
        bonus_all.append(_atom(b, "city", connection="trade|vicinity"))
    orb = _typelist(rec, "PrereqBonuses")
    if orb:
        bonus_any.append([_atom(x, "city", connection="trade|vicinity") for x in orb])
    # RawVicinity FOLDS into normal vicinity (owner 2026-06-16: lose the adjacency strictness, simpler vocab).
    for tag in ("VicinityBonus", "RawVicinityBonus"):
        v = _txt(rec, tag)
        if v:
            bonus_all.append(_atom(v, "city", connection="vicinity"))
    for tag in ("PrereqVicinityBonuses", "PrereqRawVicinityBonuses"):
        lst = _typelist(rec, tag)
        if lst:
            bonus_any.append([_atom(x, "city", connection="vicinity") for x in lst])
    # --- plot-state predicates (bare). bWater = the city is COASTAL, not the plot being water: legacy
    # isValidBuildingLocation (CvCity.cpp:18500-18506) gates bWater on isCoastal() (a city sits on LAND, so
    # IS_WATER/pl->isWater() is always false -> every coastal building wrongly hidden). With bRiver also set it is
    # coastal OR river (line 18502). bRiver-alone / bFreshWater stay plot predicates (lines 18507/18516). ---
    bw, br = _bool(rec, "bWater"), _bool(rec, "bRiver")
    if bw and br:
        build_any.append(["HAS_COAST", "HAS_RIVER"])   # city is coastal OR river (HAS_COAST @ city = isCoastal)
    elif bw:
        build_all.append("HAS_COAST")
    elif br:
        build_all.append("HAS_RIVER")
    if _bool(rec, "bFreshWater"):
        build_all.append("HAS_FRESHWATER")
    # --- power (presence) ---
    if _bool(rec, "bPower") or _bool(rec, "bPrereqPower"):
        build_all.append("HAS_POWER")
    pb = _txt(rec, "PowerBonus")
    if pb:
        bonus_all.append(_atom(pb, "city", connection="trade|vicinity", role="power"))   # operate/build by bAutoBuild
    # --- city / world counts + size (tally) ---
    for tag, scope in (("iPrereqPopulation", "city"), ("iCitiesPrereq", "empire"), ("iTeamsPrereq", "world"),
                       ("iLevelPrereq", "empire")):
        v = _int(rec, tag)
        if v and v > 0:
            kind = {"iPrereqPopulation": "POPULATION", "iCitiesPrereq": "CITY", "iTeamsPrereq": "TEAM",
                    "iLevelPrereq": "UNIT_LEVEL"}[tag]
            build_all.append(_atom(kind, scope, min=v))
    # iMinAreaSize: a LAND building -> AREA_SIZE atom = the landmass tile count (area()->getNumTiles()). A WATER
    # building (bWater) means the SEA-BODY size (legacy isCoastal(N)), already covered by the IS_COASTAL it gets
    # from bWater -> skip the atom (all 32 current iMinAreaSize buildings are water; the land atom is the capability).
    mas = _int(rec, "iMinAreaSize")
    if mas and mas > 0 and not bw:
        build_all.append(_atom("AREA_SIZE", "city", min=mas))
    lo, hi = _int(rec, "iMinLatitude"), _int(rec, "iMaxLatitude")
    if (lo and lo > 0) or (hi is not None and hi != 90):
        lat = OrderedDict([("latitude", OrderedDict())])
        if lo and lo > 0:
            lat["latitude"]["min"] = lo
        if hi is not None and hi != 90:
            lat["latitude"]["max"] = hi
        build_all.append(lat)
    # --- in-city buildings (AND / OR / NOT / count) ---
    for x in _typelist_struct(rec, "PrereqInCityBuildings", "BuildingType"):
        build_all.append(_atom(x, "city"))
    notin = _typelist_struct(rec, "PrereqNotInCityBuildings", "BuildingType")
    for x in notin:
        build_none.append(_atom(x, "city"))
    orbld = _typelist_struct(rec, "PrereqOrBuildings", "BuildingType")
    if orbld:
        build_any.append([_atom(x, "city") for x in orbld])
    for k, v in _amount_buildings(rec):
        build_all.append(_atom(k, "empire", min=v))
    anyone = _txt(rec, "PrereqAnyoneBuilding")
    if anyone:
        build_all.append(_atom(anyone, "world", min=1))
    # --- plot terrain/feature/improvement ---
    for tag, container in (("PrereqOrTerrain", None), ("PrereqOrFeature", None), ("PrereqOrImprovement", None)):
        lst = _typelist(rec, tag)
        if lst:
            build_any.append([_atom(x, "plot") for x in lst])
    for x in _typelist(rec, "PrereqAndTerrain"):
        build_all.append(_atom(x, "plot"))
    # MapCategoryTypes: the placement gate (legacy isMapCategory, CENTER-plot) -- the city plot must be in ONE of the
    # building's map categories (uncategorized plot = valid, handled engine-side in PRED_HAS_MAP_CATEGORY). Also kept
    # in identity.mapCategories (classification). Needed so space scenarios don't break (owner 2026-06-18); returns as
    # a viewport/zone gate when "all maps in one map" lands (multimap-zone-rework).
    mapcats = _typelist(rec, "MapCategoryTypes")
    if mapcats:
        build_any.append([_atom(x, "plot") for x in mapcats])
    orher = _typelist_struct(rec, "PrereqOrHeritage", "HeritageType") or _typelist(rec, "PrereqOrHeritage")
    if orher:
        build_any.append([_atom(x, "empire") for x in orher])
    # --- victory prereq (world) ---
    vp = _txt(rec, "VictoryPrereq")
    if vp:
        build_all.append(_atom(vp, "world"))
    # --- tech prereqs -> build.all (AND only: single PrereqTech + every TechTypes entry; buildings have NO OR-tech
    # -- only techs themselves model alternate tech-tree paths). The SOURCE->building enable edge is store-wired
    # (generation/frontier); this is the per-candidate CONFIRM the condition engine evaluates via isHasTech.
    # Without it the cascade can't gate future-tech wonders (the modern-wonder over-offer). enabler-spec §13.8;
    # legacy gate CvPlayer::canConstruct 6584 (PrereqAndTech) + 6589 (PrereqAndTechs). Mirrors curate_unit. ---
    t = _txt(rec, "PrereqTech")
    if t:
        build_all.append(_atom(t, "team"))
    for x in _typelist_struct(rec, "TechTypes", "PrereqTech"):
        build_all.append(_atom(x, "team"))
    # --- SpecialBuilding GROUP-gate inheritance (data-model §7, the building-group wrangle): a member building
    # inherits its GROUP's shared gates. TechPrereq -> the same build.all TECH confirm (the group carries the tech
    # the member lacks -- monasteries/temples/cathedrals/punk lines; legacy CvPlayer::canConstruct 6599). NB
    # TechPrereqAnyone is UNUSED across all 36 groups -> skipped; the group cap + waiver are handled in curate()/the
    # cascade; ObsoleteTech inheritance is store-wired (store.py). ---
    sb = _txt(rec, "SpecialBuildingType")
    if sb and store is not None:
        sbrec = store.get("SpecialBuildingInfo", sb)
        if sbrec is not None:
            sbt = sbrec.findtext("TechPrereq")
            if sbt:
                build_all.append(_atom(sbt, "team"))
    # --- instance caps are NOT a requires atom (owner 2026-06-17) — they move to the declarative `allowed` cap.
    # A requires SELF-atom forced an off-by-one (cap 1 -> max:0) and conflated "needed" (requires) with "allowed"
    # (the ceiling). `allowed:{scope:N}` names the ceiling with the REAL number; SELF leaves requires entirely.
    # Authored in curate() via allowed_building(). enabler-spec §5/§13.7. ---
    # --- civics / state-religion -> OPERATE (dormancy) ---
    for x in _typelist_struct(rec, "PrereqAndCivics", "PrereqCivic") or _typelist(rec, "PrereqAndCivics"):
        op_all.append(_atom(x, "empire"))
    orciv = _typelist_struct(rec, "PrereqOrCivics", "PrereqCivic") or _typelist(rec, "PrereqOrCivics")
    if orciv:
        # OR-group -> operate.any (a TOP-LEVEL any), NOT a nested {any:} inside operate.all: the cascade parser
        # (CvCascadeReadJson.cpp:218-224) treats each operate.all element as a LEAF, so a nested {any:} was SKIPPED
        # -> the civic gate was dropped -> over-offer. Mirror build_any. (A2 "civics" cluster fix, 2026-06-18.)
        op_any.append([_atom(x, "empire") for x in orciv])
    pc = _txt(rec, "PrereqCivic")
    if pc:
        op_all.append(_atom(pc, "empire"))
    # bNeedStateReligionInCity (STATE_RELIGION_IN_CITY) and StateReligion (getPrereqStateReligion,
    # m_iStateReligion, "this religion must be your STATE religion") are BUILD gates (greying), NOT operate
    # (dormancy): both are checked ONLY in canConstruct (needStateReligionInCity CvCity.cpp:2604;
    # getPrereqStateReligion CvPlayer.cpp:6676) and appear in NO disable/isActiveBuilding path -> once built, the
    # building does NOT go dormant if the player later switches state religion away. So -> build. (verified 2026-06-21)
    if _bool(rec, "bNeedStateReligionInCity"):
        build_all.append("STATE_RELIGION_IN_CITY")
    sr = _txt(rec, "StateReligion")
    if sr:
        build_all.append(OrderedDict([("STATE_RELIGION", sr)]))
    # PrereqReligion / PrereqCorporation are the building's reversible MEANS (a religion can leave via inquisition,
    # a corp can be lost) and the engine DOES disable a built building when they go (CvCity.cpp applyReligionModifiers
    # ~14999 / applyCorporationModifiers ~15198 set isDisabledBuilding) -> author on operate (dormancy, forward check).
    for tag, scope in (("PrereqReligion", "city"), ("PrereqCorporation", "city")):
        v = _txt(rec, tag)
        if v:
            op_all.append(_atom(v, scope))
    # ReplacementBuildings is reversible DORMANCY in the engine -- CvCity.cpp:14413 setDisabledBuilding(predecessor,
    # true) while the successor is present (NOT removal: the predecessor STAYS, hasBuilding() true), re-enabled when
    # it's gone (unless another replacer holds). Mirror it EXACTLY with requires.operate.dormant (parity -- a
    # behavioural redesign of whether e.g. blackened-skies should dorm an observatory is POST-migration). The
    # `replaces` enabler edge stays a defined concept but is now UNUSED -- the whole mechanic is operate.dormant.
    # (owner 2026-06-23; engine-verified.)
    for x in _typelist_struct(rec, "ReplacementBuildings", "BuildingType"):
        op_dormant.append(_atom(x, "city"))
    # PrereqCultureLevel is a BUILD gate (greying), NOT operate (dormancy): getPrereqCultureLevel is checked ONLY in
    # canConstruct (CvCity.cpp:2788, sets probabilityEverConstructable) and appears in NO disable/isActiveBuilding
    # path -> a built building does NOT go dormant if its city's culture level later drops. (verified 2026-06-21)
    v = _txt(rec, "PrereqCultureLevel")
    if v:
        build_all.append(_atom(v, "city"))
    # --- ConstructCondition BoolExpr -> build (greying). It is checked ONLY at canConstruct (CvCity.cpp:2976-2999),
    # never isActiveBuilding, so losing a ConstructCondition bonus after build does nothing -> build (greying), NOT
    # operate (dormancy). Folded via the shared boolexpr converter (And/Or of Has over bonus/feature/tech/terrain/
    # building). owner 2026-06-16; renames §Building. ---
    boolexpr.merge_into(boolexpr.convert_field(rec.find("ConstructCondition")), build_all, build_any, build_none)
    # EnabledCivilizationTypes: a civ-WHITELIST build gate (owner 2026-06-22) -- only the listed civ(s) may build it
    # (CvCity::canConstruct, getCivilizationType()==getEnabledCivilizationType(i).eCivilization, CvCity.cpp:2560; the
    # civ-unique building mechanism, 242 regular buildings + module). -> requires.build.any OR-group of
    # {type:CIVILIZATION_X, scope:empire} (empty list = unrestricted). Was UN-MIGRATED: it sat in REQUIRES_TAGS for the
    # coverage check only and requires_building() never read it -> emitted nothing.
    civs = _typelist_struct(rec, "EnabledCivilizationTypes", "CivilizationType")
    if civs:
        build_any.append([_atom(c, "empire") for c in civs])

    build = OrderedDict()
    if build_all:
        build["all"] = build_all
    if build_any:
        build["any"] = build_any
    if build_none:
        build["noneOf"] = build_none
    # PALACE-TYPE (government-center) buildings can't be player-BUILT where a government center already exists
    # (CvCity.cpp:2654 isGovernmentCenter gate; Palace + the bGovernmentCenter pseudo-palaces). The negation twin
    # `disabled: IS_CAPITAL` (owner 2026-06-16); IS_CAPITAL = "the city has a palace/palace-adjacent building".
    # NB this is the PLAYER build gate only — the engine's FORCED relocation (capital falls) is an ungated actor
    # that bypasses requires (the #437 placement-gate invariant: gate the checked path, engine outcomes bypass).
    if _bool(rec, "bCapital") or _bool(rec, "bGovernmentCenter"):
        build["disabled"] = "IS_CAPITAL"
    out = OrderedDict()
    if build:
        out["build"] = build
    if op_all or op_any or op_dormant:
        operate = OrderedDict()
        if op_all:
            operate["all"] = op_all
        if op_any:
            operate["any"] = op_any
        if op_dormant:
            operate["dormant"] = op_dormant   # dormant while ANY listed is present (the reversible-disable mirror)
        out["operate"] = operate
    # loadPrune (game options) lives in its own section, but author it here for now under requires for visibility.
    return out or None


def allowed_building(rec):
    """The declarative INSTANCE CAP (owner 2026-06-17): `allowed:{<scope>:N}` = "at most N of THIS may exist at
    scope" — the REAL cap number (NOT a requires SELF-atom, which forced an off-by-one and conflated needed/allowed).
    Scope-keyed (world/team/empire); for a building the cap scope ALSO derives its wonder category
    (world->worldWonder, team->teamWonder, empire->nationalWonder; CvGameCoreUtils.cpp:340-369 isWorldWonder =
    getMaxGlobalInstances()!=-1). Absent => uncapped. The new canDoStuff gate enforces it (build while
    tally.count(SELF,scope) < N) and owns ignoring it (NO_WONDER_LIMIT/NO_NATIONAL_UNIT_LIMIT/CHALLENGE_ONE_CITY),
    era-scaling, and +extra — all engine, never the parser (enabler-spec §5/§13.7)."""
    allowed = OrderedDict()
    for tag, scope in (("iMaxGlobalInstances", "world"), ("iMaxTeamInstances", "team"), ("iMaxPlayerInstances", "empire")):
        v = _int(rec, tag)
        if v is not None and v >= 0:
            allowed[scope] = v
    return allowed or None


def _typelist_struct(rec, wrapper, keytag):
    """Struct-list wrapper: each child carries a <keytag> ref (+ maybe a count). Returns [TYPE,...]."""
    node = rec.find(wrapper)
    if node is None:
        return []
    out = []
    for c in node:
        k = engine.text(c.find(keytag)) if c.find(keytag) is not None else engine.text(c)
        if k and k != "NONE":
            out.append(k)
    return out


def _amount_buildings(rec):
    """PrereqAmountBuildings: per-building count threshold -> [(BUILDING, N), ...]."""
    node = rec.find("PrereqAmountBuildings")
    if node is None:
        return []
    out = []
    for c in node:
        b = engine.text(c.find("BuildingType")) if c.find("BuildingType") is not None else None
        n = None
        for ch in c:
            if engine.is_int(engine.text(ch)) and ch.tag != "BuildingType":
                n = int(engine.text(ch))
        if b and b != "NONE" and n:
            out.append((b, n))
    return out


def loadprune_building(rec):
    out = OrderedDict()
    on = _typelist(rec, "PrereqGameOption") or ([_txt(rec, "PrereqGameOption")] if _txt(rec, "PrereqGameOption") else [])
    noton = _typelist(rec, "NotGameOption") or ([_txt(rec, "NotGameOption")] if _txt(rec, "NotGameOption") else [])
    on = [x for x in on if x]
    noton = [x for x in noton if x]
    if on:
        out["onGameOptions"] = on
    if noton:
        out["notOnGameOptions"] = noton
    return out or None


def _set_fam(fams, family, scope, member, unit, value):
    node = fams.setdefault(family, OrderedDict()).setdefault(scope, OrderedDict())
    if member:
        node = node.setdefault(member, OrderedDict())
    node[unit] = value


def _gold_cost_to_maintenance(fams):
    """gold UPKEEP -> maintenance (owner ruling 2026-06-20; DEC-maintenance-bookkeeping). A building's UNCONDITIONAL
    negative gold-commerce is its gold COST: legacy charges it to MAINTENANCE (TREAT_NEGATIVE_GOLD_AS_MAINTENANCE /
    calculateBuildingMaintenanceTimes100), NEVER to gold commerce. So move that negative out of the `gold` family into
    the `maintenance` family as a flat cost (a POSITIVE amount) — maintenance is a separate bookkeeping channel,
    OUTSIDE the commerce chain. Positive gold stays as commerce; CONDITIONAL gold (dict deposits, e.g. age-gated
    CommerceChangeDoubleTimes) is left in place — legacy's maintenance gate reads the static base getBuildingCommerce,
    not conditionals."""
    gold = fams.get("gold")
    if not isinstance(gold, dict):
        return
    city = gold.get("city")
    if not isinstance(city, dict) or "flat" not in city:
        return
    flat = city["flat"]
    cost = 0
    if isinstance(flat, (int, float)):
        if flat < 0:
            cost = -flat
            del city["flat"]
    elif isinstance(flat, list):
        cost = sum(-it for it in flat if isinstance(it, (int, float)) and it < 0)
        if cost:
            keep = [it for it in flat if not (isinstance(it, (int, float)) and it < 0)]
            if keep:
                city["flat"] = keep[0] if len(keep) == 1 else keep
            else:
                del city["flat"]
    if not cost:
        return
    if not city:
        del gold["city"]
    if not gold:
        del fams["gold"]
    _set_fam(fams, "maintenance", "city", None, "flat", cost)


def curate(typ, rec, store):
    out = OrderedDict([("type", typ)])
    for tag, key in TEXT.items():
        t = _txt(rec, tag)
        if t:
            out[key] = t

    fams = OrderedDict()
    identity = OrderedDict()
    cost = OrderedDict()
    art_blocks = OrderedDict()
    ai = OrderedDict()

    # --- scalar/percent families ---
    for tag, (family, scope, member, unit) in SCALAR_FAMILIES.items():
        v = _int(rec, tag)
        if v:
            _set_fam(fams, family, scope, member, unit, v)
    # --- yield/commerce split families ---
    for tag, spec in YIELD_FAMILIES.items():
        node = rec.find(tag)
        if node is None:
            continue
        scope, keys, unit = spec[0], spec[1], spec[2]
        submember = spec[3] if len(spec) > 3 else None     # optional sub-scope (e.g. 'specialist' for perSpecialist)
        for member, v in engine.named_array(node, keys).items():
            # per-pop (Yield/CommercePerPopChanges) is x100-scaled in XML (100 = 1/pop) -> de-scale to human (1),
            # so the JSON carries human per-pop numbers (cascade-fixed-point: curator emits human, readJson re-x100s).
            if unit == "perPopulation":
                v = descale100(v)
            _set_fam(fams, member, scope, submember, unit, v)   # member IS the family (split)

    # --- enables / obsoletes / replaces (store-derived; COPIED so pass2 can extend FoundsCorporation/ObsoletesToBuilding) ---
    enables = OrderedDict((k, list(v)) for k, v in (store.enabled_by(typ) or {}).items())
    # supersession, TARGET-side (owner ruling 2026-06-22): authored on THIS (the superseded) building, read off
    # its OWN fields, no store inversion. obsoletedBy = obsoleting tech (ObsoleteTech) + superseding building
    # (ObsoletesToBuilding). NB `ReplacementBuildings` is NOT authored as `replacedBy` here -- it is reversible
    # DORMANCY in the engine (setDisabledBuilding, not removal), so it lands on `requires.operate.dormant` (above);
    # the `replaces` enabler edge stays a defined concept but is now UNUSED. The cascade builds the obsoletion
    # reverse map (superseder -> [superseded]) at load — never stored in the JSON.
    obsoleted_by = OrderedDict()
    _ot = _txt(rec, "ObsoleteTech")
    if _ot:
        obsoleted_by["techs"] = [_ot]
    _otb = _txt(rec, "ObsoletesToBuilding")
    if _otb:
        obsoleted_by["buildings"] = [_otb]
    # SpecialBuilding GROUP obsoletion inherited onto the MEMBER, target-side (owner 2026-06-22: put the group's
    # ObsoleteTech on the monastery itself; double-representation with the store's source-side _inherit_group_obsoletes
    # is fine). The target-side cascade reads the member's obsoletedBy, not the tech's obsoletes, so author it here —
    # this is exactly the edge my target-side migration orphaned when it stopped reading store.obsoletes_of(typ).
    _sbt = _txt(rec, "SpecialBuildingType")
    if _sbt:
        _grp = store.table("SpecialBuildingInfo").get(_sbt)
        _grp_ot = _txt(_grp, "ObsoleteTech") if _grp is not None else None
        if _grp_ot and _grp_ot != "NONE":
            obsoleted_by.setdefault("techs", []).append(_grp_ot)

    grants = OrderedDict()
    repeatable = []
    # provides: bonuses this building SUPPLIES while active (XML ExtraFreeBonuses) -- a vicinity-bonus source,
    # uniform with a map bonus that provides itself. The cascade's vicinity check unions plot bonuses + active
    # buildings' provides.bonuses.
    provides = OrderedDict()
    _pb = _extra_free_bonuses(rec)
    if _pb:
        provides["bonuses"] = _pb
    requires = requires_building(rec, store)
    allowed = allowed_building(rec)
    loadprune = loadprune_building(rec)

    # --- PASS 2: keyed inversions (§6.1), properties, repeatable grants, one-shot grants, enables-from-XML ---
    pass2(typ, rec, store, fams, grants, repeatable, identity, enables)
    if repeatable:
        grants["repeatable"] = repeatable

    # gold UPKEEP -> maintenance (DEC-maintenance-bookkeeping): a building's unconditional negative gold-commerce is
    # its gold COST, which legacy charges to MAINTENANCE (TREAT_NEGATIVE_GOLD_AS_MAINTENANCE), not gold commerce.
    _gold_cost_to_maintenance(fams)

    # --- cost ---
    for tag, key in COST.items():
        v = _int(rec, tag)
        if v is not None and v != -1 and v != 0:
            cost[key] = v

    # legacy iCost == -1 (or absent -> the XML read default is -1, CvBuildingInfo.cpp:1764) = NOT player-constructible
    # (the CvPlayer::canConstruct getProductionCost()==-1 gate, CvPlayer.cpp:6667). Such buildings are instantiated by
    # OTHER systems -- autobuild-on-condition, property spawn, outcome missions, GP/event relics, doctrine toggles --
    # never built via the city production queue. Translate the dumb sentinel into an explicit clean flag (the building
    # twin of the unit identity.spawnOnly); the cascade gates buildability on this, never on a raw -1 cost. NB this is
    # the buildability GATE only -- it is distinct from identity.autoBuild (a PLACEMENT behavior: "auto-place me where
    # my requires holds"), which overlaps but is not the same.
    if _int(rec, "iCost") in (None, -1):
        identity["notConstructible"] = True

    # --- capabilities -> identity ---
    for tag, name in CAP_IDENTITY.items():
        if _bool(rec, tag):
            identity[name] = True
    # --- identity scalars / lists ---
    for tag, key in ID_SCALAR.items():
        iv = _int(rec, tag)
        sv = _txt(rec, tag)
        if iv is not None and iv != 0 and iv != -1:
            identity[key] = iv
        elif sv and not engine.is_int(sv):
            identity[key] = sv
    for tag, key in ID_LIST.items():
        lst = _typelist(rec, tag) if rec.find(tag) is not None and not _is_struct(rec.find(tag)) else _generic_list(rec, tag)
        if lst:
            identity[key] = lst

    # --- ai (Flavors + iAIWeight) ---
    fl = rec.find("Flavors")
    if fl is not None:
        g = engine.generic(fl)
        if g:
            ai["flavours"] = g
    w = _int(rec, "iAIWeight")
    if w:
        ai.setdefault("behaviour", OrderedDict())["weight"] = w

    # --- art ---
    for tag in ART:
        put_art(art_blocks, tag, engine.text(rec.find(tag)))

    # --- assemble (reserved order, modifier-spec §1.1) ---
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    if obsoleted_by:
        out["obsoletedBy"] = OrderedDict((k, obsoleted_by[k]) for k in sorted(obsoleted_by))
    if requires:
        out["requires"] = requires
    if allowed:
        out["allowed"] = allowed
    if provides:
        out["provides"] = provides
    ordered = [f for f in FAMILY_ORDER if f in fams] + [f for f in fams if f not in FAMILY_ORDER]
    for f in ordered:
        out[f] = fams[f]
    if grants:
        out["grants"] = grants
    if cost:
        out["cost"] = cost
    if ai:
        out["ai"] = ai
    if loadprune:
        out["loadPrune"] = loadprune
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out


def curate_special(typ, rec, store):
    """SpecialBuilding (#31) — a per-player-capped building GROUP (getMaxPlayerInstances, enforced by
    isBuildingGroupMaxedOut/getBuildingGroupCount). Buildings join it via their SpecialBuildingType FK
    (identity.specialBuildingType). TechPrereq/TechPrereqAnyone -> store (tech.enables.specialBuildings), dropped here."""
    out = OrderedDict([("type", typ)])
    d = _txt(rec, "Description")
    if d:
        out["description"] = d
    enables = store.enabled_by(typ)
    if enables:
        out["enables"] = OrderedDict((k, enables[k]) for k in sorted(enables))
    obsoletes = store.obsoletes_of(typ)
    if obsoletes:
        out["obsoletes"] = OrderedDict((k, obsoletes[k]) for k in sorted(obsoletes))
    cap = _int(rec, "iMaxPlayerInstances")
    if cap is not None and cap >= 0:
        out["allowed"] = OrderedDict([("empire", cap)])   # the per-player GROUP cap -> unified `allowed` idiom
    identity = OrderedDict()
    if engine.text(rec.find("bValid")) in ("0", "false", "False"):
        identity["valid"] = False                      # bValid=0 -> the group is disabled (rare)
    art_blocks = OrderedDict()
    put_art(art_blocks, "Button", engine.text(rec.find("Button")))
    emit_art(out, art_blocks)
    if identity:
        out["identity"] = identity
    return out


def _is_struct(node):
    return node is not None and len(node) > 0 and len(list(node)[0]) > 0


def _generic_list(rec, tag):
    node = rec.find(tag)
    if node is None:
        return []
    g = engine.generic(node)
    return g if isinstance(g, list) else ([g] if g else [])


HANDLED = (set(SCALAR_FAMILIES) | set(YIELD_FAMILIES) | set(CAP_IDENTITY) | set(ID_SCALAR) | set(ID_LIST)
           | set(COST) | set(TEXT) | set(ART) | REQUIRES_TAGS | STORE_TAGS | DROP_DEAD | DROP_MODULE | PASS2_TAGS
           | {"Type", "Flavors", "iAIWeight"})


# ============================ PROPERTY-BAND REALIGNMENT (owner 2026-06-23) ============================
# Property-effect "pseudobuildings" (the BuildingType under each PROPERTY's <PropertyBuildings>) are placed/removed
# by the property-band system (checkPropertyBuildings) on an [iMinValue, iMaxValue] band (data-model §2.1, enabler-
# spec §3). apply_property_bands authors a `requires.operate` PROPERTY-in-band atom {type:PROPERTY_X, scope:city,
# min, max} on each pseudobuilding -- the band is ACTIVE iff the city's property value is in [min, max]. Crime/
# disease/tourism/pollution bands carry no ReplacementBuildings, so every in-band band stays active (they COMPOUND).
#
# EDUCATION's 4 ladders (positive/negative era, argumentative-awareness, blissful-ignorance) ARE succession chains
# (legacy ReplacementBuildings) -- and that is now handled UNIFORMLY, with NO special case. ReplacementBuildings is
# reversible DORMANCY in the engine (setDisabledBuilding, CvCity.cpp:14413 -- NOT removal), so requires_building
# mirrors it as `requires.operate.dormant: successor`: a lower band dorms while a higher band is present => only the
# highest reached band is active, FULL per-band value, == the LEGACY engine (parity). The same mirror covers the
# pseudo->REAL case (BLACKENED_SKIES dorms the telescope/observatory; it is not nuked from orbit).
#
# ⛔ The earlier (2026-06-19/22) "pseudobuildings COMPOUND, NEVER replace -- strip replaces + increment-convert
# education + a banded only-highest disable" model is OBSOLETE (owner 2026-06-23): it rollerskated away, and its
# strip/ladder code was reading a `replaces` key the curator never emitted (dead). The uniform ReplacementBuildings
# -> requires.operate.dormant mirror replaces it and RESTORES education parity (the old ~1,100-building BY-DESIGN
# divergence is gone). Behavioural redesign (should education be only-highest at all?) is POST-migration, never here.

PROPERTY_INFOS_XML = os.path.join(REPO, "Assets", "XML", "GameInfo", "CIV4PropertyInfos.xml")
# top-level reserved (non-family) keys -- everything else on a band object is a modifier family to increment.
RESERVED_NONFAMILY = {"type", "description", "civilopedia", "help", "enables", "obsoletedBy", "replacedBy", "requires",
                      "allowed", "provides", "grants", "cost", "ai", "loadPrune", "ui", "world", "sound", "identity"}


def property_band_buildings():
    """The set of BuildingType in any PROPERTY's <PropertyBuildings> -- the property-effect pseudobuildings."""
    pseudo = set()
    root = ET.parse(PROPERTY_INFOS_XML).getroot()
    for node in root.iter():
        if node.tag.split("}")[-1] != "PropertyBuilding":
            continue
        for ch in node:
            if ch.tag.split("}")[-1] == "BuildingType" and ch.text:
                pseudo.add(ch.text.strip())
    return pseudo


def property_band_atoms():
    """Map each property-effect pseudobuilding -> its band atom (PROPERTY_X, iMinValue, iMaxValue) from the XML
    <PropertyBuilding>. The atom makes the band CUMULATIVELY active when the city's property value is in the band
    (owner 2026-06-19: pseudobuildings compound, set up by `requires`, never `replace`)."""
    atoms = {}
    root = ET.parse(PROPERTY_INFOS_XML).getroot()
    bare = lambda n: n.tag.split("}")[-1]
    for pi in root.iter():
        if bare(pi) != "PropertyInfo":
            continue
        ptype = next((ch.text.strip() for ch in pi if bare(ch) == "Type" and ch.text), None)
        if not ptype:
            continue
        for pbs in pi:
            if bare(pbs) != "PropertyBuildings":
                continue
            for pb in pbs:
                if bare(pb) != "PropertyBuilding":
                    continue
                d = {bare(x): (x.text.strip() if x.text else "") for x in pb}
                bt = d.get("BuildingType")
                if not bt:
                    continue
                lo = int(d["iMinValue"]) if d.get("iMinValue") not in (None, "") else None
                hi = int(d["iMaxValue"]) if d.get("iMaxValue") not in (None, "") else None
                atoms[bt] = (ptype, lo, hi)
    return atoms


def _set_requires(obj, req):
    """Place/replace `requires` in its canonical slot (after enables/obsoletes/replaces/disables, before families)."""
    LEADING = ("type", "description", "civilopedia", "help", "enables", "obsoletedBy", "replacedBy", "disables")
    out = OrderedDict()
    inserted = False
    for k, v in obj.items():
        if k == "requires":
            continue
        if not inserted and k not in LEADING:
            out["requires"] = req
            inserted = True
        out[k] = v
    if not inserted:
        out["requires"] = req
    return out


def apply_property_bands(results, pseudo):
    """Author the requires.operate PROPERTY-in-band atom on every property-effect pseudobuilding -- the band is
    ACTIVE iff the city's property value is in [iMinValue, iMaxValue] (data-model §2.1, enabler-spec §3). The
    only-highest education ladders + the pseudo->REAL disable (blackened-skies dorms the observatory) are NOT
    special-cased here: their legacy `ReplacementBuildings` is reversible DORMANCY (engine `setDisabledBuilding`,
    CvCity.cpp:14413 -- never removal), already mirrored as `requires.operate.dormant` by requires_building
    (owner 2026-06-23, engine-verified; the prior 2026-06-19/22 'pseudobuildings compound, never replace' ruling
    is OBSOLETE). Returns n_banded; mutates results."""
    band_atoms = property_band_atoms()   # building -> (ptype, iMinValue, iMaxValue)
    # Author the requires.operate PROPERTY-in-band atom on every pseudobuilding -- ACTIVE iff the city's property
    # value is in [iMinValue, iMaxValue]. Merged into requires.operate.all, preserving any requires.build tech gate
    # + the requires.operate.dormant emitted by requires_building (the only-highest / pseudo->real dormancy mirror).
    banded = 0
    for b, (ptype, lo, hi) in band_atoms.items():
        obj = results.get(b)
        if not obj:
            continue
        atom = OrderedDict([("type", ptype), ("scope", "city")])
        if lo is not None:
            atom["min"] = lo
        if hi is not None:
            atom["max"] = hi
        req = obj.get("requires")
        req = OrderedDict(req) if isinstance(req, dict) else OrderedDict()
        op = OrderedDict(req.get("operate")) if isinstance(req.get("operate"), dict) else OrderedDict()
        allc = list(op.get("all")) if isinstance(op.get("all"), list) else []
        allc.append(atom)
        op["all"] = allc
        req["operate"] = op
        results[b] = _set_requires(obj, req)
        banded += 1
    return banded


def build_era(store):
    """tech Type -> era short name (from the tech XML <Era>)."""
    cache = {}
    for ttyp, trec in store.table("TechInfo").items():
        e = engine.text(trec.find("Era"))
        if e:
            cache[ttyp] = e.replace("C2C_ERA_", "").replace("ERA_", "").lower()
    return cache


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sample", nargs="*")
    ap.add_argument("--write", action="store_true")
    args = ap.parse_args()
    store = Store()
    table = store.table("BuildingInfo")
    era_map = build_era(store)
    results = OrderedDict()
    era_of = {}
    for typ, rec in table.items():
        results[typ] = curate(typ, rec, store)
        pt = _txt(rec, "PrereqTech")
        era_of[typ] = era_map.get(pt, "none")
    n = len(results)
    # SpecialBuilding #31 rides this pass (the per-player-capped GROUP).
    sb_results = OrderedDict((typ, curate_special(typ, rec, store))
                             for typ, rec in store.table("SpecialBuildingInfo").items())

    # PROPERTY-BAND realignment (owner 2026-06-23): author the requires.operate PROPERTY-in-band atom on every
    # pseudobuilding. The only-highest education ladders + the pseudo->REAL dormancy (blackened-skies) are NOT
    # special-cased here -- they ride the uniform `ReplacementBuildings -> requires.operate.dormant` mirror.
    pseudo = property_band_buildings()
    n_band = apply_property_bands(results, pseudo)
    print("PROPERTY BANDS: %d pseudobuildings | %d got requires.operate band atom" % (len(pseudo), n_band))

    from collections import Counter
    leftover = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag not in HANDLED:
                leftover[c.tag] += 1
    pass2 = Counter()
    for _typ, rec in table.items():
        for c in rec:
            if c.tag in PASS2_TAGS:
                pass2[c.tag] += 1
    if leftover:
        print("UNHANDLED tags (count): %s" % ", ".join("%s=%d" % (t, c) for t, c in leftover.most_common()))
    else:
        print("COVERAGE: all XML tags handled or deferred (pass 2).")
    print("PASS-2 deferred tags present: %d distinct" % len(pass2))

    has = lambda k: sum(1 for o in results.values() if k in o)
    STRUCT = {"type", "description", "civilopedia", "help", "enables", "obsoletes", "replaces", "requires",
              "allowed", "provides", "cost", "ai", "loadPrune", "ui", "world", "sound", "identity"}
    seen = sorted({f for o in results.values() for f in o if f not in STRUCT})
    print("BuildingInfo curated: %d" % n)
    for k in ("enables", "obsoletes", "replaces", "requires", "allowed", "cost", "ai", "loadPrune", "identity"):
        print("  with %-9s: %d" % (k, has(k)))
    print("  families seen: %s" % ", ".join(seen))
    print("SpecialBuildingInfo curated: %d  (with cap: %d)"
          % (len(sb_results), sum(1 for o in sb_results.values() if o.get("allowed") is not None)))
    if args.sample is not None:
        for nm in (args.sample or list(results)[:1]):
            src = results if nm in results else (sb_results if nm in sb_results else None)
            if src is not None:
                print("\n=== %s (era=%s) ===" % (nm, era_of.get(nm, "-")))
                print(json.dumps(src[nm], indent=1, ensure_ascii=False))
            else:
                print("\n(%s not found)" % nm)
    if args.write:
        base = os.path.join(REPO, "Assets", "Data", "buildings")
        for typ, obj in results.items():
            folder = os.path.join(base, era_of.get(typ, "none"))
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        sbdir = os.path.join(REPO, "Assets", "Data", "specialbuildings")
        os.makedirs(sbdir, exist_ok=True)
        for typ, obj in sb_results.items():
            with open(os.path.join(sbdir, typ.lower() + ".json"), "w") as fp:
                json.dump(obj, fp, indent=1, ensure_ascii=False)
        print("\nwrote %d BuildingInfo + %d SpecialBuildingInfo JSON files under Assets/Data" % (n, len(sb_results)))


if __name__ == "__main__":
    main()
